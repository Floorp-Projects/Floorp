#ifndef TOOLPATH_H
#define TOOLPATH_H

#include <windows.h>
#include <stdio.h>
#include <process.h>

#define OGLES_SDK_INC  OGLES_SDK_PATH "inc"
#define OGLES_SDK_LIB  OGLES_SDK_PATH "lib\\wince\\nvap\\release"
#define WCE_BIN    VC_PATH "ce\\bin\\x86_arm\\"
#define WCE_RC_BIN WIN_SDK_PATH  "bin\\"
#define WCE_CRT    VC_PATH "ce\\lib\\armv4i"
#define WM_SDK_INC    WM_SDK_PATH "Include/Armv4i"
#define WCE_LIB    WM_SDK_PATH "Lib/Armv4i"
#define WCE_RC_INC  VC_PATH "ce\\atlmfc\\include"
#define WCE_INC  VC_PATH "ce\\include"
#define ATL_INC  VC_PATH "ce\\atlmfc\\include"
#define ATL_LIB  VC_PATH "ce\\atlmfc\\lib\\armv4i"

#ifndef SHUNT_LIB
#define SHUNT_LIB ""
#endif

#ifndef SHUNT_INC
#define SHUNT_INC TOPSRCDIR "/build/wince/shunt/include/"
#endif

#define ASM_PATH  "\"" WCE_BIN "armasm.exe\""
#define CL_PATH   "\"" WCE_BIN "cl.exe\""
#define LIB_PATH  "\"" WCE_BIN "lib.exe\""
#define LINK_PATH "\"" WCE_BIN "link.exe\""
#define RC_PATH   "\"" WCE_RC_BIN "rc.exe\""

#define MAX_NOLEAK_BUFFERS 1000
char noleak_buffers[MAX_NOLEAK_BUFFERS][1024];
static int next_buffer = 0;
int argpath_conv(char **args_in, char **args_out);
void dumpargs(char** args);
DWORD run(char** args);

#endif
