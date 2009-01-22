#include "toolspath.h"

int 
main(int argc, char **argv)
{  
  char* args[1000];
  int i = 0;
  
  args[i++] = ASM_PATH;
  args[i++] = "-I\"" WCE_INC "\""; 

  i += argpath_conv(&argv[1], &args[i]);

  dumpargs(args);

  return run(args);
}
