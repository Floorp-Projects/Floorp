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
 */

#include "NativeEventThread.h"
#include "CBrowserContainer.h"

#include "ns_util.h"
#include "ns_globals.h"

#include "nsEmbedAPI.h"  // for NS_InitEmbedding

#include "nsIProfile.h" // for the profile manager
#include "nsICmdLineService.h" // for the cmdline service to give to the
                               // profile manager.

#include "nsCRT.h" // for nsCRT::strcmp
#include "prenv.h"
#include "nsILocalFile.h"
// #include "WrapperFactoryImpl.cpp"

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsAppShellCIDs.h" // for NS_SESSIONHISTORY_CID
#include "nsCOMPtr.h" // to get nsIBaseWindow from webshell
//nsIDocShell is included in ns_util.h
#include "nsIEventQueueService.h" // for PLEventQueue
#include "nsRepository.h"
#include "nsIServiceManager.h" // for do_GetService
#include "nsIPref.h" // for preferences
#include "nsIThread.h" // for PRThread
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsCWebBrowser.h"
#include "nsIEventQueueService.h"
#include "nsIThread.h"
//nsIWebShell is included in ns_util.h


#include "prlog.h" // for PR_ASSERT

#ifdef XP_UNIX
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"
#endif

static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kISHistoryIID, NS_ISHISTORY_IID);
static NS_DEFINE_CID(kSHistoryCID, NS_SHISTORY_CID);

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);

static const char *NS_DOCSHELL_PROGID = "component://netscape/docshell/html";
//static const char *NS_WEBBROWSER_PROGID = "component://netscape/embedding/browser/nsWebBrowser";

extern const char * gBinDir; // defined in WrapperFactoryImpl.cpp

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
// Functions to hook into mozilla
// 

extern "C" void NS_SetupRegistry();
extern nsresult NS_AutoregisterComponents();


//
// Local functions
//

/**

 * Called from InitMozillaStuff().

 */

int processEventLoop(WebShellInitContext * initContext);

/**

 * Called from Java nativeInitialize to create the webshell, history
 * and other mozilla things, then start the event loop.

 */

nsresult InitMozillaStuff (WebShellInitContext * arg);

//
// Local data
//

nsISHistory *gHistory = nsnull;
nsIComponentManager *gComponentManager = nsnull;
static PRBool	gFirstTime = PR_TRUE;



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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeInitialize
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    nsresult rv = NS_ERROR_FAILURE;
    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;

    if (nsnull == initContext) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL webShellPtr passed to nativeInitialize.");
        return;
    }
    if (nsnull == gVm) { // declared in ../src_share/jni_util.h
        ::util_GetJavaVM(env, &gVm);  // save this vm reference
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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeProcessEvents
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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeAddListener
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
    }

    return;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeRemoveListener
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
Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeRemoveAllListeners(JNIEnv *env, jobject obj, jint webShellPtr)
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
#ifdef XP_UNIX
    while(gtk_events_pending()) {
        gtk_main_iteration();
    }
#else
    // PENDING(mark): Does this work on the Mac?
    MSG msg;
    
    if (::PeekMessage(&msg, nsnull, 0, 0, PM_NOREMOVE)) {
        if (::GetMessage(&msg, nsnull, 0, 0)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
#endif
    ::PR_Sleep(PR_INTERVAL_NO_WAIT);
    
    if ((initContext->initComplete) && (initContext->actionQueue)) {
        
        PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);
        
        if (::PL_EventAvailable(initContext->actionQueue)) {
            
            PLEvent * event = ::PL_GetEvent(initContext->actionQueue);
            
            if (event != nsnull) {
                ::PL_HandleEvent(event);
            }
        }
        
        PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
        
    }
    if (initContext->stopThread) {
        initContext->stopThread++;
        
        return 0;
      }

    // PENDING(edburns): revisit this.  Not sure why this is necessary, but
    // this fixes bug 44327
    printf("%c", 8); // 8 is ASCII for backspace

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


void DoMozInitialization(WebShellInitContext * initContext)
{    
    if (gFirstTime) {
        nsILocalFile * pathFile = nsnull;
        nsresult rv = nsnull;
        JNIEnv *   env = initContext->env;
        const char * BinDir = gBinDir;

        rv = NS_NewLocalFile(BinDir, PR_TRUE, &pathFile);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "call to NS_NewLocalFile failed.");
            return;
        }
    
        // It is vitally important to call NS_InitEmbedding before calling
        // anything else.
        NS_InitEmbedding(pathFile, nsnull);
        //        NS_SetupRegistry();
        rv = NS_GetGlobalComponentManager(&gComponentManager);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "NS_GetGlobalComponentManager() failed.");
            return;
        }
        prLogModuleInfo = PR_NewLogModule("webclient");
        const char *webclientLogFile = PR_GetEnv("WEBCLIENT_LOG_FILE");
        if (nsnull != webclientLogFile) {
            PR_SetLogFile(webclientLogFile);
            // If this fails, it just goes to stdout/stderr
        }

        gComponentManager->RegisterComponentLib(kSHistoryCID, nsnull, 
                                                nsnull, APPSHELL_DLL, 
                                                PR_FALSE, PR_FALSE);
        NS_AutoregisterComponents();

        // handle the profile manager nonsense
        nsCOMPtr<nsICmdLineService> cmdLine =do_GetService(kCmdLineServiceCID);
        nsCOMPtr<nsIProfile> profile = do_GetService(NS_PROFILE_CONTRACTID);
        if (!cmdLine || !profile) {
            ::util_ThrowExceptionToJava(env, "Can't get the profile manager.");
            return;
        }
        char *argv[1];
        argv[0] = strdup(gBinDir);
        rv = cmdLine->Initialize(1, argv);
        nsCRT::free(argv[0]);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Can't initialize nsICmdLineService.");
            return;
        }
        rv = profile->StartupWithArgs(cmdLine);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Can't statrup nsIProfile service.");
            return;
        }
    }
} 



