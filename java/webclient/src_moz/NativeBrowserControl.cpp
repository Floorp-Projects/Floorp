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


NativeBrowserControl::NativeBrowserControl(void)
{
    parentHWnd = nsnull;
    mNavigation = nsnull;
    mSessionHistory = nsnull;
    mWindow = nsnull;
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
NativeBrowserControl::Init()
{

    // Create our embed window, and create an owning reference to it and
    // initialize it.  It is assumed that this window will be destroyed
    // when we go out of scope.
    mWindow = new EmbedWindow();
    mWindowGuard = NS_STATIC_CAST(nsIWebBrowserChrome *, mWindow);
    mWindow->Init(this);

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
    
    parentHWnd = nsnull;
}
    
