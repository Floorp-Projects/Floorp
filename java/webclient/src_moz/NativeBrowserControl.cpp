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
#include "EmbedProgress.h"
#include "NativeBrowserControl.h"
#include "ns_util.h"


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
NativeBrowserControl::Init()
{

    if (!util_StringConstantsAreInitialized()) {
        util_InitStringConstants();
    }

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


    nsCOMPtr<nsIWebBrowser> webBrowser;
    mWindow->GetWebBrowser(getter_AddRefs(webBrowser));
    
    // get a handle on the navigation object
    mNavigation = do_QueryInterface(webBrowser);
    
    
    //
    // create the WindowCreator: see
    // NativeEventThread->InitializeWindowCreator
    //


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
    
    parentHWnd = nsnull;
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
