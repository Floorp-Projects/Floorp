/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 *               Kyle Yuan <kyle.yuan@sun.com>
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_NativeEventThread.h"

#include "ns_util.h"
#include "ns_globals.h"


#include "nsCRT.h" // for nsCRT::strcmp
#include "prenv.h"
// #include "WrapperFactoryImpl.cpp"

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsAppShellCIDs.h" // for NS_SESSIONHISTORY_CID
#include "nsCOMPtr.h" // to get nsIBaseWindow from webshell
//nsIDocShell is included in ns_util.h
#include "nsIEventQueueService.h" // for PLEventQueue
#include "nsIServiceManager.h" // for do_GetService
#include "nsISHistory.h" // for sHistory
#include "nsIThread.h" // for PRThread
#include "nsIDocShell.h"
#include "nsEmbedAPI.h" // for NS_HandleEmbeddingEvent
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIEventQueueService.h"
//nsIWebShell is included in ns_util.h
// #include "NativeEventThreadActionEvents.h"
#include "nsIWindowWatcher.h"
#include "nsIComponentRegistrar.h"
#include "WindowCreator.h"

#include "NativeBrowserControl.h"

#include "prlog.h" // for PR_ASSERT


static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kISHistoryIID, NS_ISHISTORY_IID);
static NS_DEFINE_CID(kSHistoryCID, NS_SHISTORY_CID);


static const char *NS_DOCSHELL_PROGID = "component://netscape/docshell/html";
//static const char *NS_WEBBROWSER_PROGID = "component://netscape/embedding/browser/nsWebBrowser";

#ifdef XP_PC

// All this stuff is needed to initialize the history

#define APPSHELL_DLL "appshell.dll"
#define BROWSER_DLL  "nsbrowser.dll"
#define EDITOR_DLL "ender.dll"

#else

#ifdef XP_MAC

#define APPSHELL_DLL "APPSHELL_DLL"
#define EDITOR_DLL  "ENDER_DLL"

#else

// XP_UNIX || XP_BEOS
#define APPSHELL_DLL  "libnsappshell"MOZ_DLL_SUFFIX
#define APPCORES_DLL  "libappcores"MOZ_DLL_SUFFIX
#define EDITOR_DLL  "libender"MOZ_DLL_SUFFIX

#endif // XP_MAC

#endif // XP_PC

//
// Local functions
//


//
// Local data
//

static PRBool	gFirstTime = PR_TRUE;
nsISHistory *gHistory = nsnull;
WindowCreator   *   gCreatorCallback = nsnull;

//
// JNI methods
//

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeProcessEvents
(JNIEnv *env, jobject obj, jint nativePtr)
{
    NativeWrapperFactory * nativeWrapperFactory = 
        (NativeWrapperFactory *) nativePtr;
    
    if (nsnull == nativePtr) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL nativePtr passed to nativeProcessEvents.");
        return;
    }
    
    nativeWrapperFactory->ProcessEventLoop();
}

