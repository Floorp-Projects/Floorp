#define XP_STRSTR                 strstr
#define XP_STRTOUL                strtoul

#include "platform.h"
#include "xp_core.h"
#include "xp_mem.h"
#include "xp_str.h"
#include "xp_list.h"
#include "prprf.h"  
#include <string.h>

#include "xp_file.h"
#include "jsotypes.h"
#include "plstr.h"
#include "seccomon.h"
#include "xpgetstr.h"

#define PORT_Strlen(s) 	strlen(s)

#define HTML_DLGS_URL "file:///y|/htmldlgs.html"
#define XP_DIALOG_CANCEL_BUTTON (1<<0)
#define XP_DIALOG_OK_BUTTON (1<<2)
#define XP_STRINGS_CHUNKSIZE 512
typedef struct _XPDialogState XPDialogState;
typedef struct _XPDialogInfo XPDialogInfo;
typedef struct _XPDialogStrings XPDialogStrings;
typedef PRBool (* XP_HTMLDialogHandler)
  (XPDialogState *state, char **argv, int argc, unsigned int button);

/* SACopy and SACat should really be defined elsewhere */
#include "plstr.h"
#include "prmem.h"
#define LocalStrAllocCopy(dest, src) Local_SACopy (&(dest), src)
#define LocalStrAllocCat(dest, src) Local_SACat (&(dest), src)

struct _XPDialogState {
  PRArenaPool *arena;
  void *window;
  void *proto_win;
  XPDialogInfo *dialogInfo;
  void *arg;
  void (* deleteCallback)(void *arg);
  void *cbarg;
  PRBool deleted;
};

struct _XPDialogInfo {
  unsigned int buttonFlags;
  XP_HTMLDialogHandler handler;
  int width;
  int height;
};

struct _XPDialogStrings
{
  PRArenaPool *arena;
  int basestringnum;
  int nargs;
  char **args;
  char *contents;
};

#ifdef InSingleSignon

char *
Local_SACopy(char **destination, const char *source);

char *
Local_SACat(char **destination, const char *source);

XPDialogState *
XP_MakeHTMLDialog(
  void *proto_win,
  XPDialogInfo *dialogInfo,
  int titlenum,
  XPDialogStrings *strings,
  void *arg,
  PRBool utf8CharSet);

void
XP_MakeHTMLDialog2(XPDialogInfo *dialogInfo);

XPDialogStrings *
XP_GetDialogStrings(int stringnum);

void
XP_SetDialogString(XPDialogStrings *strings, int argNum, char *string);

void
XP_CopyDialogString(XPDialogStrings *strings, int argNum, const char *string);

char *
XP_FindValueInArgs(const char *name, char **av, int ac);

#else

char *
Local_SACopy(char **destination, const char *source)
{
  if(*destination) {
    PL_strfree(*destination);
    *destination = 0;
  }
  *destination = PL_strdup(source);
  return *destination;
}

char *
Local_SACat(char **destination, const char *source)
{
  if (source && *source) {
    if (*destination) {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc(*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL) {
        return(NULL);
      }
      PL_strcpy (*destination + length, source);
    } else {
      *destination = PL_strdup(source);
    }
  }
  return *destination;
}

XPDialogState *
XP_MakeHTMLDialog(
  void *proto_win,
  XPDialogInfo *dialogInfo,
  int titlenum,
  XPDialogStrings *strings,
  void *arg,
  PRBool utf8CharSet)
{
  /* write out html dialog to /htmldlgs.htm */
  XP_File f = fopen("/htmldlgs.htm","w");
  for (int i=0; i<strings->nargs; i++) {
    if (strings->args[i]) {
      fprintf(f, "%s", (char *)(strings->args[i]));
    }
  }
  fclose(f);
  fflush(f);

  /* write out dialog info to /index.htm */
  f = fopen("/index.htm","w");
  fprintf(f, "<HTML> \n");
  fprintf(f, "  <BODY \n");
  fprintf(f, "    onload=\"msgWindow=window.open(\n");
  fprintf(f, "      'htmldlgs.htm', \n");
  fprintf(f, "      'window2', \n");
  fprintf(f, "      'resizable=no, titlebar=no, width=%d, height=%d')\">\n",
    dialogInfo->width, dialogInfo->height);
  fprintf(f, "  </BODY> \n");
  fprintf(f, "</HTML> \n");
  fclose(f);
  fflush(f);

  return NULL;
}

