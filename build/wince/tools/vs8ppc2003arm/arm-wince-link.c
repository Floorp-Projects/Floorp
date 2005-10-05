#include "toolpaths.h"

int 
main(int argc, char **argv)
{
  int iRetVal;
  char* args[1000];
  int i = 0;
  int j = 0;
  int k = 0;

  // Clear any link env variable that might get us tangled up
  _putenv("LINK=");

  args[i++] = LINK_PATH;
  args[i++] = "/SUBSYSTEM:WINDOWSCE,4.20";
  args[i++] = "/MACHINE:ARM";
  args[i++] = "/LIBPATH:\"" WCE_LIB "\"";
  args[i++] = "/LIBPATH:\"" SHUNT_LIB "\"";

  args[i++] = "winsock.lib";
  args[i++] = "corelibc.lib";
  args[i++] = "coredll.lib";
  args[i++] = "ceshell.lib";

  args[i++] = "mozce_shunt.lib";

  args[i++] = "/NODEFAULTLIB:LIBC";
  args[i++] = "/NODEFAULTLIB:OLDNAMES";

  // if -DLL is not passed, then change the entry to 'main'
  while(argv[j])
  {
    if (strncmp(argv[j], "-DLL", 4) == 0 || strncmp(argv[j], "/DLL", 4) == 0)
    {
      k = 1;
      break;
    }
    j++;
  }
  
  if (k==0)
    args[i++] = "/ENTRY:mainACRTStartup";

  argpath_conv(&argv[1], &args[i]);

  // dumpargs(args);

  run(args);
  return 0;
}