nsresult InitMozillaStuff (WebShellInitContext * initContext) 
{
    nsresult rv = nsnull;

    DoMozInitialization(initContext);
    
    PR_ASSERT(gComponentManager);

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
        PR_LOG(prLogModuleInfo, 3, 
               ("InitMozillaStuff(%lx): Create the Event Queue for the UI thread...\n", 
                initContext));
    }
#endif
    
    // Create the Event Queue for the UI thread...
    if (!aEventQService) {
        initContext->initFailCode = kEventQueueError;
        return rv;
    }
    
    // Create the event queue.
    rv = aEventQService->CreateThreadEventQueue();
    initContext->embeddedThread = PR_GetCurrentThread();
    
    // Create the action queue
    if (initContext->embeddedThread) {
        
        if (initContext->actionQueue == nsnull) {
            printf("InitMozillaStuff(%lx): Create the action queue\n", initContext);
            
            // We need to do something different for Unix
            nsIEventQueue * EQueue = nsnull;
            
            rv = aEventQService->GetThreadEventQueue(initContext->embeddedThread, 
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
            initContext->actionQueue = plEventQueue;
        }
    }
    else {
        initContext->initFailCode = kCreateWebShellError;
        return NS_ERROR_UNEXPECTED;
    }
    
    // Setup Prefs obj and read default prefs
    if (gFirstTime) {
        nsCOMPtr<nsIPref> mPrefs(do_GetService(kPrefCID));
        if (!mPrefs) {
            initContext->initFailCode = kCreateWebShellError;
            return NS_ERROR_UNEXPECTED;
        }
        rv = mPrefs->StartUp();  
        rv = mPrefs->ReadUserPrefs();
        gFirstTime = PR_FALSE;
    }
    PRBool allowPlugins = PR_TRUE;
    
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
    GdkSuperWin * superwin;
    GtkMozArea * mozarea;
    mozarea = (GtkMozArea *) initContext->gtkWinPtr;
    superwin = mozarea->superwin;
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - before Init Call...\n", initContext));
    }
    rv = initContext->baseWindow->InitWindow((nativeWindow) superwin, nsnull, initContext->x, initContext->y, initContext->w, initContext->h);
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
    
    // set the URIContentListener
    nsCOMPtr<nsIURIContentListener> contentListener(do_QueryInterface(initContext->browserContainer));
    webBrowser->SetParentURIContentListener(contentListener);
    
    // set the TreeOwner
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(initContext->docShell));
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner(do_QueryInterface(initContext->browserContainer));
    docShellAsItem->SetTreeOwner(treeOwner);
    
    // set the docloaderobserver
    nsCOMPtr<nsIDocumentLoaderObserver> observer(do_QueryInterface(initContext->browserContainer));
    initContext->docShell->SetDocLoaderObserver(observer);
    
    if (nsnull == gHistory) {
        rv = gComponentManager->CreateInstance(kSHistoryCID, nsnull, 
                                               kISHistoryIID, 
                                               (void**)&gHistory);
        if (NS_FAILED(rv)) {
            initContext->initFailCode = kHistoryWebShellError;
            return rv;
        }
    }
    
	printf("Creation Done.....\n");
    // Get the WebNavigation Object from the DocShell
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(initContext->docShell));
    initContext->webNavigation = webNav;
    
    // Set the History
    //    initContext->webNavigation->SetSessionHistory(gHistory);
    
    // Save the sessionHistory in the initContext
    //    initContext->sHistory = gHistory;
    
    printf("Show the webBrowser\n");
    // Show the webBrowser
    rv = initContext->baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return rv;
    }
    
    initContext->initComplete = TRUE;
    
#if DEBUG_RAPTOR_CANVAS
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("InitMozillaStuff(%lx): enter event loop\n", initContext));
    }
#endif
    // Just need to loop once to clear out events before returning
    processEventLoop(initContext);
    
    return rv;
}



