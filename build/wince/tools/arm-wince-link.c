#include "toolspath.h"
#include "linkargs.h"

int
main(int argc, char **argv)
{
  int iRetVal;
  char* args[1000];
  int i = 0;
  int j = 0;
  int k = 0;
  int s = 0;

  // if -DLL is not passed, then change the entry to 'main'
  while(argv[j]) {
    checkLinkArgs(&k, &s, &i, &j, args, argv);
    j++;
  }
  args[i++] = LINK_PATH;
  addLinkArgs(k, s, &i, &j, args, argv);

  argpath_conv(&argv[1], &args[i]);

  return run(args);
}

