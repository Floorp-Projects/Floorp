#include <stdlib.h>
#include <stdio.h>
#include <process.h>

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

  args[i++] = "link.exe";
  args[i++] = "/SUBSYSTEM:WINDOWSCE,4.20";
  args[i++] = "/MACHINE:ARM";
  args[i++] = "/LIBPATH:\"" WCE_LIB "\"";
  args[i++] = "/LIBPATH:\"" SHUNT_LIB "\"";

  args[i++] = "winsock.lib";
  args[i++] = "corelibc.lib";
  args[i++] = "coredll.lib";
  args[i++] = "ceshell.lib";

  args[i++] = "shunt.lib";

  args[i++] = "/NODEFAULTLIB:LIBC";
  args[i++] = "/NODEFAULTLIB:OLDNAMES";

  //  args[i++] = "/STACK:0x5000000,0x100000";

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

  //  dumpargs(args);

  iRetVal = _spawnv( _P_WAIT, LINK_PATH, args );

  if (iRetVal == -1)
  {
    printf("-----------------> %d <----------------------\n\n\n\n", errno);
  }
  return 0;
}
