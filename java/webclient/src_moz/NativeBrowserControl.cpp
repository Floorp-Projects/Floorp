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
#include <nsIWindowWatcher.h> // for initializing our window watcher service

#include "EmbedWindow.h"
#include "WindowCreator.h"
#include "EmbedProgress.h"
#include "EmbedEventListener.h"
#include "NativeBrowserControl.h"
#include "ns_util.h"

// all of the crap that we need for event listeners
// and when chrome windows finish loading
#include <nsIDOMWindow.h>
#include <nsPIDOMWindow.h>
#include <nsIDOMWindowInternal.h>
#include <nsIChromeEventHandler.h>

NativeBrowserControl::NativeBrowserControl(void)
{
    parentHWnd = nsnull;
    mNavigation = nsnull;
    mSessionHistory = nsnull;
    mWindow = nsnull;
    mJavaBrowserControl = nsnull;
    mChromeMask       = 0;
    mIsChrome         = PR_FALSE;
    mChromeLoaded     = PR_FALSE;
    mIsDestroyed      = PR_FALSE;
    mListenersAttached = PR_FALSE;
}

NativeBrowserControl::~NativeBrowserControl()
{
    // PENDING(edburns): assert that this widget has been destroyed
    mChromeMask       = 0;
    mIsChrome         = PR_FALSE;
    mChromeLoaded     = PR_FALSE;
    mJavaBrowserControl = nsnull;
}

nsresult
NativeBrowserControl::Init(NativeWrapperFactory *yourWrapperFactory)
{

    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants();
    }

    wrapperFactory = yourWrapperFactory;

    // Create our embed window, and create an owning reference to it and
    // initialize it.  It is assumed that this window will be destroyed
    // when we go out of scope.
    mWindow = new EmbedWindow();
    mWindowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, mWindow);
    mWindow->Init(this);

    // Create our progress listener object, make an owning reference,
    // and initialize it.  It is assumed that this progress listener
    // will be destroyed when we go out of scope.
    mProgress = new EmbedProgress();
    mProgressGuard = NS_STATIC_CAST(nsIWebProgressListener *,
                                    mProgress);
    mProgress->Init(this);

    // Create our key listener object and initialize it.  It is assumed
    // that this will be destroyed before we go out of scope.
    mEventListener = new EmbedEventListener();
    mEventListenerGuard =
        NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(nsIDOMKeyListener *,
                                                     mEventListener));
    mEventListener->Init(this);
    
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // get a handle on the navigation object
    mNavigation = do_QueryInterface(webBrowser);
    
    
    //
    // create the WindowCreator: see
    WindowCreator *creator = new WindowCreator(this);
    nsCOMPtr<nsIWindowCreator> windowCreator;
    windowCreator = NS_STATIC_CAST(nsIWindowCreator *, creator);

    // Attach it via the watcher service
    nsCOMPtr<nsIWindowWatcher> watcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    if (watcher) {
        watcher->SetWindowCreator(windowCreator);
    }

    return NS_OK;
}

nsresult
NativeBrowserControl::Realize(jobject javaBrowserControl,
                              void *parentWinPtr, PRBool *aAlreadyRealized,
                              PRUint32 width, PRUint32 height)
{
    mJavaBrowserControl = javaBrowserControl;

    // Create our session history object and tell the navigation object
    // to use it.  We need to do this before we create the web browser
    // window.
    mSessionHistory = do_CreateInstance(NS_SHISTORY_CONTRACTID);
    mNavigation->SetSessionHistory(mSessionHistory);

#ifdef XP_UNIX
    PR_ASSERT(PR_FALSE);
    GtkWidget *ownerAsWidget (GTK_WIDGET(parentWinPtr));
    parentHWnd = ownerAsWidget;
    width = ownerAsWidget->allocation.width;
    height = ownerAsWidget->allocation.height;
#else 
    parentHWnd = (HWND) parentWinPtr;
#endif

    // create the window
    mWindow->CreateWindow_(width, height);

    // bind the progress listener to the browser object
    nsCOMPtr<nsISupportsWeakReference> supportsWeak;
    supportsWeak = do_QueryInterface(mProgressGuard);
    nsCOMPtr<nsIWeakReference> weakRef;
    supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
    mWindow->AddWebBrowserListener(weakRef,
                                   nsIWebProgressListener::GetIID());

    // set the eventRegistration into the progress listener
    jobject eventRegistration = 
        this->QueryInterfaceJava(EVENT_REGISTRATION_INDEX);
    if (nsnull != eventRegistration) {
        mProgress->SetEventRegistration(eventRegistration);
        mEventListener->SetEventRegistration(eventRegistration);
    }
    else {
        JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
        ::util_ThrowExceptionToJava(env, "Can't get EventRegistration from BrowserControl");
    }


    return NS_OK;
}

