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
 *               Ann Sunhachawee
 */

#include "NativeEventThread.h"

#include "jni_util.h"
#include "EventRegistration.h" // event callbacks

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsAppShellCIDs.h" // for NS_SESSIONHISTORY_CID
#include "nsIBaseWindow.h" // to get methods like SetVisibility
#include "nsCOMPtr.h" // to get nsIBaseWindow from webshell
//nsIDocShell is included in jni_util.h
#include "nsIEventQueueService.h" // for PLEventQueue
#include "nsIPref.h" // for preferences
#include "nsRepository.h"
#include "nsIServiceManager.h" // for nsServiceManager::GetService
#include "nsISessionHistory.h" // for history
#include "nsIThread.h" // for PRThread
//nsIWebShell is included in jni_util.h


static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_CID(kPrefCID,             NS_PREF_CID);

static NS_DEFINE_IID(kISessionHistoryIID, NS_ISESSIONHISTORY_IID);
static NS_DEFINE_CID(kSessionHistoryCID, NS_SESSIONHISTORY_CID);

//
// Local functions
//

/**

 * Called from InitMozillaStuff().

 */

int processEventLoop(WebShellInitContext * initContext);

/**

 * Called from Java nativeInitialize to create the webshell, history,
 * prefs, and other mozilla things, then start the event loop.

 */

nsresult InitMozillaStuff (WebShellInitContext * arg);

//
// Local data
//

static nsISessionHistory *gHistory = NULL;

char * errorMessages[] = {
	"No Error",
	"Could not obtain the event queue service.",
	"Unable to create the WebShell instance.",
	"Unable to initialize the WebShell instance.",
	"Unable to show the WebShell."
};

/**

 * a null terminated array of listener interfaces we support.

 * PENDING(): this should probably live in EventRegistration.h

 */

const char *gSupportedListenerInterfaces[] = {
    "org/mozilla/webclient/DocumentLoadListener",
    NULL
};

// these index into the gSupportedListenerInterfaces array, this should
// also live in EventRegistration.h

typedef enum {
    DOCUMENT_LOAD_LISTENER = 0,
    LISTENER_NOT_FOUND
} LISTENER_CLASSES;


//
// JNI methods
//

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeInitialize
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    nsresult rv = NS_ERROR_FAILURE;
    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;

    if (NULL == initContext) {
	    ::util_ThrowExceptionToJava(env, 
                                    "NULL webShellPtr passed to nativeInitialize.");
        return;
    }
    if (NULL == gVm) { // declared in jni_util.h
        env->GetJavaVM(&gVm);  // save this vm reference
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
            ::util_ThrowExceptionToJava(initContext->env, errorMessages[initContext->initFailCode]);
            return;
        }
    }
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeProcessEvents
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
    
    if (NULL == initContext) {
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

 * See the comments for EventRegistration.h:addDocumentLoadListener

 */

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NativeEventThread_nativeAddListener
(JNIEnv *env, jobject obj, jint webShellPtr, jobject typedListener)
{
    WebShellInitContext *initContext = (WebShellInitContext *)webShellPtr;

    if (initContext == NULL) {
        ::util_ThrowExceptionToJava(env, "Exception: null initContext passed tonativeAddListener");
        return;
    }
    
    if (NULL == initContext->nativeEventThread) {
        // store the java EventRegistrationImpl class in the initContext
        initContext->nativeEventThread = obj; // VERY IMPORTANT!!
        // This enables the listener to call back into java
    }
    
    jclass clazz = NULL;
    int listenerType = 0;

    while (NULL != gSupportedListenerInterfaces[listenerType]) {
        if (NULL == 
            (clazz = 
             env->FindClass(gSupportedListenerInterfaces[listenerType]))) {
            return;
        }
        if (env->IsInstanceOf(typedListener, clazz)) {
            // We've got a winner!
            break;
        }
        listenerType++;
    }
    
    if (LISTENER_NOT_FOUND == (LISTENER_CLASSES) listenerType) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't find listener \n\tclass for argument");
        return;
    }

    jobject globalRef = NULL;

    // PENDING(edburns): make sure do DeleteGlobalRef on the removeListener
    if (NULL == (globalRef = env->NewGlobalRef(typedListener))) {
        ::util_ThrowExceptionToJava(env, "Exception: NativeEventThread.nativeAddListener(): can't create NewGlobalRef\n\tfor argument");
        return;
    }

    switch(listenerType) {
    case DOCUMENT_LOAD_LISTENER:
        addDocumentLoadListener(env, initContext, globalRef); 
        break;
    }
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
    
    if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (::GetMessage(&msg, NULL, 0, 0)) {
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
            
            if (event != NULL) {
                ::PL_HandleEvent(event);
            }
        }
        
        PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
        
    }
    if (initContext->stopThread) {
        initContext->stopThread++;
        
        return 0;
      }
    return 1;
}

