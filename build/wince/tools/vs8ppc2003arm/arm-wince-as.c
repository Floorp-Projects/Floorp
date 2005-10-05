#include "toolpaths.h"

int 
main(int argc, char **argv)
{  
  char* args[1000];
  int i = 0;
  
  args[i++] = ASM_PATH;
  i += argpath_conv(&argv[1], &args[i]);

  run(args);

  return 0;
}
