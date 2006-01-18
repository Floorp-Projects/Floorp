#ifndef TOPSRCDIR
#include "../topsrcdir.h"
#endif

#define WCE_BIN   "c:/Program Files/Microsoft eMbedded C++ 4.0/EVC/wce420/bin/"

#define WCE_INC   "c:/Program Files/Windows CE Tools/wce420/POCKET PC 2003/Include/Armv4/"
#define WCE_LIB   "c:/Program Files/Windows CE Tools/wce420/POCKET PC 2003/Lib/Armv4/"

#define SHUNT_LIB TOPSRCDIR "/build/wince/shunt/build/static/ARMV4Dbg/"
#define SHUNT_INC TOPSRCDIR "/build/wince/shunt/include/"

#define ASM_PATH WCE_BIN "armasm.exe"
#define CL_PATH WCE_BIN "clarm.exe"
#define LIB_PATH WCE_BIN "lib.exe"
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