nsresult InitMozillaStuff (WebShellInitContext * initContext) 
{
    nsIEventQueueService * aEventQService = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    PRBool allowPlugins = PR_FALSE;

    //TODO Add tracing from nspr.
    
#if DEBUG_RAPTOR_CANVAS
    printf("InitMozillaStuff(%lx): Create the Event Queue for the UI thread...\n", initContext);
#endif
    
    // Create the Event Queue for the UI thread...
    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      kIEventQueueServiceIID,
                                      (nsISupports **) &aEventQService);
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kEventQueueError;
        return rv;
    }
    
    // Create the event queue.
    rv = aEventQService->CreateThreadEventQueue();

    initContext->embeddedThread = PR_GetCurrentThread();

    // Create the action queue
    if (initContext->embeddedThread) {

#if DEBUG_RAPTOR_CANVAS
		printf("InitMozillaStuff(%lx): embeddedThread != NULL\n", initContext);
#endif
		if (initContext->actionQueue == NULL) {
#if DEBUG_RAPTOR_CANVAS
			printf("InitMozillaStuff(%lx): Create the action queue\n", initContext);
#endif
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

#if DEBUG_RAPTOR_CANVAS
	printf("InitMozillaStuff(%lx): Create the WebShell...\n", initContext);
#endif
    // Create the WebShell.
    rv = nsRepository::CreateInstance(kWebShellCID, nsnull, kIWebShellIID, (void**)&initContext->webShell);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kCreateWebShellError;
        return rv;
    }
    
#if DEBUG_RAPTOR_CANVAS
	printf("InitMozillaStuff(%lx): Init the WebShell...\n", initContext);
#endif
    
    rv = initContext->webShell->Init((nsNativeWidget *)initContext->parentHWnd,
                                     initContext->x, initContext->y, initContext->w, initContext->h);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kInitWebShellError;
        return rv;
    }

#if DEBUG_RAPTOR_CANVAS
	printf("InitMozillaStuff(%lx): Install Prefs in the Webshell...\n", initContext);
#endif

    nsIPref *prefs;
    
    rv = nsServiceManager::GetService(kPrefCID, 
                                      nsIPref::GetIID(), 
                                      (nsISupports **)&prefs);
    if (NS_SUCCEEDED(rv)) {
        // Set the prefs in the outermost webshell.
        initContext->webShell->SetPrefs(prefs);
        nsServiceManager::ReleaseService(kPrefCID, prefs);
    }

    if (NULL == gHistory) {
        rv = nsComponentManager::CreateInstance(kSessionHistoryCID, nsnull, kISessionHistoryIID, 
                                                (void**)&gHistory);
        if (NS_FAILED(rv)) {
            initContext->initFailCode = kHistoryWebShellError;
            return rv;
        }
    }
    initContext->webShell->SetSessionHistory(gHistory);

    // set the sessionHistory in the initContexn
    initContext->sessionHistory = gHistory;
    
#if DEBUG_RAPTOR_CANVAS
	printf("InitMozillaStuff(%lx): Show the WebShell...\n", initContext);
#endif

    nsCOMPtr<nsIBaseWindow> baseWindow;
    
    rv = initContext->webShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return rv;
    }
    rv = baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return rv;
    }

    // set the docShell
    rv = initContext->webShell->QueryInterface(NS_GET_IID(nsIDocShell),
                                               getter_AddRefs(initContext->docShell));
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kHistoryWebShellError;
        ::util_ThrowExceptionToJava(initContext->env, 
                                    "Exception: Can't get DocShell from nsIWebShell");
        return rv;
    }
    
    
    initContext->initComplete = TRUE;
    
#if DEBUG_RAPTOR_CANVAS
    printf("InitMozillaStuff(%lx): enter event loop\n", initContext);
#endif

    // Just need to loop once to clear out events before returning
    processEventLoop(initContext);

    return rv;
}
