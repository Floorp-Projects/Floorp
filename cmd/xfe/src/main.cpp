
#include "FolderFrame.h"
#include "MsgFrame.h"
#include "ThreadFrame.h"
#include "AddrBookFrame.h"

Widget toplevel;
XtAppContext theApp;

int
main(int argc,
     char **argv)
{
  toplevel = XtAppInitialize(&theApp, "Mozilla", NULL, 0, &argc, argv, NULL, NULL, 0);

  fe_showFolders(toplevel);
  fe_showMsg(toplevel);
  fe_showThread(toplevel);
  fe_showAddrBook(toplevel);

  XtAppMainLoop(theApp);

  return 0; // keep cc happy
}
