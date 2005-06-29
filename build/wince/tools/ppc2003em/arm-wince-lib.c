#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include "toolpaths.h"


int 
main(int argc, char **argv)
{
  char* args[1000];
  int i = 0;

  args[i++] = "lib.exe";
  args[i++] = "/SUBSYSTEM:WINDOWSCE,4.20";
  args[i++] = "/MACHINE:X86";

  argpath_conv(&argv[1], &args[i]);

  return _spawnv( _P_WAIT, LIB_PATH, args );
}
