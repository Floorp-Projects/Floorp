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
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

/*
 * WindowControlActionEvents.cpp
 */

#include "WindowControlActionEvents.h"

#include "ns_util.h"
#include "nsIDocShellTreeItem.h"
#include "nsEmbedAPI.h"  // for NS_TermEmbedding

#ifdef _WIN32
#include <stdlib.h>
#endif


/*
 * wsResizeEvent
 */

wsResizeEvent::wsResizeEvent(nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mLeft(x),
        mBottom(y),
        mWidth(w),
        mHeight(h)
{
}


void *
wsResizeEvent::handleEvent ()
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mBaseWindow) {

        rv = mBaseWindow->SetPositionAndSize(mLeft, mBottom, mWidth, mHeight, 
                                            PR_TRUE);
        
        
        return (void *) rv;
    }
    return nsnull;
} // handleEvent()


wsDeallocateInitContextEvent::wsDeallocateInitContextEvent(WebShellInitContext* yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsDeallocateInitContextEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    PRBool isLastWindow = PR_TRUE;
    PRInt32 winCount;
    if (NS_SUCCEEDED(mInitContext->browserContainer->
                     GetInstanceCount(&winCount))) {
        isLastWindow = (1 == winCount);
    }

    mInitContext->parentHWnd = nsnull;

    // make sure the webBrowser's contentListener doesn't have a ref to
    // CBrowserContainer
    mInitContext->webBrowser->SetParentURIContentListener(nsnull);
    mInitContext->webBrowser = nsnull;

    // make sure the webShell's container is set to null.
    // get the webShell from the docShell
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(mInitContext->docShell));
    if (webShell) {
        webShell->SetContainer(nsnull);
    }

    // make sure the docShell's TreeOwner is set to null
    // get the docShellTreeItem from the docShell
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mInitContext->docShell));
    if (docShellAsItem) {
        docShellAsItem->SetTreeOwner(nsnull);
    }

    mInitContext->docShell = nsnull;

    // PENDING(edburns): this is a leak.  For some reason, webShell's
    // refcount is two.  I believe it should be one.
    // see http://bugzilla.mozilla.org/show_bug.cgi?id=38271

    mInitContext->webShell = nsnull;
    mInitContext->webNavigation = nsnull;
    mInitContext->presShell = nsnull;
    mInitContext->baseWindow = nsnull;

    //    mInitContext->embeddedThread = nsnull;
    mInitContext->env = nsnull;
    if (nsnull != mInitContext->nativeEventThread) {
        ::util_DeleteGlobalRef((JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION), 
                               mInitContext->nativeEventThread);
        mInitContext->nativeEventThread = nsnull;
    }
    mInitContext->stopThread = -1;
    mInitContext->initComplete = FALSE;
    mInitContext->initFailCode = 0;
    mInitContext->x = -1;
    mInitContext->y = -1;
    mInitContext->w = -1;
    mInitContext->h = -1;    
    mInitContext->gtkWinPtr = nsnull;

    // make sure we aren't listening anymore.  This needs to be done
    // before currentDocument = nsnull
    mInitContext->browserContainer->RemoveAllListeners();
    
    mInitContext->currentDocument = nsnull;
    mInitContext->browserContainer = nsnull;
    util_DeallocateShareInitContext(&(mInitContext->shareContext));

    //  delete mInitContext;
    if (isLastWindow) {
        NS_TermEmbedding();
#ifdef _WIN32
        exit(0);
#endif
    }
    return (void *) NS_OK;
} // handleEvent()


/*
 * wsMoveToEvent
 */

wsMoveToEvent::wsMoveToEvent(nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mX(x),
        mY(y)
{
}


void *
wsMoveToEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->SetPosition(mX, mY);
        }
        return nsnull;
} // handleEvent()


/*
 * wsSetFocusEvent
 */

wsSetFocusEvent::wsSetFocusEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsSetFocusEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->SetFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRemoveFocusEvent
 */

wsRemoveFocusEvent::wsRemoveFocusEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsRemoveFocusEvent::handleEvent ()
{
        if (mBaseWindow) {
	  //PENDING (Ashu) : No removeFocus functionality provided in M15
	  //                nsresult rv = mWebShell->RemoveFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRepaintEvent
 */

wsRepaintEvent::wsRepaintEvent(nsIBaseWindow* baseWindow, PRBool forceRepaint) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
        mForceRepaint(forceRepaint)
{
}


void *
wsRepaintEvent::handleEvent ()
{
        if (mBaseWindow) {
               nsresult rv = mBaseWindow->Repaint(mForceRepaint);
        }
        return nsnull;
} // handleEvent()




/*
 * wsShowEvent
 */

wsShowEvent::wsShowEvent(nsIBaseWindow* baseWindow, PRBool state) :
        nsActionEvent(),
        mBaseWindow(baseWindow),
	mState(state)
{
}


void *
wsShowEvent::handleEvent ()
{
        if (mBaseWindow) {
                mBaseWindow->SetVisibility(mState);
        }
        return nsnull;
} // handleEvent()




/*
 * wsHideEvent
 */

wsHideEvent::wsHideEvent(nsIBaseWindow* baseWindow) :
        nsActionEvent(),
        mBaseWindow(baseWindow)
{
}


void *
wsHideEvent::handleEvent ()
{
        if (mBaseWindow) {
                mBaseWindow->SetVisibility(PR_FALSE);
        }
        return nsnull;
} // handleEvent()


