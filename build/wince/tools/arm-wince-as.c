#include "toolspath.h"

int 
main(int argc, char **argv)
{  
  char* args[1000];
  int i = 0;
  
  args[i++] = ASM_PATH;
  args[i++] = "-I\"" WCE_INC "\""; 
  args[i++] = "-CPU ARM1136";  // we target ARM 11 and newer 

  i += argpath_conv(&argv[1], &args[i]);

  return run(args);
}
