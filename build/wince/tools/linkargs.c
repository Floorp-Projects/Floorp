#include "toolspath.h"
#include "linkargs.h"

void checkLinkArgs(int* k, int* s, int* i, int* j, char** args, char** argv) {
      if (strncmp(argv[*j], "-DLL", 4) == 0 ||
	  strncmp(argv[*j], "/DLL", 4) == 0) {
	*k = 1;
      }
      if (strncmp(argv[*j], "-entry", 6) == 0 ||
	  strncmp(argv[*j], "/entry", 6) == 0 ||
	  strncmp(argv[*j], "-ENTRY", 6) == 0 ||
	  strncmp(argv[*j], "/ENTRY",6 ) == 0) {
	*k = 1;
      }
      if (strncmp(argv[*j], "-subsystem:", 11) == 0 ||
	  strncmp(argv[*j], "/subsystem:", 11) == 0 ||
	  strncmp(argv[*j], "-SUBSYSTEM:", 11) == 0 ||
	  strncmp(argv[*j], "/SUBSYSTEM:", 11) == 0) {
	*s = 1;
      }
}

void addLinkArgs(int k, int s, int *i, int *j, char** args, char** argv) {
  args[(*i)++] = "/LIBPATH:\"" WCE_LIB "\"";
  args[(*i)++] = "/LIBPATH:\"" WCE_CRT "\"";
  args[(*i)++] = "/LIBPATH:\"" ATL_LIB "\"";
  args[(*i)++] = "/LIBPATH:\"" OGLES_SDK_LIB "\"";
  args[(*i)++] = "/NODEFAULTLIB";

  args[(*i)++] = "/MAP";
  args[(*i)++] = "/MAPINFO:EXPORTS";

  if (getenv("LOCK_DLLS") != NULL) {
    // lock our dlls in memory
    args[(*i)++] = "/SECTION:.text,\!P";
    args[(*i)++] = "/SECTION:.rdata,\!P";
  }

#ifdef HAVE_SHUNT   // simple test to see if we're in configure or not
  if(getenv("NO_SHUNT") == NULL) {
    args[(*i)++] = "/LIBPATH:\"" SHUNT_LIB "\"";
    args[(*i)++] = "mozce_shunt.lib";
  }
#endif

  args[(*i)++] = "corelibc.lib";
  args[(*i)++] = "coredll.lib";
  args[(*i)++] = "ceshell.lib";
  args[(*i)++] = "mmtimer.lib";

  if (k==0)
    args[(*i)++] = "/ENTRY:mainACRTStartup";

  if (s==0){
    args[(*i)++] = "/subsystem:\"WINDOWSCE,5.02\"";
  }
}
