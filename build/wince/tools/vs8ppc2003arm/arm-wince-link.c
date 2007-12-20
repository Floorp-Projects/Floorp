#include "toolpaths.h"

int 
main(int argc, char **argv)
{
  int iRetVal;
  char* args[1000];
  int i = 0;
  int j = 0;
  int k = 0;

  args[i++] = LINK_PATH;

  args[i++] = "/LIBPATH:\"" WCE_LIB "\"";
  args[i++] = "/LIBPATH:\"" SHUNT_LIB "\"";

  args[i++] = "winsock.lib";
  args[i++] = "corelibc.lib";
  args[i++] = "coredll.lib";
  args[i++] = "ceshell.lib";
  args[i++] = "ole32.lib";
  args[i++] = "mmtimer.lib";
  args[i++] = "shunt.lib";

  args[i++] = "/subsystem:\"WINDOWSCE,5.01\"";
  args[i++] = "/MACHINE:THUMB";

  args[i++] = "-OPT:REF";
  args[i++] = "-OPT:ICF";

  args[i++] = "/NODEFAULTLIB:LIBC";
  args[i++] = "/NODEFAULTLIB:OLDNAMES";
  args[i++] = "/NODEFAULTLIB:LIBCMT";
  args[i++] = "/NODEFAULTLIB:LIBCMTD";

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

  dumpargs(args);

  return run(args);
}