void
NativeBrowserControl::Unrealize(void)
{
}

void
NativeBrowserControl::Show(void)
{
    // Get the nsIWebBrowser object for our embedded window.
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // and set the visibility on the thing
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);
    baseWindow->SetVisibility(PR_TRUE);
}

void
NativeBrowserControl::Hide(void)
{
    // Get the nsIWebBrowser object for our embedded window.
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // and set the visibility on the thing
    nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webBrowser);
    baseWindow->SetVisibility(PR_FALSE);
}

void
NativeBrowserControl::Resize(PRUint32 x, PRUint32 y,
                             PRUint32 aWidth, PRUint32 aHeight)
{
    mWindow->SetDimensions(nsIEmbeddingSiteWindow::DIM_FLAGS_POSITION |
                           nsIEmbeddingSiteWindow::DIM_FLAGS_SIZE_INNER,
                           x, y, aWidth, aHeight);
}

void
NativeBrowserControl::Destroy(void)
{
    mIsDestroyed = PR_TRUE;
    // PENDING(edburns): take over the stuff from
    // WindowControlActionEvents
    // wsDeallocateInitContextEvent::handleEvent()

    // This flag might have been set from
    // EmbedWindow::DestroyBrowserWindow() as well if someone used a
    // window.close() or something or some other script action to close
    // the window.  No harm setting it again.
    
    // Get the nsIWebBrowser object for our embedded window.
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));

    // detach our event listeners and release the event receiver
    DetachListeners();
    if (mEventReceiver)
        mEventReceiver = nsnull;
    
    // destroy our child window
    mWindow->ReleaseChildren();
    
    // release navigation
    mNavigation = nsnull;

    // Release our progress listener
    nsCOMPtr<nsISupportsWeakReference> supportsWeak;
    supportsWeak = do_QueryInterface(mProgressGuard);
    nsCOMPtr<nsIWeakReference> weakRef;
    supportsWeak->GetWeakReference(getter_AddRefs(weakRef));
    webBrowser->RemoveWebBrowserListener(weakRef,
                                         nsIWebProgressListener::GetIID());
    weakRef = nsnull;
    supportsWeak = nsnull;
    
    // Now that we have removed the listener, release our progress
    // object
    mProgressGuard = nsnull;
    mProgress = nsnull;
    fflush(stdout);
    
    parentHWnd = nsnull;
}

NativeWrapperFactory *NativeBrowserControl::GetWrapperFactory()
{
    return wrapperFactory;
}

jobject NativeBrowserControl::QueryInterfaceJava(WEBCLIENT_INTERFACES interface)
{
    PR_ASSERT(nsnull != mJavaBrowserControl);
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    jobject result = nsnull;
    jstring interfaceJStr = ::util_NewStringUTF(env, 
                                                gImplementedInterfaces[interface]);
    
    jclass clazz = env->GetObjectClass(mJavaBrowserControl);
    jmethodID mid = env->GetMethodID(clazz, "queryInterface", 
                                     "(Ljava/lang/String;)Ljava/lang/Object;");
    if (nsnull != mid) {
        result = env->CallObjectMethod(mJavaBrowserControl, mid,
                                       interfaceJStr);
    }
    else {
        ::util_ThrowExceptionToJava(env, "Can't QueryInterface BrowserControl");
    }
    ::util_DeleteStringUTF(env, interfaceJStr);
    
    return result;
}

