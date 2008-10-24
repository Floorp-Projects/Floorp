#include <windows.h>
#include <stdio.h>
#include <process.h>

#ifndef TOPSRCDIR
#include "../topsrcdir.h"
#endif

#define WCE_BIN   "c:\\Program Files\\Microsoft Visual Studio 8\\VC\\ce\\bin\\x86_arm\\"
#define WCE_CRT   "c:\\Program Files\\Microsoft Visual Studio 8\\VC\\ce\\lib\\armv4i"
#define WCE_INC   "c:\\Program Files\\Windows Mobile 6 SDK\\PocketPC\\Include\\Armv4i"
#define WCE_LIB   "c:\\Program Files\\Windows Mobile 6 SDK\\PocketPC\\Lib\\Armv4i"

//#define WCE_RC_BIN "c:\\Program Files\\Microsoft Visual Studio 8\\VC\\bin\\"
#define WCE_RC_BIN "c:\\Program Files\\Microsoft SDKs\\Windows\\v6.0\\bin\\"

#define SHUNT_LIB TOPSRCDIR "/build/wince/shunt/build/vs8/"
#define SHUNT_INC TOPSRCDIR "/build/wince/shunt/include/"

#define ASM_PATH  WCE_BIN "armasm.exe"
#define CL_PATH   WCE_BIN "cl.exe"
#define LIB_PATH  WCE_BIN "lib.exe"
#define LINK_PATH WCE_BIN "link.exe"
#define RC_PATH   WCE_RC_BIN "rc.exe"

#define MAX_NOLEAK_BUFFERS 1000
char noleak_buffers[MAX_NOLEAK_BUFFERS][1024];
static int next_buffer = 0;

int argpath_conv(char **args_in, char **args_out)
{
  int i = 0;

  while (args_in[i])
  {
    char *offset;

    args_out[i] = args_in[i];

    if (args_in[i])
    {
      // First, look for the case of "-Fo/c/xxxxxxx" and "/Fo/c/xxxxx"
      if ( (args_out[i][0] == '-' || args_out[i][0] == '/') &&
           (args_out[i][1] == 'F') && (args_out[i][2] == 'o') &&
           (args_out[i][3] == '/') && (strlen(args_out[i]) > 5) ) {

        //printf("ARGS_IN: -FoXXXX is %s\n",args_in[i]);

        strcpy(noleak_buffers[next_buffer], args_in[i]);

        noleak_buffers[next_buffer][0] = '/';
        noleak_buffers[next_buffer][3] = noleak_buffers[next_buffer][4];
        noleak_buffers[next_buffer][4] = ':';

        args_out[i] = noleak_buffers[next_buffer];

        //printf("ARGS_OUT: -FoXXXX is %s\n",args_out[i]);

        next_buffer++;
      }
      else if ((args_out[i][0] == '/') && (args_out[i][2] == '/'))
      {
        // Assume this is a pathname, and adjust accordingly
        //printf("ARGS_IN: PATHNAME ASSUMED: %s\n", args_in[i]);

        strcpy(noleak_buffers[next_buffer], args_in[i]);

        noleak_buffers[next_buffer][0] = noleak_buffers[next_buffer][1];
        noleak_buffers[next_buffer][1] = ':';

        args_out[i] = noleak_buffers[next_buffer];
        //printf("ARGS_OUT: PATHNAME MODIFIED TO BE: %s\n", args_out[i]);

        next_buffer++;
      }
      else if ((args_out[i][0] == '\\') && (args_out[i][2] == '\\'))
      {
        // Assume this is a pathname, and adjust accordingly
        //printf("ARGS_IN: PATHNAME ASSUMED: %s\n", args_in[i]);

        strcpy(noleak_buffers[next_buffer], args_in[i]);

        noleak_buffers[next_buffer][0] = noleak_buffers[next_buffer][1];
        noleak_buffers[next_buffer][1] = ':';

        args_out[i] = noleak_buffers[next_buffer];
        //printf("ARGS_OUT: PATHNAME MODIFIED TO BE: %s\n", args_out[i]);

        next_buffer++;
      }
      else if ((args_out[i][0] == '\\') && (args_out[i][1] == '\\') &&
               (args_out[i][3] == '\\') && (args_out[i][4] == '\\'))
      {
        // Assume this is a pathname, and adjust accordingly
        //printf("ARGS_IN: PATHNAME ASSUMED: %s\n", args_in[i]);

        noleak_buffers[next_buffer][0] = args_in[i][2];
        noleak_buffers[next_buffer][1] = ':';
        noleak_buffers[next_buffer][2] = '\0';

        strcpy(noleak_buffers[next_buffer], &args_in[i][3]);

        args_out[i] = noleak_buffers[next_buffer];
        //printf("ARGS_OUT: PATHNAME MODIFIED TO BE: %s\n", args_out[i]);

        next_buffer++;
      }
      else if ( strstr(args_out[i], "OUT:") || strstr(args_out[i], "DEF:") )
      {
        // Deal with -OUT:/c/....
        //
        // NOTE: THERE IS A BUG IN THIS IMPLEMENTATION IF
        //       THERE IS A SPACE IN THE TOPSRCDIR PATH.
        //
        // Should really check for spaces, then double-quote
        // the path if any space is found.
        // -- wolfe@lobo.us  25-Aug-08
        if ((args_out[i][5] == '/') && (args_out[i][7] == '/'))
        {
          // Assume this is a pathname, and adjust accordingly
          //printf("ARGS_IN: PATHNAME ASSUMED: %s\n", args_in[i]);

          strcpy(noleak_buffers[next_buffer], args_in[i]);

          noleak_buffers[next_buffer][5] = noleak_buffers[next_buffer][6];
          noleak_buffers[next_buffer][6] = ':';

          args_out[i] = noleak_buffers[next_buffer];
          //printf("ARGS_OUT: PATHNAME MODIFIED TO BE: %s\n", args_out[i]);
        }
        // Deal with -OUT:"/c/...."
        else if ((args_out[i][6] == '/') && (args_out[i][8] == '/'))
        {
          // Assume this is a pathname, and adjust accordingly
          //printf("ARGS_IN: PATHNAME ASSUMED: %s\n", args_in[i]);

          strcpy(noleak_buffers[next_buffer], args_in[i]);

          noleak_buffers[next_buffer][6] = noleak_buffers[next_buffer][7];
          noleak_buffers[next_buffer][7] = ':';

          args_out[i] = noleak_buffers[next_buffer];
          //printf("ARGS_OUT: PATHNAME MODIFIED TO BE: %s\n", args_out[i]);
        }

        next_buffer++;
      }
      else
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
            }
          }
        }
      }

      if (next_buffer > MAX_NOLEAK_BUFFERS) {
        printf("OOPS - next_buffer > MAX_NOLEAK_BUFFERS\n");
        exit(-1);
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
