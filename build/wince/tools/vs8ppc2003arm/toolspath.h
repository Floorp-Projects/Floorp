#include <windows.h>
#include <stdio.h>
#include <process.h>

#ifndef TOPSRCDIR
#include "../topsrcdir.h"
#endif

#define WCE_BIN   "c:/Program Files/Microsoft Visual Studio 8/VC/ce/bin/x86_arm/"
#define WCE_INC   "C:/Program Files/Windows Mobile 6 SDK/PocketPC/Include/Armv4i"
#define WCE_LIB   "C:/Program Files/Windows Mobile 6 SDK/PocketPC/Lib/Armv4i"

#define SHUNT_LIB TOPSRCDIR "/build/wince/shunt/build/vs8/"
#define SHUNT_INC TOPSRCDIR "/build/wince/shunt/include/"

#define ASM_PATH  WCE_BIN "armasm.exe"
#define CL_PATH   WCE_BIN "cl.exe"
#define LIB_PATH  WCE_BIN "lib.exe"
#define LINK_PATH WCE_BIN "link.exe"

#define MAX_NOLEAK_BUFFERS 100
char noleak_buffers[MAX_NOLEAK_BUFFERS][1024];
static int next_buffer = 0;

int argpath_conv(char **args_in, char **args_out)
{
 int i = 0;

 while (args_in[i])
 {
   args_out[i] = args_in[i];

   if (args_in[i])
   {
     char *offset = strstr(args_out[i], "/cygdrive/");

     if (offset) {

       strcpy(offset, offset+9);
       offset[0] = offset[1];
       offset[1] = ':';
       offset[2] = '/';
     }

     if ( (args_out[i][0] == '-' || args_out[i][0] == '/') &&
          (args_out[i][1] == 'D'))
     {

       offset = strstr(args_out[i]+2, "=");
       if (offset)
       {
         char* equalsChar = offset;

         if (equalsChar[1] == '"')
         {
           *equalsChar = '\0';

           strcpy(noleak_buffers[next_buffer], args_out[i]);        

           *equalsChar = '=';

           strcat(noleak_buffers[next_buffer], "=\\\"");
           strcat(noleak_buffers[next_buffer], equalsChar+1);
           strcat(noleak_buffers[next_buffer], "\\\"");

           args_out[i] = noleak_buffers[next_buffer];

           next_buffer++;

           if (next_buffer > MAX_NOLEAK_BUFFERS) {
             printf("next_buffer>MAX_NOLEAK_BUFFERS\n");
             exit(-1);
           }
         }
       }
     }
   }
   i++;
 }
 args_out[i] = NULL;
 return i;
}

void dumpargs(char** args)
{
 int i = 0;

 if (args[0] == NULL)
   printf(":: first element is null!\n");

 while(args[i])
   printf("%s ", args[i++]);

 printf("\n");
 fflush(stdout);
 fflush(stderr);
}


DWORD run(char** args)
{

 DWORD exitCode;
 STARTUPINFO si;
 PROCESS_INFORMATION pi;

 char theArgs[1024*16];

 int totalLen = 0;
 int i, j;


 // Clear any link env variable that might get us tangled up
 _putenv("LINK=");
 _putenv("LIBPATH=");
 _putenv("CC=");

 _putenv("INCLUDE=" WCE_INC);
 _putenv("LIB=" WCE_LIB);

 for (j=1; args[j]; j++)
 {
   int len = strlen(args[j]);
   strcat(&theArgs[totalLen], args[j]);
   totalLen += len;

   strcat(&theArgs[totalLen], " ");
   totalLen++;
 }

 i = strlen(args[0]);
 for (j=0; j<i; j++)
 {
   if (args[0][j] == '/')
     args[0][j] = '\\';
 }

 ZeroMemory( &si, sizeof(si) );
 si.cb = sizeof(si);
 ZeroMemory( &pi, sizeof(pi));

 CreateProcess(args[0],
               theArgs,
               NULL,
               NULL,
               0,
               0,
               NULL,
               NULL,
               &si,              // Pointer to STARTUPINFO structure.
               &pi);


 // Wait until child process exits.
 WaitForSingleObject( pi.hProcess, INFINITE );
 GetExitCodeProcess(pi.hProcess, &exitCode);
 // Close process and thread handles. 
 CloseHandle( pi.hProcess );
 CloseHandle( pi.hThread );

 return exitCode;
}
