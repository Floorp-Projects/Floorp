#include "toolspath.h"

int
main(int argc, char **argv)
{
  int startOfArgvs;
  int i = 0;
  int j = 0;
  int link = 0;

  char* args[1000];
  char  outputFileArg[1000];

  args[i++] = CL_PATH;
  args[i++] = "/I\"" WCE_INC "\"";
  args[i++] = "/I\"" SHUNT_INC "\"";
  args[i++] = "/FI\"mozce_shunt.h\"";

  args[i++] = "/DMOZCE_STATIC_BUILD";
  args[i++] = "/DUNICODE";
  args[i++] = "/D_UNICODE_";
  args[i++] = "/DARM";
  args[i++] = "/D_ARM_";
  args[i++] = "/DWINCE";
  args[i++] = "/D_WIN32_WCE=0x502";
  args[i++] = "/DUNDER_CE";
  args[i++] = "/DWIN32_PLATFORM_WFSP";
//  args[i++] = "/DWIN32_PLATFORM_PSPC";
//  args[i++] = "/DPOCKETPC2003_UI_MODEL";
  args[i++] = "/D_WINDOWS";
  args[i++] = "/DNO_ERRNO";

  args[i++] = "/Zc:wchar_t-";          //
  args[i++] = "/GS-";                  // disable security checks
  args[i++] = "/GR-";                  // disable C++ RTTI
  args[i++] = "/fp:fast";

  startOfArgvs = i;

  i += argpath_conv(&argv[1], &args[i]);

  // if /Fe is passed, then link
  //
  // if -o is passed, then blank out this argument, and place a "/Fo"
  // before the next argument
  while(argv[j])
    {
      if (strncmp(argv[j], "-o", 2) == 0)
	{
	  printf("%s is -o\n",argv[j]);


	  link = strstr(args[startOfArgvs+j], ".obj") ? 0:1;


	  // If we are outputting a .OBJ file, then we are
	  // NOT linking, and we need to do some fancy
	  // footwork to output "/FoFILENAME" as an argument
	  args[startOfArgvs+j-1] = "";
	  strcpy(outputFileArg, ( strstr(args[startOfArgvs+j], ".exe") )?"/Fe":"/Fo");
	  strcat(outputFileArg, args[startOfArgvs+j]);
	  args[startOfArgvs+j] = outputFileArg;
	}
      j++;
    }

  if (link)
    {
      args[i++] = "/link";

      args[i++] = "/ENTRY:main";

      args[i++] = "/SUBSYSTEM:WINDOWSCE,5.02";

      args[i++] = "/LIBPATH:\"" WCE_LIB "\"";
      args[i++] = "/LIBPATH:\"" WCE_CRT "\"";
      args[i++] = "/LIBPATH:\"" SHUNT_LIB "\"";
      args[i++] = "mozce_shunt.lib";
      args[i++] = "winsock.lib";
      args[i++] = "corelibc.lib";
      args[i++] = "coredll.lib";


      args[i++] = "/NODEFAULTLIB:LIBC";
      args[i++] = "/NODEFAULTLIB:OLDNAMES";

    }

  args[i] = NULL;

  dumpargs(args);
  return run(args);
}