void
XP_MakeHTMLDialog2(XPDialogInfo *dialogInfo) {
  char* argv[] = {NULL, NULL, NULL, NULL, NULL, NULL};
  int argc = 0;
  char* button = NULL;

  nsAutoString * nsCookie = new nsAutoString("");
  nsIURL* url;
  char* separator;

  nsINetService *netservice;
  nsresult res;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_OK == res) && (nsnull != netservice)) {
    const nsAutoString html_dlgs = nsAutoString(HTML_DLGS_URL);
    if (!NS_FAILED(NS_NewURL(&url, html_dlgs))) {
      res = netservice->GetCookieString(url, *nsCookie);

      /* convert cookie to a C string */

      char *cookies = nsCookie->ToNewCString();
      char *cookie = PL_strstr(cookies, "htmldlgs=|"); /* get to htmldlgs=| */
      if (cookie) {
        cookie = cookie + PL_strlen("htmldlgs=|"); /* get passed htmldlgs=| */

        /* button name is first item in cookie (up to next verical bar) */

        separator = strchr(cookie, '|');
        *separator = '\0';
        LocalStrAllocCopy(button, cookie);
        cookie = separator+1;
        *separator = '|';

        /* remainder of cookie string are the args, separated by vertical bars */
        for (int i=0; ((*cookie != '\0') && (*cookie != ';')); i++) {
          separator = strchr(cookie, '|');
          *separator = '\0';
          LocalStrAllocCopy(argv[i], cookie);
          cookie = separator+1;
          *separator = '|';
          argc++;
        }

        /* call the callback routine */
        if (!PORT_Strcmp(button,"OK")) {
          (dialogInfo->handler)(NULL, argv, argc, XP_DIALOG_OK_BUTTON);
        } else {
          (dialogInfo->handler)(NULL, argv, argc, XP_DIALOG_CANCEL_BUTTON);
        }

        /* free up the allocated strings */
        XP_FREE(button);
        for (int j=0; j<argc; j++) {
          XP_FREE(argv[j]);
        }
      }
      delete[] cookies;
    }
    nsServiceManager::ReleaseService(kNetServiceCID, netservice);
  }
}

XPDialogStrings *
XP_GetDialogStrings(int stringnum)
{
  XPDialogStrings *header = NULL;
  PRArenaPool *arena = NULL;
  char *dst, *src;
  int n, size, len, done = 0;

  /* get a new arena */
  arena = PORT_NewArena(XP_STRINGS_CHUNKSIZE);
  if ( arena == NULL ) {
    return(NULL);
  }

  /* allocate the header structure */
  header = (XPDialogStrings *)PORT_ArenaAlloc(arena, sizeof(XPDialogStrings));
  if ( header == NULL ) {
    goto loser;
  }

  /* init the header */
  header->arena = arena;
  header->basestringnum = stringnum;

#ifdef	XP_MAC
  goto loser;
#else
  src = XP_GetString(stringnum);

#endif
  len = PORT_Strlen(src);
  size = len + 1;
  dst = header->contents =
    (char *)PORT_ArenaAlloc(arena, sizeof(char) * size);
  if (dst == NULL) {
    goto loser;
  }

  while (!done) { /* Concatenate pieces to form message */
    PORT_Memcpy(dst, src, len+1);
    done = 1;
    if (XP_STRSTR(src, "%-cont-%")) { /* Continuation */
      src = XP_GetString(++stringnum);
      len = PORT_Strlen(src);
      header->contents =
        (char *)PORT_ArenaGrow(arena,
        header->contents, size, size + len);
      if (header->contents == NULL) {
        goto loser;
      }
      dst = header->contents + size - 1;
      size += len;
      done = 0;
    }
  }

  /* At this point we should have the complete message in
     header->contents, including like %-cont-%, which will be
     ignored later. */

  /* Count the arguments in the message */
  header->nargs = -1; /* Support %0% as lowest token */
  src = header->contents;
  while ((src = PORT_Strchr(src, '%'))) {
    src++;
    n = (int)XP_STRTOUL(src, &dst, 10);
    if (dst == src) { /* Integer not found... */
      src = PORT_Strchr(src, '%') + 1; /* so skip this %..% */
      PORT_Assert(NULL != src-1); /* Unclosed %..% ? */
      continue;
    }

    if (header->nargs < n) {
      header->nargs = n;
    }
    src = dst + 1;
  }

  if (++(header->nargs) > 0) { /* Allocate space for arguments */
    header->args =
      (char **)PORT_ArenaZAlloc(arena, sizeof(char *) * header->nargs);
  }

  return(header);

loser:
  PORT_FreeArena(arena, PR_FALSE);
  return(NULL);
}



void
XP_SetDialogString(XPDialogStrings *strings, int argNum, char *string)
{
  /* make sure we are doing it right */
  PORT_Assert(argNum < strings->nargs);
  PORT_Assert(argNum >= 0);
  PORT_Assert(strings->args[argNum] == NULL);

  /* set the string */
  strings->args[argNum] = string;

  return;
}

void
XP_CopyDialogString(XPDialogStrings *strings, int argNum, const char *string)
{
  int len;

  /* make sure we are doing it right */
  PORT_Assert(argNum < strings->nargs);
  PORT_Assert(argNum >= 0);
  PORT_Assert(strings->args[argNum] == NULL);

  /* copy the string */
  len = PORT_Strlen(string) + 1;
  strings->args[argNum] = (char *)PORT_ArenaAlloc(strings->arena, len);
  if ( strings->args[argNum] != NULL ) {
    PORT_Memcpy(strings->args[argNum], string, len);
  }

  return;
}

char *
XP_FindValueInArgs(const char *name, char **av, int ac)
{
  for( ;ac > 0; ac -= 2, av += 2 ) {
    if ( PORT_Strcmp(name, av[0]) == 0 ) {
      return(av[1]);
    }
  }
  return(0);
}

#endif

#define BUFLEN 5000

#define FLUSH_BUFFER                   \
  if (buffer) {                        \
    LocalStrAllocCat(buffer2, buffer); \
    buffer[0] = '\0';                  \
    g = 0;                             \
  }

extern int XP_EMPTY_STRINGS;
