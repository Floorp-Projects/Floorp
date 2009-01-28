#include "toolspath.h"

int
main(int argc, char **argv)
{
  int iRetVal;
  char* args[1000];
  int i = 0;
  int j = 0;
  int k = 0;
  int s = 0;
  args[i++] = RC_PATH;
  args[i++] = "/I\"" WCE_RC_INC "\"";
  args[i++] = "/I\"" WM_SDK_INC  "\"";

  argpath_conv(&argv[1], &args[i]);

  //  dumpargs(args);

  return run(args);
}
