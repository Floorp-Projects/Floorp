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
 * nsActions.cpp
 */

#include "nsActions.h"
#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsIBaseWindow.h"
#include "nsISHEntry.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebNavigation.h"

#include "ns_util.h"

#include "nsEmbedAPI.h"  // for NS_TermEmbedding

void *          handleEvent     (PLEvent * event);
void            destroyEvent    (PLEvent * event);


void *
handleEvent (PLEvent * event)
{
        nsActionEvent   * actionEvent = (nsActionEvent*) event->owner;
        void            * result = nsnull;

        result = actionEvent->handleEvent();

        return result;
} // handleEvent()


void
destroyEvent (PLEvent * event)
{
        nsActionEvent * actionEvent = (nsActionEvent*) event->owner;

        if (actionEvent != nsnull) {
                actionEvent->destroyEvent();
        }
} // destroyEvent()



/*
 * nsActionEvent
 */

nsActionEvent::nsActionEvent ()
{
        PL_InitEvent(&mEvent, this,
                (PLHandleEventProc ) ::handleEvent,
                (PLDestroyEventProc) ::destroyEvent);
}

wsHistoryActionEvent::wsHistoryActionEvent(nsISHistory *yourHistory)
{
    mHistory = yourHistory;
}

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




/*
 * wsLoadURLEvent
 */

wsLoadURLEvent::wsLoadURLEvent(nsIWebNavigation* webNavigation, PRUnichar * urlString, PRInt32 urlLength) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
        mURL(nsnull)
{
        mURL = new nsString(urlString, urlLength);
}


void *
wsLoadURLEvent::handleEvent ()
{
  if (mWebNavigation && mURL) {
      nsresult rv = mWebNavigation->LoadURI(mURL->GetUnicode(), nsIWebNavigation::LOAD_FLAGS_NONE);
  }
  return nsnull;
} // handleEvent()


wsLoadURLEvent::~wsLoadURLEvent ()
{
  if (mURL != nsnull)
    delete mURL;
}



/*
 * wsStopEvent
 */

wsStopEvent::wsStopEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
        mWebNavigation(webNavigation)
{
}


void *
wsStopEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Stop();
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
 * wsCanBackEvent
 */

wsCanBackEvent::wsCanBackEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
	mWebNavigation(webNavigation)
{

}

void *
wsCanBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv;
        PRBool canGoBack;
        
        rv = mWebNavigation->GetCanGoBack(&canGoBack);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *)canGoBack;
    }
    return result;
} // handleEvent()

/*
 * wsCanForwardEvent
 */

wsCanForwardEvent::wsCanForwardEvent(nsIWebNavigation* webNavigation) :
        nsActionEvent(),
	mWebNavigation(webNavigation)
{

}


void *
wsCanForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv;
        PRBool canGoForward;
        
        rv = mWebNavigation->GetCanGoForward(&canGoForward);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *)canGoForward;
        
    }
    return result;
} // handleEvent()



/*
 * wsBackEvent
 */

wsBackEvent::wsBackEvent(nsIWebNavigation* webNavigation) :
  nsActionEvent(),
  mWebNavigation(webNavigation)
{
}


void *
wsBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
        nsresult rv = mWebNavigation->GoBack();
        
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsForwardEvent
 */

wsForwardEvent::wsForwardEvent(nsIWebNavigation* webNavigation) :
  nsActionEvent(),
  mWebNavigation(webNavigation)     
{
}


void *
wsForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mWebNavigation) {
                
        nsresult rv = mWebNavigation->GoForward();
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGoToEvent
 */

wsGoToEvent::wsGoToEvent(nsIWebNavigation* webNavigation, PRInt32 historyIndex) :
        nsActionEvent(),
        mWebNavigation(webNavigation), mHistoryIndex(historyIndex)
{
}


void *
wsGoToEvent::handleEvent ()
{
    void *result = nsnull;
    nsresult rv = nsnull;
    if (mWebNavigation) {
      //PENDING (Ashu) : GoTo Functionality seems to be missing in M15
      //        nsresult rv = mHistory->Goto(mHistoryIndex, mWebShell, PR_TRUE);
      result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryLengthEvent
 */

wsGetHistoryLengthEvent::wsGetHistoryLengthEvent(nsISHistory * sHistory) :
     nsActionEvent(),
     mHistory(sHistory)
{
}


void *
wsGetHistoryLengthEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyLength = 0;
        
        nsresult rv = mHistory->GetCount(&historyLength);
        result = (void *) historyLength;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryIndexEvent
 */

wsGetHistoryIndexEvent::wsGetHistoryIndexEvent(nsISHistory * sHistory) :
        nsActionEvent(),
	mHistory(sHistory)
{
}


void *
wsGetHistoryIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyIndex = 0;

        nsresult rv = mHistory->GetIndex(&historyIndex);
        result = (void *) historyIndex;
    }
    return result;
} // handleEvent()



/*
 * wsGetURLEvent
 */

wsGetURLEvent::wsGetURLEvent(nsISHistory * sHistory) :
        nsActionEvent(),
	mHistory(sHistory)
{
}


void *
wsGetURLEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        
        
        PRInt32 currentIndex;
        char *currentURL = nsnull;
        nsresult rv;

        rv = mHistory->GetIndex(&currentIndex);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        nsISHEntry * Entry;
        rv = mHistory->GetEntryAtIndex(currentIndex, PR_FALSE, &Entry);

        if (NS_FAILED(rv)) {
            return result;
        }

	nsIURI * URI;
	rv = Entry->GetURI(&URI);

	if (NS_FAILED(rv)) {
            return result;
        }
	
	rv = URI->GetSpec(&currentURL);
	if (NS_FAILED(rv)) {
            return result;
        }
        
        result = (void *) currentURL;
    }
    return result;
} // handleEvent()