void
NativeBrowserControl::ContentStateChange(void)
{
    // we don't attach listeners to chrome
    if (mListenersAttached && !mIsChrome) {
        return;
    }
    
    GetListener();
    
    if (!mEventReceiver) {
        return;
    }
    
    AttachListeners();
    
}    

void 
NativeBrowserControl::GetListener()
{
    if (mEventReceiver) {
        return;
    }
    
    nsCOMPtr<nsPIDOMWindow> piWin;
    GetPIDOMWindow(getter_AddRefs(piWin));
    
    if (!piWin) {
        return;
    }
    
    nsCOMPtr<nsIChromeEventHandler> chromeHandler;
    piWin->GetChromeEventHandler(getter_AddRefs(chromeHandler));
    
    mEventReceiver = do_QueryInterface(chromeHandler);
    
}

void 
NativeBrowserControl::AttachListeners()
{
    if (!mEventReceiver || mListenersAttached) {
        return;
    }
    
    nsIDOMEventListener *eventListener =
        NS_STATIC_CAST(nsIDOMEventListener *,
                       NS_STATIC_CAST(nsIDOMKeyListener *, mEventListener));
    
    // add the key listener
    nsresult rv;
    rv = mEventReceiver->AddEventListenerByIID(eventListener,
                                               NS_GET_IID(nsIDOMKeyListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add key listener\n");
        return;
    }
    
    rv = mEventReceiver->AddEventListenerByIID(eventListener,
                                               NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to add mouse listener\n");
        return;
    }
    
    // ok, all set.
    mListenersAttached = PR_TRUE;
}

void 
NativeBrowserControl::DetachListeners()
{
    if (!mListenersAttached || !mEventReceiver) {
        return;
    }
    
    nsIDOMEventListener *eventListener =
        NS_STATIC_CAST(nsIDOMEventListener *,
                       NS_STATIC_CAST(nsIDOMKeyListener *, mEventListener));
    
    nsresult rv;
    rv = mEventReceiver->RemoveEventListenerByIID(eventListener,
                                                  NS_GET_IID(nsIDOMKeyListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to remove key listener\n");
        return;
    }
    
    rv =
        mEventReceiver->RemoveEventListenerByIID(eventListener,
                                                 NS_GET_IID(nsIDOMMouseListener));
    if (NS_FAILED(rv)) {
        NS_WARNING("Failed to remove mouse listener\n");
        return;
    }
    
    mListenersAttached = PR_FALSE;
    
}

nsresult
NativeBrowserControl::GetPIDOMWindow(nsPIDOMWindow **aPIWin)
{
    *aPIWin = nsnull;
    
    // get the web browser
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // get the content DOM window for that web browser
    nsCOMPtr<nsIDOMWindow> domWindow;
    webBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow) {
        return NS_ERROR_FAILURE;
    }
    
    // get the private DOM window
    nsCOMPtr<nsPIDOMWindow> domWindowPrivate = do_QueryInterface(domWindow);
    // and the root window for that DOM window
    nsCOMPtr<nsIDOMWindowInternal> rootWindow;
    domWindowPrivate->GetPrivateRoot(getter_AddRefs(rootWindow));
    
    nsCOMPtr<nsIChromeEventHandler> chromeHandler;
    nsCOMPtr<nsPIDOMWindow> piWin(do_QueryInterface(rootWindow));
    
    *aPIWin = piWin.get();
    
    if (*aPIWin) {
        NS_ADDREF(*aPIWin);
        return NS_OK;
    }
    
    return NS_ERROR_FAILURE;
    
}


