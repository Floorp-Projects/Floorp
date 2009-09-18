#include "toolspath.h"

int 
main(int argc, char **argv)
{  
  char* args[1000];
  int i = 0;
  
  args[i++] = ASM_PATH;

  // armasm.exe requires a space between -I and the path. See bug 508721
  args[i++] = "-I \"" WCE_INC "\""; 

  i += argpath_conv(&argv[1], &args[i]);

  return run(args);
}
