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
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ed Burns <edburns@acm.org>
 */

/*
 * NativeEventThreadActionEvents.cpp
 */

#include "NativeEventThreadActionEvents.h"

#include "nsIInterfaceRequestorUtils.h"

static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

static NS_DEFINE_IID(kISHistoryIID, NS_ISHISTORY_IID);
static NS_DEFINE_CID(kSHistoryCID, NS_SHISTORY_CID);

extern NativeBrowserControl* gNewWindowInitContext;

/*
 * wsRealizeBrowserEvent
 */

wsRealizeBrowserEvent::wsRealizeBrowserEvent(NativeBrowserControl * yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsRealizeBrowserEvent::handleEvent ()
{
  nsresult rv = nsnull;

    nsCOMPtr<nsIWebBrowser> webBrowser = nsnull;
    webBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
    
    mInitContext->webBrowser = webBrowser;
    
    // Get the BaseWindow from the DocShell - upcast
    //  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(webBrowser));
	nsCOMPtr<nsIBaseWindow> docShellAsWin;
	rv = webBrowser->QueryInterface(NS_GET_IID(nsIBaseWindow), getter_AddRefs(docShellAsWin));
    
    mInitContext->baseWindow = docShellAsWin;
    
    printf ("Init the baseWindow\n");
    
#ifdef XP_UNIX
    GtkWidget * bin;
    bin = (GtkWidget *) mInitContext->gtkWinPtr;
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - before Init Call...\n", mInitContext));
    }
    rv = mInitContext->baseWindow->InitWindow((nativeWindow) bin, nsnull, mInitContext->x, mInitContext->y, mInitContext->w, mInitContext->h);
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, ("Ashu Debugs - Inside InitMozillaStuff(%lx): - after Init Call...\n", mInitContext));
    }
#else    
    rv = mInitContext->baseWindow->InitWindow((nativeWindow) mInitContext->parentHWnd, nsnull,
                                             mInitContext->x, mInitContext->y, mInitContext->w, mInitContext->h);
#endif
    
    printf("Create the BaseWindow...\n");
    
    rv = mInitContext->baseWindow->Create();
    
    if (NS_FAILED(rv)) {
        mInitContext->initFailCode = kInitWebShellError;
        return (void *) rv;
    }
    
    // Create the DocShell
    
    mInitContext->docShell = do_GetInterface(mInitContext->webBrowser);
    
    if (!mInitContext->docShell) {
        mInitContext->initFailCode = kCreateDocShellError;
        return (void *) rv;
    }
    
    // create our BrowserContainer, which implements many many things.
    CBrowserContainer *browserContainer;
    
    mInitContext->browserContainer = browserContainer = 
        new CBrowserContainer(mInitContext->webBrowser, mInitContext->env, 
                              mInitContext);
    
    // set the WebShellContainer.  This is a pain.  It's necessary
    // because nsWebShell.cpp still checks for mContainer all over the
    // place.
    nsCOMPtr<nsIWebShellContainer> wsContainer(do_QueryInterface(mInitContext->browserContainer));
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mInitContext->docShell));
    webShell->SetContainer(wsContainer);
    
    // set the TreeOwner
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mInitContext->docShell));
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner(do_QueryInterface(mInitContext->browserContainer));
    docShellAsItem->SetTreeOwner(treeOwner);
    
    // set the docloaderobserver PENDING(edburns): how to we make our
    // presence as a nsIWebProgressListener know?n
    nsWeakPtr weakling(
                       dont_AddRef(NS_GetWeakReference(NS_STATIC_CAST(nsIWebProgressListener*, browserContainer))));
    webBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));
    

	printf("Creation Done.....\n");
    // Get the WebNavigation Object from the DocShell
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mInitContext->docShell));
    mInitContext->webNavigation = webNav;

    if (nsnull == gHistory) {
        rv = webNav->GetSessionHistory((nsISHistory **)&gHistory);
        if (NS_FAILED(rv)) {
            mInitContext->initFailCode = kHistoryWebShellError;
            return (void *) rv;
        }
    }
    
    
    // Set the History
    //    mInitContext->webNavigation->SetSessionHistory(gHistory);
    
    // Save the sessionHistory in the initContext
    //    mInitContext->sHistory = gHistory;
    
    printf("Show the webBrowser\n");
    // Show the webBrowser
    rv = mInitContext->baseWindow->SetVisibility(PR_TRUE);
    if (NS_FAILED(rv)) {
        mInitContext->initFailCode = kShowWebShellError;
        return (void *) rv;
    }
    
    mInitContext->initComplete = TRUE;  

    // we will check this value in WindowCreator::CreateChromeWindow
    gNewWindowInitContext = mInitContext;
    return (void *) nsnull;

} // handleEvent()

wsRealizeBrowserEvent::~wsRealizeBrowserEvent ()
{
}
