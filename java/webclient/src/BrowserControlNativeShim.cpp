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

/*
 * BrowserControlNativeShim.cpp
 */

#include <jni.h>
#include "BrowserControlNativeShim.h"

#ifdef XP_PC
#include <windows.h>
#endif

// Raptor includes
#include <nsGUIEvent.h>
#include "nsIWidget.h"
#include "nsIWebShell.h"
#include "nsISessionHistory.h"
#include "nsAppShellCIDs.h"
#include "nsWidgetsCID.h"
#include "nsIGlobalHistory.h"
#include "nsIComponentManager.h"
#include "nsIThread.h"
#include "nsString.h"
#include "nsRepository.h"
#ifdef NECKO
#include "nsNetUtil.h"
#else
#include "nsINetService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"

#include "prmem.h"

#ifdef XP_MAC
#include "nsMacMessageSink.h"
#include "nsRepeater.h"

nsMacMessageSink gMessageSink;

#endif

#ifdef XP_UNIX
#include <gtk/gtk.h>
#include "motif/MozillaEventThread.h"
#endif

#include "nsActions.h"

#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include "DocumentObserver.h"
#include "nsIDocumentLoader.h"

#include "nsCOMPtr.h"
#include "nsIBaseWindow.h"


#ifdef XP_PC

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

#define DEBUG_RAPTOR_CANVAS 1

static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID); // PENDING(edburns): should this be NS_DEFINE_CID?
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kISessionHistoryIID, NS_ISESSIONHISTORY_IID);
static NS_DEFINE_CID(kSessionHistoryCID, NS_SESSIONHISTORY_CID);

static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kPrefCID,             NS_PREF_CID);

extern "C" void NS_SetupRegistry();
extern nsresult NS_AutoregisterComponents();

static nsFileSpec gBinDir;  
static nsISessionHistory *gHistory;

struct WebShellInitContext {
#ifdef XP_UNIX
    GtkWidget * parentHWnd;
#else 
    // PENDING(mark): Don't we need something for Mac?
    HWND				parentHWnd;
#endif
	nsIWebShell		*	webShell;
	PLEventQueue	*	actionQueue;
	PRThread		*	embeddedThread;
	JNIEnv			*	env;
	int					stopThread;
	int					initComplete;
	int					initFailCode;
	int					x;
	int					y;
	int					w;
	int					h;
};

void    PostEvent (WebShellInitContext * initContext, PLEvent * event);
void *  PostSynchronousEvent (WebShellInitContext * initContext, PLEvent * event);

static void
ThrowExceptionToJava (JNIEnv * env, const char * message)
{
    if (env->ExceptionOccurred())
	env->ExceptionClear();
    jclass excCls = env->FindClass("java/lang/Exception");
    
    if (excCls == 0) { // Unable to find the exception class, give up.
	if (env->ExceptionOccurred())
	    env->ExceptionClear();
	return;
    }
    
    // Throw the exception with the error code and description
    jmethodID jID = env->GetMethodID(excCls, "<init>", "(Ljava/lang/String;)V");		// void Exception(String)
    
    if (jID != NULL) {
	jstring	exceptionString = env->NewStringUTF(message);
	jthrowable newException = (jthrowable) env->NewObject(excCls, jID, exceptionString);
        
        if (newException != NULL) {
	    env->Throw(newException);
	}
	else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
	} else {
	    if (env->ExceptionOccurred())
		env->ExceptionClear();
	}
} // ThrowExceptionToJava()


enum {
	kEventQueueError = 1,
	kCreateWebShellError,
	kInitWebShellError,
	kShowWebShellError,
    kHistoryWebShellError
};


char * errorMessages[] = {
	"No Error",
	"Could not obtain the event queue service.",
	"Unable to create the WebShell instance.",
	"Unable to initialize the WebShell instance.",
	"Unable to show the WebShell."
};


