#ifndef MINIMO_PRIVATE_H
#define MINIMO_PRIVATE_H

#include <stdio.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Win32 header files
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <aygshell.h>
#include <sipapi.h>
#include <winsock2.h>
#include <connmgr.h>

// Mozilla header files
#include "nsEmbedAPI.h"
#include "nsIClipboardCommands.h"
#include "nsXPIDLString.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebBrowserFocus.h"
#include "nsIWindowWatcher.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIURI.h"
#include "plstr.h"
#include "nsIInterfaceRequestor.h"
#include "nsIEventQueueService.h"

#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsProfileDirServiceProvider.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsEmbedCID.h"
#include "nsIPromptService.h"
#include "nsIDOMWindow.h"
#include "nsIComponentRegistrar.h"

#include "nsIWidget.h"

// Local header files
#include "WebBrowserChrome.h"
#include "WindowCreator.h"

nsresult ResizeEmbedding(nsIWebBrowserChrome* chrome);

#endif // MINIMO_PRIVATE_H
