#include "toolpaths.h"

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

  args[i++] = "/DARM";
  args[i++] = "/DWINCE";
  args[i++] = "/D_WIN32_WCE=420";
  args[i++] = "/DUNDER_CE=420";
  args[i++] = "/DWIN32_PLATFORM_PSPC=400";
  args[i++] = "/D_ARM_";
  args[i++] = "/DDEPRECATE_SUPPORTED";
  args[i++] = "/DSTDC_HEADERS";

  args[i++] = "/Gy";  // For link warning LNK1166

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
	    if ( strstr(args[startOfArgvs+j], ".obj") )
	    {
		    // If we are outputting a .OBJ file, then we are
		    // NOT linking, and we need to do some fancy
		    // footwork to output "/FoFILENAME" as an argument
		    args[startOfArgvs+j-1] = "";
		    strcpy(outputFileArg, "/Fo");
		    strcat(outputFileArg, args[startOfArgvs+j]);
		    args[startOfArgvs+j] = outputFileArg;
	    } else
	    {
            argv[j] = "";
		    // Otherwise, we are linking as usual
		    link = 1;
	    }
    }
    j++;
  }

  if (link)
  {
    args[i++] = "/link";

    args[i++] = "-ENTRY:mainACRTStartup";
    args[i++] = "-SUBSYSTEM:WINDOWSCE,4.20";
    args[i++] = "-MACHINE:ARM";
    args[i++] = "-LIBPATH:\"" WCE_LIB "\"";
    args[i++] = "-LIBPATH:\"" SHUNT_LIB "\"";
    args[i++] = "mozce_shunt.lib";
    args[i++] = "winsock.lib"; 
    args[i++] = "corelibc.lib";
    args[i++] = "coredll.lib";
  }
  args[i] = NULL;

  run(args);

  return 0;
}
