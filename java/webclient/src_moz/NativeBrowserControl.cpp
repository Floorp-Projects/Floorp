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

#include "nsIEventQueueService.h" // for PLEventQueue
#include "nsIServiceManager.h" // for do_GetService
#include "nsEmbedAPI.h" // for NS_HandleEmbeddingEvent

#include "EmbedWindow.h"
#include "NativeBrowserControl.h"
#include "ns_util.h"

#ifdef XP_UNIX
#include <unistd.h>
#include "gdksuperwin.h"
#include "gtkmozarea.h"

extern "C" {
    static int	    wc_x_error	 (Display     *display, 
				  XErrorEvent *error);
}

#endif


PLEventQueue	*NativeBrowserControl::sActionQueue        = nsnull;
PRThread	*NativeBrowserControl::sEmbeddedThread     = nsnull;
PRBool           NativeBrowserControl::sInitComplete       = PR_FALSE;

NativeBrowserControl::NativeBrowserControl(void)
{
    parentHWnd = nsnull;
    mNavigation = nsnull;
    mSessionHistory = nsnull;
    mWindow = nsnull;
    mEnv = nsnull;
    mNativeEventThread = nsnull;
    mChromeMask       = 0;
    mIsChrome         = PR_FALSE;
    mChromeLoaded     = PR_FALSE;
    mIsDestroyed      = PR_FALSE;
}

NativeBrowserControl::~NativeBrowserControl()
{
    // PENDING(edburns): assert that this widget has been destroyed
    mChromeMask       = 0;
    mIsChrome         = PR_FALSE;
    mChromeLoaded     = PR_FALSE;
}

nsresult
NativeBrowserControl::Init(JNIEnv * env, jobject newNativeEventThread)
{
    mFailureCode = NS_ERROR_FAILURE;

    //
    // Do java communication initialization
    // 
    mEnv = env;
    // store the java NativeEventThread class
    mNativeEventThread = ::util_NewGlobalRef(env, newNativeEventThread);
    // This enables the listener to call back into java

    util_InitializeShareInitContext(mEnv, &mShareContext);
    
    //
    // create the singleton event queue
    // 

    // create the static sActionQueue
    if (nsnull == sEmbeddedThread) {
        nsCOMPtr<nsIEventQueueService> 
            aEventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);
        
        if (!aEventQService) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        // Create the event queue.
        mFailureCode = aEventQService->CreateThreadEventQueue();
        sEmbeddedThread = PR_GetCurrentThread();
        
        if (!sEmbeddedThread) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: Create UI Thread EventQueue: %d\n",
                mFailureCode));
        
        // We need to do something different for Unix
        nsIEventQueue * EQueue = nsnull;
        
        mFailureCode = aEventQService->GetThreadEventQueue(sEmbeddedThread, &EQueue);
        if (NS_FAILED(mFailureCode)) {
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: Get UI Thread EventQueue: %d\n",
                mFailureCode));
        
#ifdef XP_UNIX
        gdk_input_add(EQueue->GetEventQueueSelectFD(),
                      GDK_INPUT_READ,
                      event_processor_callback,
                      EQueue);
#endif
        
        PLEventQueue * plEventQueue = nsnull;
        
        EQueue->GetPLEventQueue(&plEventQueue);
        sActionQueue = plEventQueue;
        if (!sActionQueue) {
            mFailureCode = NS_ERROR_FAILURE;
            return mFailureCode;
        }
        
        PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
               ("NativeBrowserControl_Init: get ActionQueue: %d\n",
                mFailureCode));
        
#ifdef XP_UNIX
        
        // The gdk_x_error function exits in some cases, we don't 
        // want that.
        XSetErrorHandler(wc_x_error);
#endif
        sInitComplete = PR_TRUE;
    }

    // Create our embed window, and create an owning reference to it and
    // initialize it.  It is assumed that this window will be destroyed
    // when we go out of scope.
    mWindow = new EmbedWindow();
    mWindowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, mWindow);
    mWindow->Init(this);
    
    //
    // create the WindowCreator: see
    // NativeEventThread->InitializeWindowCreator
    //


    return NS_OK;
}

nsresult
NativeBrowserControl::Realize(void *parentWinPtr, PRBool *aAlreadyRealized)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void
NativeBrowserControl::Unrealize(void)
{
}

void
NativeBrowserControl::Show(void)
{
}

void
NativeBrowserControl::Hide(void)
{
}

void
NativeBrowserControl::Resize(PRUint32 aWidth, PRUint32 aHeight)
{
}

void
NativeBrowserControl::Destroy(void)
{
    mIsDestroyed = PR_TRUE;
    if (nsnull != mNativeEventThread) {
        ::util_DeleteGlobalRef(mEnv, mNativeEventThread);
    }

    // PENDING(edburns): take over the stuff from
    // WindowControlActionEvents
    // wsDeallocateInitContextEvent::handleEvent()
}
    
PRUint32 
NativeBrowserControl::ProcessEventLoop(void)
{
    if (PR_GetCurrentThread() != sEmbeddedThread) {
        return 0;
    }
    
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
    
    if (sInitComplete && sActionQueue) {
        PLEvent * event = nsnull;
        
        PL_ENTER_EVENT_QUEUE_MONITOR(sActionQueue);
        if (::PL_EventAvailable(sActionQueue)) {
            event = ::PL_GetEvent(sActionQueue);
        }
        PL_EXIT_EVENT_QUEUE_MONITOR(sActionQueue);
        if (event != nsnull) {
            ::PL_HandleEvent(event);
        }
    }

    // PENDING(edburns): revisit this.  Not sure why this is necessary, but
    // this fixes bug 44327
    //    printf("%c", 8); // 8 is ASCII for backspace

    return 1;
}

nsresult
NativeBrowserControl::GetFailureCode(void)
{
    return mFailureCode;
}

/* static */
PRBool
NativeBrowserControl::IsInitialized(void)
{
    return sInitComplete;
}

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