int processEventLoop(WebShellInitContext * initContext) {
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

#ifdef XP_UNIX
static void event_processor_callback(gpointer data,
                                     gint source,
                                     GdkInputCondition condition) {
#if DEBUG_RAPTOR_CANVAS
    printf("EventHandler: event_processor_callback()\n");
#endif
    nsIEventQueue *eventQueue = (nsIEventQueue*)data;
    eventQueue->ProcessPendingEvents();
#if DEBUG_RAPTOR_CANVAS
    printf("EventHandler: Done processing pending events\n");
#endif
}
#endif

void
EmbeddedEventHandler (void * arg) {
    WebShellInitContext * initContext = (WebShellInitContext *) arg;
    nsIEventQueueService * aEventQService = nsnull;
    nsresult rv;
    PRBool allowPlugins = PR_FALSE;

    //TODO Add tracing from nspr.
    
#if DEBUG_RAPTOR_CANVAS
    printf("EmbeddedEventHandler(%lx): Create the Event Queue for the UI thread...\n", initContext);
#endif
    
    // Create the Event Queue for the UI thread...
    rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                      kIEventQueueServiceIID,
                                      (nsISupports **) &aEventQService);
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kEventQueueError;
        return;
    }
    
    // Create the event queue.
    rv = aEventQService->CreateThreadEventQueue();

#ifndef NECKO    
    NS_InitINetService();
#endif
    // HACK(mark): On Unix, embeddedThread isn't set until after this method returns!!
    initContext->embeddedThread = PR_GetCurrentThread();

    // Create the action queue
    if (initContext->embeddedThread) {

#if DEBUG_RAPTOR_CANVAS
		printf("EmbeddedEventHandler(%lx): embeddedThread != NULL\n", initContext);
#endif
		if (initContext->actionQueue == NULL) {
#if DEBUG_RAPTOR_CANVAS
			printf("EmbeddedEventHandler(%lx): Create the action queue\n", initContext);
#endif
#ifdef XP_UNIX
            // We need to do something different for Unix
            nsIEventQueue * EQueue = nsnull;
            
            rv = aEventQService->GetThreadEventQueue(PR_GetCurrentThread(), &EQueue);
            gdk_input_add(EQueue->GetEventQueueSelectFD(),
                          GDK_INPUT_READ,
                          event_processor_callback,
                          EQueue);

            PLEventQueue * plEventQueue = nsnull;

            EQueue->GetPLEventQueue(&plEventQueue);
            initContext->actionQueue = plEventQueue;
#else
			initContext->actionQueue = ::PL_CreateMonitoredEventQueue("Action Queue", initContext->embeddedThread);
#endif
		}
	}

#if DEBUG_RAPTOR_CANVAS
	printf("EmbeddedEventHandler(%lx): Create the WebShell...\n", initContext);
#endif
    // Create the WebShell.
    rv = nsRepository::CreateInstance(kWebShellCID, nsnull, kIWebShellIID, (void**)&initContext->webShell);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kCreateWebShellError;
        return;
    }
    
#if DEBUG_RAPTOR_CANVAS
	printf("EmbeddedEventHandler(%lx): Init the WebShell...\n", initContext);
#endif
    
    rv = initContext->webShell->Init((nsNativeWidget *)initContext->parentHWnd,
                                     initContext->x, initContext->y, initContext->w, initContext->h);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kInitWebShellError;
        return;
    }

#if DEBUG_RAPTOR_CANVAS
	printf("EmbeddedEventHandler(%lx): Install Prefs in the Webshell...\n", initContext);
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

    rv = nsComponentManager::CreateInstance(kSessionHistoryCID, nsnull, kISessionHistoryIID, 
                                            (void**)&gHistory);
	if (NS_FAILED(rv)) {
        initContext->initFailCode = kHistoryWebShellError;
        return;
    }
    
    initContext->webShell->SetSessionHistory(gHistory);
    
    
#if DEBUG_RAPTOR_CANVAS
	printf("EmbeddedEventHandler(%lx): Show the WebShell...\n", initContext);
#endif

    nsCOMPtr<nsIBaseWindow> baseWindow;
    
    rv = initContext->webShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return;
    }
    rv = baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return;
    }
    
    initContext->initComplete = TRUE;
    
#if DEBUG_RAPTOR_CANVAS
    printf("EmbeddedEventHandler(%lx): enter event loop\n", initContext);