/**

 * <P> This method takes the typedListener, which is a
 * WebclientEventListener java subclass, figures out what type of
 * subclass it is, using the gSupportedListenerInterfaces array, and
 * calls the appropriate add*Listener local function.  </P>

 *<P> PENDING(): we could do away with the switch statement using
 * function pointers, or some other mechanism.  </P>

 * <P>the NewGlobalRef call is very important, since the argument
 * typedListener is used to call back into java, at another time, as a
 * result of the a mozilla event being fired.</P>

 * PENDING(): implement nativeRemoveListener, which must call
 * RemoveGlobalRef.

 */

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeAddListener
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject typedListener, 
 jstring listenerString)
{
    /***************

    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *)nativeBCPtr;
        
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBrowserControl passed tonativeAddListener");
        return;
    }

    jclass clazz = nsnull;
    int listenerType = 0;
    const char *listenerStringChars = ::util_GetStringUTFChars(env, 
                                                               listenerString);
    if (listenerStringChars == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeAddListener: Can't get className for listener.");
        return;
    }
        
    while (nsnull != gSupportedListenerInterfaces[listenerType]) {
        if (0 == nsCRT::strcmp(gSupportedListenerInterfaces[listenerType], 
                               listenerStringChars)) {
            // We've got a winner!
            break;
        }
        listenerType++;
    }
    ::util_ReleaseStringUTFChars(env, listenerString, listenerStringChars);
    listenerStringChars = nsnull;
    
    if (LISTENER_NOT_FOUND == (LISTENER_CLASSES) listenerType) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't find listener \n\tclass for argument");
        return;
    }

    jobject globalRef = nsnull;

    // PENDING(edburns): make sure do DeleteGlobalRef on the removeListener
    if (nsnull == (globalRef = ::util_NewGlobalRef(env, typedListener))) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't create NewGlobalRef\n\tfor argument");
        return;
    }
    PR_ASSERT(nativeBrowserControl->browserContainer);

    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        nativeBrowserControl->browserContainer->AddDocumentLoadListener(globalRef); 
        break;
    case MOUSE_LISTENER:
        nativeBrowserControl->browserContainer->AddMouseListener(globalRef); 
        break;
    case NEW_WINDOW_LISTENER:
        if (gCreatorCallback)
            gCreatorCallback->AddNewWindowListener(globalRef); 
        break;
    }
    *********************/
    return;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveListener
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject typedListener,
 jstring listenerString)
{
    /*******************
    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *)nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBrowserControl passed to nativeRemoveListener");
        return;
    }
    
    jclass clazz = nsnull;
    int listenerType = 0;
    const char *listenerStringChars = ::util_GetStringUTFChars(env, 
                                                               listenerString);
    if (listenerStringChars == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeRemoveListener: Can't get className for listener.");
        return;
    }
    
    while (nsnull != gSupportedListenerInterfaces[listenerType]) {
        if (0 == nsCRT::strcmp(gSupportedListenerInterfaces[listenerType], 
                               listenerStringChars)) {
            // We've got a winner!
            break;
        }
        listenerType++;
    }
    ::util_ReleaseStringUTFChars(env, listenerString, listenerStringChars);
    listenerStringChars = nsnull;
    
    if (LISTENER_NOT_FOUND == (LISTENER_CLASSES) listenerType) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeRemoveListener(): can't find listener \n\tclass for argument");
        return;
    }

    PR_ASSERT(nativeBrowserControl->browserContainer);
    
    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        nativeBrowserControl->browserContainer->RemoveDocumentLoadListener(); 
        break;
    case MOUSE_LISTENER:
        nativeBrowserControl->browserContainer->RemoveMouseListener(); 
        break;
    }
    *****************/
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveAllListeners(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    /*******************
    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *)nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBrowserControl passed to nativeRemoveAllListeners");
        return;
    }

    nativeBrowserControl->browserContainer->RemoveAllListeners();
    *******************/
}

// Ashu
#ifdef XP_UNIX
static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition) {
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3,
               "EventHandler: event_processor_callback()\n");
    }
#endif
    nsIEventQueue *eventQueue = (nsIEventQueue*)data;
    eventQueue->ProcessPendingEvents();
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, "EventHandler: Done processing pending events\n");
    }
#endif
}
#endif
//


/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This is how all new windows are opened,
   except any created directly by the embedding app. */
/*****************
nsresult InitializeWindowCreator(NativeBrowserControl * nativeBrowserControl)
{
    // create an nsWindowCreator and give it to the WindowWatcher service
    gCreatorCallback = new WindowCreator(nativeBrowserControl);
    if (gCreatorCallback)
    {
        nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, gCreatorCallback));
        if (windowCreator)
        {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
            if (wwatch)
            {
                wwatch->SetWindowCreator(windowCreator);
                return NS_OK;
            }
        }
    }
    return NS_ERROR_FAILURE;
}
*******************/

