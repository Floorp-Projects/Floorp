/*
 * "@(#)dllmain.c	10.2 (Netscape) 4/25/98";
 */

#include <windows.h>

extern int __os_segdata_init(void);
extern int __os_segdata_destroy(void);

int FAR PASCAL LibMain(HANDLE hInstance, WORD wDataSeg,
                       WORD wHeapSize, LPSTR lpszCmdLine)
{
  if( wHeapSize > 0 ) {
    UnlockData(0);
  }

  if( 0 != __os_segdata_init() ) {
    return 0;
  }

  return 1;
}

int FAR PASCAL _export WEP(int nParam)
{
  (void)__os_segdata_destroy();
  return 1;
}