/*
 * wsGetURLForIndexEvent
 */

wsGetURLForIndexEvent::wsGetURLForIndexEvent(nsISHistory * sHistory, 
                                             PRInt32 historyIndex) :
  nsActionEvent(),
  mHistory(sHistory),
  mHistoryIndex(historyIndex)
{
}


void *
wsGetURLForIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        nsresult rv;
        char *indexURL = nsnull;

	nsISHEntry * Entry;
        rv = mHistory->GetEntryAtIndex(mHistoryIndex, PR_FALSE, &Entry);
        if (NS_FAILED(rv)) {
            return result;
        }

	nsIURI * URI;
	rv = Entry->GetURI(&URI);

	if (NS_FAILED(rv)) {
            return result;
        }
	
	rv = URI->GetSpec(&indexURL);
	if (NS_FAILED(rv)) {
            return result;
        }	

        result = (void *) indexURL;
    }
    return result;
} // handleEvent()



// Added by Mark Goddard OTMP 9/2/1999

/*
 * wsRefreshEvent
 */

wsRefreshEvent::wsRefreshEvent(nsIWebNavigation* webNavigation, PRInt32 reloadType) :
        nsActionEvent(),
        mWebNavigation(webNavigation),
	mReloadType(reloadType)
{

}


void *
wsRefreshEvent::handleEvent ()
{
        if (mWebNavigation) {
                nsresult rv = mWebNavigation->Reload(mReloadType);
                return (void *) rv;
        }
        return nsnull;
} // handleEvent()



wsViewSourceEvent::wsViewSourceEvent(nsIDocShell* docShell, PRBool viewMode) :
    nsActionEvent(),
    mDocShell(docShell),
    mViewMode(viewMode)
{
}

void *
wsViewSourceEvent::handleEvent ()
{
    if(mDocShell) {
        if(mViewMode) {
            nsresult rv = mDocShell->SetViewMode(nsIDocShell::viewSource);
            return (void *) rv;
        }
        else
            {
                nsresult rv = mDocShell->SetViewMode(nsIDocShell::viewNormal);
                return (void *) rv;
            }
    }
    return nsnull;
}

wsAddDocLoaderObserverEvent::wsAddDocLoaderObserverEvent(nsIDocShell* yourDocShell,
                                                         nsIDocumentLoaderObserver *yourObserver) :
        nsActionEvent(),
        mDocShell(yourDocShell), mDocObserver(yourObserver)
{
}

void *
wsAddDocLoaderObserverEvent::handleEvent ()
{
    void *result = nsnull;
    
    if (mDocShell && mDocObserver) {
        
        nsresult rv = mDocShell->SetDocLoaderObserver(mDocObserver);
        result = (void *) rv;
    }
    return result;
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

    mInitContext->docShell->SetDocLoaderObserver(nsnull);

    mInitContext->docShell = nsnull;

    // PENDING(edburns): this is a leak.  For some reason, webShell's
    // refcount is two.  I believe it should be one.
    // see http://bugzilla.mozilla.org/show_bug.cgi?id=38271

    mInitContext->webShell = nsnull;
    mInitContext->webNavigation = nsnull;
    mInitContext->presShell = nsnull;
    mInitContext->sHistory = nsnull;
    mInitContext->baseWindow = nsnull;

    mInitContext->embeddedThread = nsnull;
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
    mInitContext->searchContext = nsnull;

    // make sure we aren't listening anymore.  This needs to be done
    // before currentDocument = nsnull
    mInitContext->browserContainer->RemoveAllListeners();
    
    mInitContext->currentDocument = nsnull;
    mInitContext->browserContainer = nsnull;
    util_DeallocateShareInitContext(&(mInitContext->shareContext));

    //  delete mInitContext;
    NS_TermEmbedding();
    return (void *) NS_OK;
} // handleEvent()


// EOF

