#include <iostream>
#include <fstream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vector>
#include <cstring>
#include <string>

#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    int in = dup(0);
    int out = dup(1);

    int in_fd = dup(STDIN_FILENO);
    int out_fd = dup(STDOUT_FILENO);

    int fd[2]; //file descriptor for piping

    char prevDir[1024];
    getcwd(prevDir, 1024);
    
    for (;;) {
        // need date/time, username, and absolute path to current dir
        time_t now = time(0);
        char* dnt = ctime(&now);
        char dow[4];
        char month[4];
        char day[4];
        char time_[10];
        sscanf(dnt, "%s %s %s %s", dow, month, day, time_);
        char dir[1024];

        bool inToFile = false;
        bool outToFile = false;

        char* username = getenv("USER");
        getcwd(dir, sizeof(dir));
        cout << YELLOW << month << " " << day << " " << time_ << " " << username << ":" << dir << "$" << NC << " ";
        // get user inputted command
        string input;
        getline(cin, input);

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

        //change directory
        if (tknr.commands.size() > 0 && tknr.commands[0]->args.size() > 0 && tknr.commands[0]->args[0] == "cd"){
            char temp[1024];
            if (tknr.commands[0]->args.size() > 1) {
                
                if (tknr.commands[0]->args[1] == "-"){
                    getcwd(temp, 1024);
                    chdir(prevDir);
                    strcpy(prevDir, temp);
                }
                else{
                    getcwd(temp, 1024);
                    strcpy(prevDir, temp);
                    chdir(tknr.commands[0]->args[1].c_str());
                }
            }
            else{
                //If no specified path, go home
                getcwd(temp, 1024);
                strcpy(temp, prevDir);
                chdir(getenv("HOME"));
            }
            continue;
        }
        
        
        for (size_t n=0; n < tknr.commands.size(); n++){
            if (!tknr.commands.at(n)) {
                // handle null command
                continue;
            }

            if (tknr.commands.at(n)->hasInput()){
                inToFile= true;
            }
            if (tknr.commands.at(n)->hasOutput()){
                outToFile= true;
            }
                           

            // // print out every command token-by-token on individual lines
            // // prints to cerr to avoid influencing autograder
            // for (auto cmd : tknr.commands) {
            //     for (auto str : cmd->args) {
            //         cerr << "|" << str << "| ";
            //     }
            //     if (cmd->hasInput()) {
            //         cerr << "in< " << cmd->in_file << " ";
            //     }
            //     if (cmd->hasOutput()) {
            //         cerr << "out> " << cmd->out_file << " ";
            //     }
            //     cerr << endl;
            // }

            // fork to create child

            if (pipe(fd) < 0) {
                perror("pipe"); //error check
                return 1;
            }

            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                if (n < tknr.commands.size() - 1){
                    dup2(fd[1], STDOUT_FILENO); 
                    close(fd[0]);
                }

                int numargs = tknr.commands.at(n)->args.size();
                char** args = new char*[numargs+1];
                for (int a=0; a<numargs; a++){
                    args[a]= new char[tknr.commands.at(n)->args.at(a).length()+1];
                    strcpy(args[a], tknr.commands.at(n)->args.at(a).c_str());
                }

                if (inToFile){
                    in_fd= open(tknr.commands.at(n)->in_file.c_str(), O_RDONLY);
                    dup2(in_fd, STDIN_FILENO);
                }

                if (outToFile){
                    out_fd= open(tknr.commands.at(n)->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    dup2(out_fd, STDOUT_FILENO);
                }

                args[numargs]= nullptr;

                if (execvp(args[0], args) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
                for (int a=0; a<numargs; a++){
                    delete[] args[a];
                }
                delete[] args;   
            }
            else {  // if parent, wait for child to finish
                dup2(fd[0], STDIN_FILENO); // In parent, redirect input to the read end of the pipe
                close(fd[1]);
                int status = 0;
                if (n == tknr.commands.size()-1)
                    waitpid(pid, &status, 0);
                if (status > 1) {  // exit if child didn't exec properly
                    exit(status);
                }
            } 
        }
        close(in_fd);
        close(out_fd);
        dup2(in,STDIN_FILENO);
        dup2(out,STDOUT_FILENO);
    }
}