#include <stdlib.h>
#include <stdio.h>
#include <process.h>
#include "toolpaths.h"

int 
main(int argc, char **argv)
{  
  char* args[1000];

  argpath_conv(argv, args);

  return _spawnv( _P_WAIT, ASM_PATH, args );
}
