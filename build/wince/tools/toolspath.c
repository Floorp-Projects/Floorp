#include "toolspath.h"
int argpath_conv(char **args_in, char **args_out)
{
  int i = 0;

  while (args_in[i])
  {
    char *offset;

    args_out[i] = args_in[i];

    if (args_in[i])
    {
      // First, look for the case of "Fo" and "Fe" options
      if ( (args_out[i][0] == '-' || args_out[i][0] == '/') &&
           (args_out[i][1] == 'F') && ((args_out[i][2] == 'o') || (args_out[i][2] == 'e')) &&
           (args_out[i][3] == '/') && (strlen(args_out[i]) > 5) ) {

        strcpy(noleak_buffers[next_buffer], args_in[i]);

        noleak_buffers[next_buffer][0] = '/';
        noleak_buffers[next_buffer][3] = noleak_buffers[next_buffer][4];
        noleak_buffers[next_buffer][4] = ':';

        args_out[i] = noleak_buffers[next_buffer];

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

 char theCmdLine[1024*32] = {'\0'};

 int totalLen = 0;
 int i, j;


 // Clear any link env variable that might get us tangled up
 _putenv("LINK=");
 _putenv("LIBPATH=");
 _putenv("CC=");

 _putenv("INCLUDE=" SHUNT_INC ";" WM_SDK_INC ";" OGLES_SDK_INC ";" WCE_INC);
 _putenv("LIB=" WCE_LIB ";" OGLES_SDK_LIB ";" WCE_CRT);

 i = strlen(args[0]);
 for (j=0; j<i; j++)
 {
   if (args[0][j] == '/')
     args[0][j] = '\\';
 }

 for (j=0; args[j]; j++)
 {
   int len = strlen(args[j]);
   strcat(&theCmdLine[totalLen], args[j]);
   totalLen += len;

   strcat(&theCmdLine[totalLen], " ");
   totalLen++;
 }

 ZeroMemory( &si, sizeof(si) );
 si.cb = sizeof(si);
 ZeroMemory( &pi, sizeof(pi));

 // See bug 508721.
 // lpApplicationName (first parameter) when provided conflicts with
 // the first token in the lpCommandLine (second parameter).
 // So we pass the whole command line including the EXE in lpCommandLine.
 // See http://support.microsoft.com/kb/175986 for more info.
 CreateProcess(NULL,
               theCmdLine,
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

