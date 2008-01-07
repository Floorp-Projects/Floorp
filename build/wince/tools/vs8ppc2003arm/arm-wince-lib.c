#include "toolspath.h"


int 
main(int argc, char **argv)
{
  char* args[1000];
  int i = 0;

  args[i++] = LIB_PATH;

  //  args[i++] = "/SUBSYSTEM:WINDOWSCE,4.20";
  //  args[i++] = "/MACHINE:ARM";

  argpath_conv(&argv[1], &args[i]);

  return run(args);

}
