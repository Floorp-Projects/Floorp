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
#include "CBrowserContainer.h"

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
#include "NativeEventThreadActionEvents.h"
#include "nsIWindowWatcher.h"
#include "nsIComponentRegistrar.h"
#include "WindowCreator.h"

#include "prlog.h" // for PR_ASSERT

#ifdef XP_UNIX
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"

extern "C" {
    static int	    wc_x_error			 (Display     *display, 
                                          XErrorEvent *error);
}

#endif

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

#ifdef XP_UNIX
static int
wc_x_error (Display	 *display,
            XErrorEvent *error)
{
    if (error->error_code)
        {
            char buf[64];
            
            XGetErrorText (display, error->error_code, buf, 63);
            
            fprintf (stderr, "Webclient-Gdk-ERROR **: %s\n  serial %ld error_code %d request_code %d minor_code %d\n",
                     buf, 
                     error->serial, 
                     error->error_code, 
                     error->request_code,
                     error->minor_code);
        }
    
    return 0;
}
#endif

/**

 * Called from InitMozillaStuff().

 */

int processEventLoop(WebShellInitContext * initContext);

/**

 * Called from Java nativeInitialize to create the webshell 
 * and other mozilla things, then start the event loop.

 */

nsresult InitMozillaStuff (WebShellInitContext * arg);

//
// Local data
//

static PRBool	gFirstTime = PR_TRUE;
PLEventQueue	*	gActionQueue = nsnull;
PRThread		*	gEmbeddedThread = nsnull;
nsISHistory *gHistory = nsnull;
WindowCreator   *   gCreatorCallback = nsnull;

char * errorMessages[] = {
	"No Error",
	"Could not obtain the event queue service.",
	"Unable to create the WebShell instance.",
	"Unable to initialize the WebShell instance.",
	"Unable to show the WebShell."
};

//
// JNI methods
//

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeInitialize
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    nsresult rv = NS_ERROR_FAILURE;
    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;

    if (nsnull == initContext) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL webShellPtr passed to nativeInitialize.");
        return;
    }
    rv = InitMozillaStuff(initContext);
    if (NS_FAILED(rv)) {
	    ::util_ThrowExceptionToJava(env, 
                                    errorMessages[initContext->initFailCode]);
        return;
    }

    while (initContext->initComplete == FALSE) {
        
        ::PR_Sleep(PR_INTERVAL_NO_WAIT);
        
        if (initContext->initFailCode != 0) {
            ::util_ThrowExceptionToJava(env, errorMessages[initContext->initFailCode]);
            return;
        }
    }
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeProcessEvents
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
    
    if (nsnull == initContext) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL webShellPtr passed to nativeProcessEvents.");
        return;
    }

    processEventLoop(initContext);
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
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener, 
 jstring listenerString)
{
    WebShellInitContext *initContext = (WebShellInitContext *)webShellPtr;
        
    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed tonativeAddListener");
        return;
    }

    if (nsnull == initContext->nativeEventThread) {
        // store the java EventRegistrationImpl class in the initContext
        initContext->nativeEventThread = 
            ::util_NewGlobalRef(env, obj); // VERY IMPORTANT!!
        
        // This enables the listener to call back into java
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
    PR_ASSERT(initContext->browserContainer);

    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        initContext->browserContainer->AddDocumentLoadListener(globalRef); 
        break;
    case MOUSE_LISTENER:
        initContext->browserContainer->AddMouseListener(globalRef); 
        break;
    case NEW_WINDOW_LISTENER:
        if (gCreatorCallback)
            gCreatorCallback->AddNewWindowListener(globalRef); 
        break;
    }

    return;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener,
 jstring listenerString)
{
    WebShellInitContext *initContext = (WebShellInitContext *)webShellPtr;

    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed to nativeRemoveListener");
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

    PR_ASSERT(initContext->browserContainer);
    
    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        initContext->browserContainer->RemoveDocumentLoadListener(); 
        break;
    case MOUSE_LISTENER:
        initContext->browserContainer->RemoveMouseListener(); 
        break;
    }
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_NativeEventThread_nativeRemoveAllListeners(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext *initContext = (WebShellInitContext *)webShellPtr;

    if (initContext == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed to nativeRemoveAllListeners");
        return;
    }

    initContext->browserContainer->RemoveAllListeners();
}