nsresult InitMozillaStuff (NativeBrowserControl * nativeBrowserControl) 
{
    nsresult rv = nsnull;

    /*    
    // Create the WebBrowser.
    nsCOMPtr<nsIWebBrowser> webBrowser = nsnull;
    webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    
    nativeBrowserControl->webBrowser = webBrowser;
    
    // Get the BaseWindow from the DocShell - upcast
    //  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(webBrowser));
	nsCOMPtr<nsIBaseWindow> docShellAsWin;
	rv = webBrowser->QueryInterface(NS_GET_IID(nsIBaseWindow), getter_AddRefs(docShellAsWin));
    
    nativeBrowserControl->baseWindow = docShellAsWin;
    
    printf ("Init the baseWindow\n");
    
#ifdef XP_UNIX
    GtkWidget * bin;
    bin = (GtkWidget *) nativeBrowserControl->gtkWinPtr;
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - before Init Call...\n", nativeBrowserControl));
    }
    rv = nativeBrowserControl->baseWindow->InitWindow((nativeWindow) bin, nsnull, nativeBrowserControl->x, nativeBrowserControl->y, nativeBrowserControl->w, nativeBrowserControl->h);
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - after Init Call...\n", nativeBrowserControl));
    }
#else    
    rv = nativeBrowserControl->baseWindow->InitWindow((nativeWindow) nativeBrowserControl->parentHWnd, nsnull,
                                             nativeBrowserControl->x, nativeBrowserControl->y, nativeBrowserControl->w, nativeBrowserControl->h);
#endif
    
    printf("Create the BaseWindow...\n");
    
    rv = nativeBrowserControl->baseWindow->Create();
    
    if (NS_FAILED(rv)) {
        nativeBrowserControl->initFailCode = kInitWebShellError;
        return rv;
    }
    
    // Create the DocShell
    
    nativeBrowserControl->docShell = do_GetInterface(nativeBrowserControl->webBrowser);
    
    if (!nativeBrowserControl->docShell) {
        nativeBrowserControl->initFailCode = kCreateDocShellError;
        return rv;
    }
    
    // create our BrowserContainer, which implements many many things.
    
    nativeBrowserControl->browserContainer = 
        new CBrowserContainer(nativeBrowserControl->webBrowser, nativeBrowserControl->env, 
                              nativeBrowserControl);
    
    // set the WebShellContainer.  This is a pain.  It's necessary
    // because nsWebShell.cpp still checks for mContainer all over the
    // place.
    nsCOMPtr<nsIWebShellContainer> wsContainer(do_QueryInterface(nativeBrowserControl->browserContainer));
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(nativeBrowserControl->docShell));
    webShell->SetContainer(wsContainer);
    
    // set the TreeOwner
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(nativeBrowserControl->docShell));
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner(do_QueryInterface(nativeBrowserControl->browserContainer));
    docShellAsItem->SetTreeOwner(treeOwner);
    
    // set the docloaderobserver
    nsCOMPtr<nsIDocumentLoaderObserver> observer(do_QueryInterface(nativeBrowserControl->browserContainer));
    nativeBrowserControl->docShell->SetDocLoaderObserver(observer);
    
	printf("Creation Done.....\n");
    // Get the WebNavigation Object from the DocShell
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(nativeBrowserControl->docShell));
    nativeBrowserControl->webNavigation = webNav;
    
    printf("Show the webBrowser\n");
    // Show the webBrowser
    rv = nativeBrowserControl->baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        nativeBrowserControl->initFailCode = kShowWebShellError;
        return rv;
    }
    
    nativeBrowserControl->initComplete = TRUE;
   

    wsRealizeBrowserEvent * actionEvent = new wsRealizeBrowserEvent(nativeBrowserControl);
    PLEvent			* event       = (PLEvent*) *actionEvent;      
    ::util_PostSynchronousEvent(nativeBrowserControl, event);


#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("InitMozillaStuff(%lx): enter event loop\n", nativeBrowserControl));
    }
#endif
    */
    return rv;
}



