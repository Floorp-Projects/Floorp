#include "OSObject.cpp"
#include "jsoptparse.cpp"
#define main shell_main
#include "js.cpp"
#undef main

#include <unistd.h>

extern "C" bool GetDocumentsDirectory(char *dir);

// Fake editline
char* readline(const char* prompt)
{
  return nullptr;
}

void add_history(char* line) {}

int main(int argc, char** argv, char** envp)
{
    char dir[1024];
    GetDocumentsDirectory(dir);
    chdir(dir);
    return shell_main(argc, argv, envp);
}