/**

 * This function processes methods inserted into WebShellInitContext's
 * actionQueue.  It is called once during the initialization of the
 * NativeEventThread java thread, and infinitely in
 * NativeEventThread.run()'s event loop.  The call to PL_HandleEvent
 * below, ends up calling the nsActionEvent subclass's handleEvent()
 * method.  After which it calls nsActionEvent::destroyEvent().

 */

int processEventLoop(WebShellInitContext * initContext) 
{
    if (PR_GetCurrentThread() != gEmbeddedThread)
        return 0;
    
    if (nsnull == initContext)
        return 0;
    
#ifdef XP_UNIX
    while(gtk_events_pending()) {
        gtk_main_iteration();
    }
#else
    // PENDING(mark): Does this work on the Mac?
    MSG msg;
    PRBool wasHandled;
    
    if (::PeekMessage(&msg, nsnull, 0, 0, PM_NOREMOVE)) {
        if (::GetMessage(&msg, nsnull, 0, 0)) {
            wasHandled = PR_FALSE;
            ::NS_HandleEmbeddingEvent(msg, wasHandled);
            if (!wasHandled) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
    }
#endif
    ::PR_Sleep(PR_INTERVAL_NO_WAIT);
    
    if ((initContext->initComplete) && (gActionQueue)) {
        PLEvent * event = nsnull;
        
        PL_ENTER_EVENT_QUEUE_MONITOR(gActionQueue);
        if (::PL_EventAvailable(gActionQueue)) {
            event = ::PL_GetEvent(gActionQueue);
        }
        PL_EXIT_EVENT_QUEUE_MONITOR(gActionQueue);
        if (event != nsnull) {
            ::PL_HandleEvent(event);
        }
    }
    if (initContext->stopThread) {
        initContext->stopThread++;
        
        return 0;
      }

    // PENDING(edburns): revisit this.  Not sure why this is necessary, but
    // this fixes bug 44327
    //    printf("%c", 8); // 8 is ASCII for backspace

    return 1;
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
nsresult InitializeWindowCreator(WebShellInitContext * initContext)
{
    // create an nsWindowCreator and give it to the WindowWatcher service
    gCreatorCallback = new WindowCreator(initContext);
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

void DoMozInitialization(WebShellInitContext * initContext)
{    
    if (gFirstTime) {
        
        nsresult rv = nsnull;
        JNIEnv *   env = initContext->env;

        const char *webclientLogFile = PR_GetEnv("WEBCLIENT_LOG_FILE");
        if (nsnull != webclientLogFile) {
            PR_SetLogFile(webclientLogFile);
            // If this fails, it just goes to stdout/stderr
        }
        
        InitializeWindowCreator(initContext);

    }
} 



nsresult InitMozillaStuff (WebShellInitContext * initContext) 
{
    nsresult rv = nsnull;

    DoMozInitialization(initContext);
    
    if (gFirstTime) {
        printf ("\n\nCreating Event Queue \n\n");
        nsCOMPtr<nsIEventQueueService> 
            aEventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
    
        // if we get here, we know that aEventQService is not null.
        if (!aEventQService) {
            rv = NS_ERROR_FAILURE;
            return rv;
        }
    
    //TODO Add tracing from nspr.
    
#if DEBUG_RAPTOR_CANVAS
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, ("InitMozillaStuff(%lx): Create the Event Queue for the UI thread...\n", initContext));
        }
#endif
    
        // Create the Event Queue for the UI thread...
        if (!aEventQService) {
            initContext->initFailCode = kEventQueueError;
            return rv;
        }
    
        // Create the event queue.
        rv = aEventQService->CreateThreadEventQueue();
        gEmbeddedThread = PR_GetCurrentThread();
    
        // Create the action queue
        if (gEmbeddedThread) {
            
            if (gActionQueue == nsnull) {
                printf("InitMozillaStuff(%lx): Create the action queue\n", initContext);
            
                // We need to do something different for Unix
                nsIEventQueue * EQueue = nsnull;
            
                rv = aEventQService->GetThreadEventQueue(gEmbeddedThread, 
                                                     &EQueue);
                if (NS_FAILED(rv)) {
                    initContext->initFailCode = kCreateWebShellError;
                    return rv;
                }
            
#ifdef XP_UNIX
                gdk_input_add(EQueue->GetEventQueueSelectFD(),
                              GDK_INPUT_READ,
                              event_processor_callback,
                              EQueue);
#endif
            
                PLEventQueue * plEventQueue = nsnull;
            
                EQueue->GetPLEventQueue(&plEventQueue);
                gActionQueue = plEventQueue;
            }
        }
        else {
            initContext->initFailCode = kCreateWebShellError;
            return NS_ERROR_UNEXPECTED;
        }

#ifdef XP_UNIX

        // The gdk_x_error function exits in some cases, we don't 
        // want that.
        XSetErrorHandler(wc_x_error);
#endif

    }

    if (gFirstTime) {
        gFirstTime = PR_FALSE;
    }
    PRBool allowPlugins = PR_TRUE;


    /*    
    // Create the WebBrowser.
    nsCOMPtr<nsIWebBrowser> webBrowser = nsnull;
    webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    
    initContext->webBrowser = webBrowser;
    
    // Get the BaseWindow from the DocShell - upcast
    //  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(webBrowser));
	nsCOMPtr<nsIBaseWindow> docShellAsWin;
	rv = webBrowser->QueryInterface(NS_GET_IID(nsIBaseWindow), getter_AddRefs(docShellAsWin));
    
    initContext->baseWindow = docShellAsWin;
    
    printf ("Init the baseWindow\n");
    
#ifdef XP_UNIX
    GtkWidget * bin;
    bin = (GtkWidget *) initContext->gtkWinPtr;
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - before Init Call...\n", initContext));
    }
    rv = initContext->baseWindow->InitWindow((nativeWindow) bin, nsnull, initContext->x, initContext->y, initContext->w, initContext->h);
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - after Init Call...\n", initContext));
    }
#else    
    rv = initContext->baseWindow->InitWindow((nativeWindow) initContext->parentHWnd, nsnull,
                                             initContext->x, initContext->y, initContext->w, initContext->h);
#endif
    
    printf("Create the BaseWindow...\n");
    
    rv = initContext->baseWindow->Create();
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kInitWebShellError;
        return rv;
    }
    
    // Create the DocShell
    
    initContext->docShell = do_GetInterface(initContext->webBrowser);
    
    if (!initContext->docShell) {
        initContext->initFailCode = kCreateDocShellError;
        return rv;
    }
    
    // create our BrowserContainer, which implements many many things.
    
    initContext->browserContainer = 
        new CBrowserContainer(initContext->webBrowser, initContext->env, 
                              initContext);
    
    // set the WebShellContainer.  This is a pain.  It's necessary
    // because nsWebShell.cpp still checks for mContainer all over the
    // place.
    nsCOMPtr<nsIWebShellContainer> wsContainer(do_QueryInterface(initContext->browserContainer));
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(initContext->docShell));
    webShell->SetContainer(wsContainer);
    
    // set the TreeOwner
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(initContext->docShell));
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner(do_QueryInterface(initContext->browserContainer));
    docShellAsItem->SetTreeOwner(treeOwner);
    
    // set the docloaderobserver
    nsCOMPtr<nsIDocumentLoaderObserver> observer(do_QueryInterface(initContext->browserContainer));
    initContext->docShell->SetDocLoaderObserver(observer);
    
	printf("Creation Done.....\n");
    // Get the WebNavigation Object from the DocShell
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(initContext->docShell));
    initContext->webNavigation = webNav;
    
    printf("Show the webBrowser\n");
    // Show the webBrowser
    rv = initContext->baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return rv;
    }
    
    initContext->initComplete = TRUE;
   
    */

    wsRealizeBrowserEvent * actionEvent = new wsRealizeBrowserEvent(initContext);
    PLEvent			* event       = (PLEvent*) *actionEvent;      
    ::util_PostSynchronousEvent(initContext, event);


#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("InitMozillaStuff(%lx): enter event loop\n", initContext));
    }
#endif

    processEventLoop(initContext);
    
    return rv;
}