#endif

    // Don't loop here. The looping will be handled by a Java Thread 
    // (see org.mozilla.webclient.motif.MozillaEventThread).
#ifndef XP_UNIX	
    do {
        if (!processEventLoop(initContext)) {
            break;
        }
    } while (PR_TRUE);
#else
    // Just need to loop once for Unix
    processEventLoop(initContext);
#endif
}

// It appears the current thread (belonging to Java) is assumed to be
// the "main GUI thread." (Creation story given below.) Hence the
// RunPump() of the nsToolkit conflicting with the Java event dispatching.
//
// I'm unsure whether threads created PR_LOCAL_THREAD have
// a one-to-one correspondence with the threads appearing
// on Win32. Changed the "main GUI thread" to be
// a global thread, rather than a local thread.
//
// Creation story...
// nsWebShell::Init() [ webshell/src/nsWebShell.cpp ] calling
//  nsWindow::StandardWindowCreate() [ widget/src/windows/nsWindow.cpp ] calling
//    nsBaseWidget::BaseCreate() [ widget/src/xpwidgets/nsBaseWidget.cpp ] calling
//      nsToolkit::Init() [ widget/src/windows/nsToolkit.cpp ]
//
void
InitEmbeddedEventHandler (WebShellInitContext* initContext)
{
#if DEBUG_RAPTOR_CANVAS
    printf("InitEmbeddedEventHandler(%lx): Creating embedded thread...\n", initContext);
#endif
    // Don't create a NSPR Thread on Unix, because we will use a Java Thread instead.
    // (see org.mozilla.webclient.motif.MozillaEventThread).
#ifndef XP_UNIX
    initContext->embeddedThread = ::PR_CreateThread(PR_SYSTEM_THREAD,
                                                    EmbeddedEventHandler,
                                                    (void*)initContext,
                                                    PR_PRIORITY_NORMAL,
                                                    PR_GLOBAL_THREAD,
                                                    PR_UNJOINABLE_THREAD,
                                                    0);
#else
    EmbeddedEventHandler((void *) initContext);
#endif
#if DEBUG_RAPTOR_CANVAS
	printf("InitEmbeddedEventHandler(%lx): Embedded Thread created...\n", initContext);
#endif
    while (initContext->initComplete == FALSE) {

        ::PR_Sleep(PR_INTERVAL_NO_WAIT);

        if (initContext->initFailCode != 0) {
	    ::ThrowExceptionToJava(initContext->env, errorMessages[initContext->initFailCode]);
	    return;
        }
    }
}

static nsEventStatus PR_CALLBACK
HandleRaptorEvent (nsGUIEvent *)
{
    nsEventStatus result = nsEventStatus_eIgnore;
    return result;
} // HandleRaptorEvent()


static void 
DispatchEvent (nsGUIEvent* eventPtr, nsIWidget* widgetPtr)
{
    // PENDING(mark): DO I NEED TO DO ANYTHING SPECIAL HERE FOR UNIX?????????
#ifdef XP_MAC
    // Enable Raptor background activity
    // Note: This will be moved to nsMacMessageSink very soon.
    // The application will not have to do it anymore.
    
    EventRecord * macEventPtr = (EventRecord *) eventPtr->nativeMsg;
    
    gMessageSink.DispatchOSEvent(*macEventPtr, FrontWindow());
    Repeater::DoRepeaters(*macEventPtr);
    
    if (macEventPtr->what == nullEvent)
        Repeater::DoIdlers(*macEventPtr);
    
#else
    nsEventStatus aStatus = nsEventStatus_eIgnore;
    
    widgetPtr->DispatchEvent(eventPtr, aStatus);
#endif
} // DispatchEvent()


const jint SHIFT_MASK		= 1 << 0;
const jint CTRL_MASK		= 1 << 1;
const jint META_MASK		= 1 << 2;
const jint ALT_MASK			= 1 << 3;
const jint BUTTON1_MASK		= 1 << 4;
const jint BUTTON2_MASK		= ALT_MASK;
const jint BUTTON3_MASK		= META_MASK;

#ifdef XP_MAC

