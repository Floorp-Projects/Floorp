#include "toolspath.h"
#include "linkargs.h"

int
main(int argc, char **argv)
{
  int startOfArgvs;
  int i = 0;
  int j = 0;
  int link = 0;
  int s = 0;
  int k = 0;

  char* args[1000];
  char  outputFileArg[1000];

  args[i++] = CL_PATH;

#ifdef HAVE_SHUNT   // simple test to see if we're in configure or not
  if(!getenv("NO_SHUNT")) {
    args[i++] = "/I\"" SHUNT_INC "\"";
    args[i++] = "/FI\"mozce_shunt.h\"";
  }
#endif
#ifdef MOZ_MEMORY
  args[i++] = "/DMOZ_MEMORY";
#endif
  args[i++] = "/I\"" ATL_INC "\"";
  args[i++] = "/DMOZCE_STATIC_BUILD";
  args[i++] = "/DUNICODE";
  args[i++] = "/D_UNICODE_";
  args[i++] = "/DARM";
  args[i++] = "/D_ARM_";
  args[i++] = "/DWINCE";
  args[i++] = "/D_WIN32_WCE=0x502";
  args[i++] = "/DUNDER_CE";
//  args[i++] = "/DWIN32_PLATFORM_WFSP";
  args[i++] = "/DWIN32_PLATFORM_PSPC";
//  args[i++] = "/DPOCKETPC2003_UI_MODEL";
  args[i++] = "/D_WINDOWS";
  args[i++] = "/DNO_ERRNO";
  args[i++] = "/D_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA";

  args[i++] = "/GS-";                  // disable security checks
  args[i++] = "/GR-";                  // disable C++ RTTI

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
      if (strcmp(argv[j], "-link") ||
	  strcmp(argv[j], "-LINK") ||
	  strcmp(argv[j], "/link") ||
	  strcmp(argv[j], "/LINK")) 
	link = 1;
	  
      checkLinkArgs(&k, &s, &i, &j, args, argv);
      j++;
    }

  if (link)
    {
      args[i++] = "/link";
      addLinkArgs(k, s, &i, &j, args, argv);
    }

  args[i] = NULL;

  //  dumpargs(args);
  return run(args);
}