static unsigned short
ConvertModifiersToMacModifiers (jint modifiers)
{
    unsigned short convertedModifiers = 0;
    
    if (modifiers & META_MASK)
        convertedModifiers |= cmdKey;
    if (modifiers & SHIFT_MASK)
        convertedModifiers |= shiftKey;
    if (modifiers & ALT_MASK)
        convertedModifiers |= optionKey;
    if (modifiers & CTRL_MASK)
        convertedModifiers |= controlKey;
    if (modifiers & BUTTON1_MASK)
        convertedModifiers |= btnState;
    
    // KAB alphaLock would be nice too...
    return convertedModifiers;
} // ConvertModifiersToMacModifiers()


static void
ConvertMouseCoordsToMacMouseCoords (jint windowPtr, jint x, jint y, Point& pt)
{
    GrafPtr	savePort;

    pt.h = (short) x;
    pt.v = (short) y;
    GetPort(&savePort);
    SetPort((WindowPtr) windowPtr);
    LocalToGlobal(&pt);
    SetPort(savePort);
} // ConvertMouseCoordsToMacMouseCoords()

#endif // ifdef XP_MAC


static void
ConvertModifiersTo_nsInputEvent (jint modifiers, nsInputEvent& event)
{
    event.isShift   = (modifiers & SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    event.isControl = (modifiers & CTRL_MASK) ? PR_TRUE : PR_FALSE;
    event.isAlt     = (modifiers & ALT_MASK) ? PR_TRUE : PR_FALSE;
#ifdef XP_MAC
    event.isCommand = (modifiers & META_MASK) ? PR_TRUE : PR_FALSE;
#endif
} // ConvertModifiersTo_nsInputEvent()



static unsigned long
ConvertKeys (jchar keyChar, jint keyCode)
{
    return 0;
} // ConvertKeys()



const jint MOUSE_FIRST 	  = 500;
const jint MOUSE_CLICKED  = MOUSE_FIRST;
const jint MOUSE_PRESSED  = 1 + MOUSE_FIRST; // Event.MOUSE_DOWN
const jint MOUSE_RELEASED = 2 + MOUSE_FIRST; // Event.MOUSE_UP
const jint RAPTOR_MOUSE_MOVED    = 3 + MOUSE_FIRST; // Event.MOUSE_MOVE
const jint MOUSE_ENTERED  = 4 + MOUSE_FIRST; //Event.MOUSE_ENTER
const jint MOUSE_EXITED   = 5 + MOUSE_FIRST; //Event.MOUSE_EXIT
const jint MOUSE_DRAGGED  = 6 + MOUSE_FIRST; //Event.MOUSE_DRAG



static PRUint32
ConvertMouseMessageTo_nsMouseMessage (jint message, jint modifiers, jint numClicks)
{
	if (RAPTOR_MOUSE_MOVED == message)
	{
		return NS_MOUSE_MOVE;
	}
	else if (MOUSE_CLICKED == message)
	{
		PRBool doubleClick = (numClicks & 1) ? PR_FALSE : PR_TRUE;

		if (modifiers & BUTTON1_MASK)
		{
			if (doubleClick)
				return NS_MOUSE_LEFT_DOUBLECLICK;
			return NS_MOUSE_LEFT_CLICK;
		}
		else if (modifiers & BUTTON2_MASK)
		{
			if (doubleClick)
				return NS_MOUSE_MIDDLE_DOUBLECLICK;
			return NS_MOUSE_MIDDLE_CLICK;
		}
		else if (modifiers & BUTTON3_MASK)
		{
			if (doubleClick)
				return NS_MOUSE_RIGHT_DOUBLECLICK;
			return NS_MOUSE_RIGHT_CLICK;
		}
	}
	else if ((MOUSE_PRESSED == message) || (MOUSE_DRAGGED == message))
	{
		if (modifiers & BUTTON1_MASK)
		{
			return NS_MOUSE_LEFT_BUTTON_DOWN;
		}
		else if (modifiers & BUTTON2_MASK)
		{
			return NS_MOUSE_MIDDLE_BUTTON_DOWN;
		}
		else if (modifiers & BUTTON3_MASK)
		{
			return NS_MOUSE_RIGHT_BUTTON_DOWN;
		}
	}
	else if (MOUSE_RELEASED == message)
	{
		if (modifiers & BUTTON1_MASK)
		{
			return NS_MOUSE_LEFT_BUTTON_UP;
		}
		else if (modifiers & BUTTON2_MASK)
		{
			return NS_MOUSE_MIDDLE_BUTTON_UP;
		}
		else if (modifiers & BUTTON3_MASK)
		{
			return NS_MOUSE_RIGHT_BUTTON_UP;
		}
	}
	else if (MOUSE_ENTERED == message)
	{
		return NS_MOUSE_ENTER;
	}
	else if (MOUSE_EXITED == message)
	{
		return NS_MOUSE_EXIT;
	}
	return NS_MOUSE_MOVE;
} // ConvertMouseMessageTo_nsMouseMessage()


#ifdef XP_MAC
static unsigned short
ConvertMouseMessageToMacMouseEvent (jint message)
{
	if (RAPTOR_MOUSE_MOVED == message)
	{
		return nullEvent;
	}
	else if (MOUSE_CLICKED == message)
	{
		return nullEvent;
	}
	else if ((MOUSE_PRESSED == message) || (MOUSE_DRAGGED == message))
	{
		return mouseDown;
	}
	else if (MOUSE_RELEASED == message)
	{
		return mouseUp;
	}
	return nullEvent;
} // ConvertMouseMessageToMacMouseEvent()
#endif


/*
 * JNI interfaces
 */

// I need this extern "C" for reasons I won't go into right now....
// - Mark
#ifdef XP_UNIX 
extern "C" {
#endif

/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorInitialize
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeInitialize(
    JNIEnv *env, 
    jobject obj, 
    jstring verifiedBinDirAbsolutePath)
{
	JNIEnv		*	pEnv = env;
	jobject			jobj = obj;
	static PRBool	gFirstTime = PR_TRUE;
	if (gFirstTime)
	{
        const char *nativePath = (const char *) env->GetStringUTFChars(verifiedBinDirAbsolutePath, 0);
        gBinDir = nativePath;
        
        NS_InitXPCOM(NULL, &gBinDir);
		NS_SetupRegistry();
        nsComponentManager::RegisterComponentLib(kSessionHistoryCID, NULL, 
                                                 NULL, APPSHELL_DLL, 
                                                 PR_FALSE, PR_FALSE);
        NS_AutoregisterComponents();
		gFirstTime = PR_FALSE;
	}
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeInitialize()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorTerminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeTerminate (
	JNIEnv	*	env,
	jobject		obj)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
#if DEBUG_RAPTOR_CANVAS
	printf("raptorTerminate() called\n");
#endif

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeTerminate()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellCreate
 * Signature: (IIIIII)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCreate (
	JNIEnv		*	env,
	jobject			obj,
	jint			windowPtr,
	jint			x,
	jint			y,
	jint			width,
	jint			height)
{
	jobject			jobj = obj;
#ifdef XP_MAC
	WindowPtr		pWindow = (WindowPtr) windowPtr;
	Rect			webRect = pWindow->portRect;
    // nsIWidget	*	pWidget = (nsIWidget *) widgetPtr;
#elif defined(XP_PC)
    // elif defined(XP_WIN)
	HWND parentHWnd = (HWND)windowPtr;
#elif defined(XP_UNIX)
    GtkWidget * parentHWnd = (GtkWidget *) windowPtr;
#endif

	if (parentHWnd == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null window handle passed to raptorWebShellCreate");
		return (jint) 0;
	}
	WebShellInitContext* initContext = new WebShellInitContext;

	initContext->initComplete = FALSE;
	initContext->initFailCode = 0;
	initContext->parentHWnd = parentHWnd;
	initContext->webShell = NULL;
	initContext->embeddedThread = NULL;
	initContext->actionQueue = NULL;
	initContext->env = env;
	initContext->stopThread = FALSE;
	initContext->x = x;
	initContext->y = y;
	initContext->w = width;
	initContext->h = height;

#ifdef XP_UNIX
    // This probably needs some explaining...
    // On Unix, instead of using a NSPR thread to manage our Mozilla event loop, we
    // use a Java Thread (for reasons I won't go into now, see 
    // org.mozilla.webclient.motif.MozillaEventThread for more details). This java thread will 
    // process any pending native events in 
    // org.mozilla.webclient.motif.MozillaEventThread.processNativeEventQueue()
    // (see below). However, when processNativeEventQueue() gets called, it needs access to it's associated
    // WebShellInitContext structure in order to process any GTK / Mozilla related events. So what we
    // do is we keep a static Hashtable inside the class MozillaEventThread to store mappings of
    // gtkWindowID's to WebShellInitContext's (since there is one WebShellInitContext to one GTK Window). 
    // This is what this part of the code does:
    //
    // 1) It first accesses the class org.mozilla.webclient.motif.MozillaEventThread
    // 2) It then accesses a static method in this class called "storeContext()" which takes as
    //    arguments, a GTK window pointer and a WebShellInitContext pointer
    // 3) Then it invokes the method
    // 4) Inside MozillaEventThread.run(), you can see the code which fetches the appropriate 
    //    WebShellInitContext and then calls processNativeEventQueue() with it below.

    // PENDING(mark): Should we cache this? Probably....
    jclass mozillaEventThreadClass = env->FindClass("org/mozilla/webclient/motif/MozillaEventThread");

    if (mozillaEventThreadClass == NULL) {
        printf("Error: Cannot find class org.mozilla.webclient.motif.MozillaEventThread!\n");

        ThrowExceptionToJava(env, "Error: Cannot find class org.mozilla.webclient.motif.MozillaEventThread!");
        return 0;
    }

    jmethodID storeMethodID = env->GetStaticMethodID(mozillaEventThreadClass, "storeContext", "(II)V");

    if (storeMethodID == NULL) {
        printf("Error: Cannot get static method storeContext(int, int) from MozillaEventThread!\n");

        ThrowExceptionToJava(env, "Error: Cannot get static method storeContext(int, int) from MozillaEventThread!");
        return 0;
    }

    // PENDING(mark): How do I catch this if there's an exception?
    env->CallStaticVoidMethod(mozillaEventThreadClass, storeMethodID, windowPtr, (int) initContext);
#else
	InitEmbeddedEventHandler(initContext);
#endif

	return (jint) initContext;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCreate()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellDelete
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellDelete (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

	if (webShellPtr)
	{
	    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

		// stop the event thread
		initContext->stopThread = 1;
		while (initContext->stopThread <= 1) {
			; // TODO there's got to be a better way than this.  ::PR_Interrupt()?
		}

		delete (initContext->webShell);
		// TODO delete the other things too (thread, action queue, etc.)
		delete initContext;
	}
	else {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellDelete");
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellDelete()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellLoadURL
 * Signature: (ILjava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellLoadURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jstring		urlString)
{
	jobject			jobj = obj;
   	const char	*	urlChars = (char *) env->GetStringUTFChars(urlString, 0);

	printf("Native URL = \"%s\"\n", urlChars);
	env->ReleaseStringUTFChars(urlString, urlChars);

   	PRUnichar	*	urlStringChars = (PRUnichar *) env->GetStringChars(urlString, 0);

	if (env->ExceptionOccurred())
	{
		::ThrowExceptionToJava(env, "raptorWebShellLoadURL Exception: unable to extract Java string");
		if (urlStringChars != NULL)
			env->ReleaseStringChars(urlString, urlStringChars);
		return;
	}

	WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellLoadURL");
		if (urlStringChars != NULL)
			env->ReleaseStringChars(urlString, urlStringChars);
		return;
	}

	if (initContext->initComplete) {
		wsLoadURLEvent	* actionEvent = new wsLoadURLEvent(initContext->webShell, urlStringChars);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
    }

	env->ReleaseStringChars(urlString, urlStringChars);
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellLoadURL()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellStop
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellStop (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellStop");
		return;
	}

	if (initContext->initComplete) {
		wsStopEvent		* actionEvent = new wsStopEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

        ::PostEvent(initContext, event);
	}
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellStop()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellShow
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellShow (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

	WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellShow");
		return;
	}

	if (initContext->initComplete) {
		wsShowEvent		* actionEvent = new wsShowEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

        ::PostEvent(initContext, event);
	}
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellShow()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellHide
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellHide (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellHide");
		return;
	}

	if (initContext->initComplete) {
		wsHideEvent		* actionEvent = new wsHideEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellHide()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellSetBounds
 * Signature: (IIIII)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetBounds (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y,
	jint		width,
	jint		height)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellSetBounds");
		return;
	}

	if (initContext->initComplete) {
		wsResizeEvent	* actionEvent = new wsResizeEvent(initContext->webShell, x, y, width, height);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetBounds()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellMoveTo
 * Signature: (III)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellMoveTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		x,
	jint		y)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellMoveTo");
		return;
	}

	if (initContext->initComplete) {
		wsMoveToEvent	* actionEvent = new wsMoveToEvent(initContext->webShell, x, y);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellMoveTo()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellSetFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellSetFocus");
		return;
	}

	if (initContext->initComplete) {
		wsSetFocusEvent	* actionEvent = new wsSetFocusEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellSetFocus()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellRemoveFocus
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRemoveFocus (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRemoveFocus");
		return;
	}

	if (initContext->initComplete) {
		wsRemoveFocusEvent	* actionEvent = new wsRemoveFocusEvent(initContext->webShell);
        PLEvent				* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRemoveFocus()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellRepaint
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRepaint (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jboolean	forceRepaint)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRepaint");
		return;
	}

	if (initContext->initComplete) {
		wsRepaintEvent	* actionEvent = new wsRepaintEvent(initContext->webShell, (PRBool) forceRepaint);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::PostEvent(initContext, event);
	}

} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRepaint()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellCanBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellCanBack");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsCanBackEvent	* actionEvent = new wsCanBackEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

        return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanBack()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellCanForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellCanForward");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsCanForwardEvent	* actionEvent = new wsCanForwardEvent(initContext->webShell);
        PLEvent				* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellCanForward()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellBack
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellBack");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsBackEvent	* actionEvent = new wsBackEvent(initContext->webShell);
        PLEvent	   	* event       = (PLEvent*) *actionEvent;

        voidResult = ::PostSynchronousEvent(initContext, event);

		return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack()

JNIEXPORT jboolean JNICALL 
    Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellAddDocListener 
 (JNIEnv *env, 
  jobject obj, 
  jint webShellPtr, 
  jobject doclistener) 
{
    
    // need to pass in the environment to the addition of c doclistener, so 
    // that it can call into the java stuff again
    DocumentObserver * documentObserver = new DocumentObserver(env, obj, webShellPtr, doclistener);
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellAddDocListener");
		return JNI_FALSE;
	}
    
	if (initContext->initComplete) {
        nsIDocumentLoader  *docloader;
        initContext->webShell->GetDocumentLoader(docloader );
        if (docloader != nsnull) {
            docloader->AddObserver((nsIDocumentLoaderObserver *) documentObserver);
        }
        
    }
    return JNI_TRUE;
}

/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellForward
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellForward (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellForward");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsForwardEvent	* actionEvent = new wsForwardEvent(initContext->webShell);
        PLEvent		   	* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellForward()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellGoTo
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGoTo (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
 	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGoTo");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsGoToEvent	* actionEvent = new wsGoToEvent(initContext->webShell, historyIndex);
        PLEvent	   	* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGoTo()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellGetHistoryLength
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryLength (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetHistoryLength");
		return 0;
	}

	if (initContext->initComplete) {
		wsGetHistoryLengthEvent	* actionEvent = new wsGetHistoryLengthEvent(initContext->webShell);
        PLEvent	   				* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		return ((jint) voidResult);
	}

	return 0;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryLength()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellGetHistoryIndex
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryIndex (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetHistoryIndex");
		return 0;
	}

	if (initContext->initComplete) {
		wsGetHistoryIndexEvent	* actionEvent = new wsGetHistoryIndexEvent(initContext->webShell);
        PLEvent	   				* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		return ((jint) voidResult);
	}

	return 0;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetHistoryIndex()


/*
 * Class:     BrowserControlNativeShim
 * Method:    raptorWebShellGetURL
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetURL (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr,
	jint		historyIndex)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;
	jstring		urlString = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetURL");
		return nsnull;
	}

	if (initContext->initComplete) {
		wsGetURLEvent	* actionEvent = new wsGetURLEvent(initContext->webShell, historyIndex);
        PLEvent	   	  	* event       = (PLEvent*) *actionEvent;

		voidResult = ::PostSynchronousEvent(initContext, event);

		if (voidResult != nsnull) {

#ifdef NECKO
			nsString * string = new nsString((PRUnichar *) voidResult);
#else
			nsString1 * string = new nsString1((PRUnichar *) voidResult);
#endif
			if (string == nsnull) {
				::ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: unable to create Java string");
				return nsnull;
			}

			int length = string->Length();

			delete string;

			urlString = env->NewString((PRUnichar *) voidResult, length);

			if (env->ExceptionOccurred())
			{
				::ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: unable to create Java string");
				return nsnull;
			}
		}
		else {
			::ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned NULL");
			return nsnull;
		}
	}

	return urlString;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellGetURL()

// added my Mark Goddard OTMP 9/2/1999
/*
 * Class:     org_mozilla_webclient_BrowserControlNativeShim
 * Method:    nativeWebShellRefresh
 * Signature: (I)V
 */
JNIEXPORT jboolean JNICALL 
    Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellRefresh (
	JNIEnv	*	env,
	jobject		obj,
	jint		webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRefresh");
		return JNI_FALSE;
	}

	if (initContext->initComplete) {
		wsRefreshEvent	* actionEvent = new wsRefreshEvent(initContext->webShell);
        PLEvent	   	* event       = (PLEvent*) *actionEvent;

        voidResult = ::PostSynchronousEvent(initContext, event);

		return (NS_FAILED((nsresult) voidResult)) ? JNI_FALSE : JNI_TRUE;
	}

	return JNI_FALSE;
} // Java_org_mozilla_webclient_BrowserControlNativeShim_nativeWebShellBack()


#ifdef XP_UNIX
/*
 * Class:     org_mozilla_webclient_motif_MotifMozillaEventQueue
 * Method:    processNativeEventQueue
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_motif_MozillaEventThread_processNativeEventQueue
    (JNIEnv * env, jobject obj, jint webShellInitContextPtr) {
    // Given a WebShellInitContext pointer, process any pending Mozilla / GTK events
    WebShellInitContext * initContext = (WebShellInitContext *) webShellInitContextPtr;

    // Initialize the WebShellInitContext if it's hasn't been already
    if (initContext->initComplete == FALSE) {
        InitEmbeddedEventHandler(initContext);
    }

    // Process any pending events...
    processEventLoop(initContext);
}
#endif

/*
 * Class:     org_mozilla_webclient_BrowserControlNativeShim
 * Method:    nativeDebugBreak
 * Signature: (Ljava/lang/String;I)V
 */
JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_BrowserControlNativeShim_nativeDebugBreak(JNIEnv *jEnv, 
                                                                      jclass myClass, 
                                                                      jstring fileName, 
                                                                      jint lineNumber)
{
    const char *charFileName = (char *) jEnv->GetStringUTFChars(fileName, 0);
    nsDebug::Break(charFileName, lineNumber);
}


#ifdef XP_UNIX
}// End extern "C"
#endif


void
PostEvent (WebShellInitContext * initContext, PLEvent * event)
{
        PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);

        ::PL_PostEvent(initContext->actionQueue, event);
        
        PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);
} // PostEvent()


void *
PostSynchronousEvent (WebShellInitContext * initContext, PLEvent * event)
{
        void    *       voidResult = nsnull;

        PL_ENTER_EVENT_QUEUE_MONITOR(initContext->actionQueue);

        voidResult = ::PL_PostSynchronousEvent(initContext->actionQueue, event);

        PL_EXIT_EVENT_QUEUE_MONITOR(initContext->actionQueue);

        return voidResult;
} // PostSynchronousEvent()          



// EOF
