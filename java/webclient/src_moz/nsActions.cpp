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
 */

/*
 * nsActions.cpp
 */

#include "nsActions.h"
#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsIBaseWindow.h"


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

wsHistoryActionEvent::wsHistoryActionEvent(nsISessionHistory *yourHistory)
{
    mHistory = yourHistory;
}

/*
 * wsResizeEvent
 */

wsResizeEvent::wsResizeEvent(nsIWebShell* webShell, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h) :
        nsActionEvent(),
        mWebShell(webShell),
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
    if (mWebShell) {
        
        nsCOMPtr<nsIBaseWindow> baseWindow;
        
        rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                       getter_AddRefs(baseWindow));
        
        if (NS_FAILED(rv)) {
            return nsnull;
        }
        rv = baseWindow->SetPositionAndSize(mLeft, mBottom, mWidth, mHeight, 
                                            PR_TRUE);
        
        
        return (void *) rv;
    }
    return nsnull;
} // handleEvent()




/*
 * wsLoadURLEvent
 */

wsLoadURLEvent::wsLoadURLEvent(nsIWebShell* webShell, PRUnichar * urlString) :
        nsActionEvent(),
        mWebShell(webShell),
        mURL(nsnull)
{
        mURL = new nsString(urlString);
}


void *
wsLoadURLEvent::handleEvent ()
{
        if (mWebShell && mURL) {
                
                nsresult rv = mWebShell->LoadURL(mURL->GetUnicode());
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

wsStopEvent::wsStopEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsStopEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv = mWebShell->Stop();
        }
        return nsnull;
} // handleEvent()



/*
 * wsShowEvent
 */

wsShowEvent::wsShowEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsShowEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv;
                nsCOMPtr<nsIBaseWindow> baseWindow;
                
                rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
                
                if (NS_FAILED(rv)) {
                    return nsnull;
                }
                baseWindow->SetVisibility(PR_TRUE);
        }
        return nsnull;
} // handleEvent()



/*
 * wsHideEvent
 */

wsHideEvent::wsHideEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsHideEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv;
                nsCOMPtr<nsIBaseWindow> baseWindow;
                
                rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
                
                if (NS_FAILED(rv)) {
                    return nsnull;
                }
                baseWindow->SetVisibility(PR_FALSE);
        }
        return nsnull;
} // handleEvent()



/*
 * wsMoveToEvent
 */

wsMoveToEvent::wsMoveToEvent(nsIWebShell* webShell, PRInt32 x, PRInt32 y) :
        nsActionEvent(),
        mWebShell(webShell),
        mX(x),
        mY(y)
{
}


void *
wsMoveToEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv;
                nsCOMPtr<nsIBaseWindow> baseWindow;
                
                rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
                
                if (NS_FAILED(rv)) {
                    return nsnull;
                }
                rv = baseWindow->SetPosition(mX, mY);
        }
        return nsnull;
} // handleEvent()


/*
 * wsSetFocusEvent
 */

wsSetFocusEvent::wsSetFocusEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsSetFocusEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv;
                nsCOMPtr<nsIBaseWindow> baseWindow;
                
                rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
                
                if (NS_FAILED(rv)) {
                    return nsnull;
                }
                rv = baseWindow->SetFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRemoveFocusEvent
 */

wsRemoveFocusEvent::wsRemoveFocusEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsRemoveFocusEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv = mWebShell->RemoveFocus();
        }
        return nsnull;
} // handleEvent()



/*
 * wsRepaintEvent
 */

wsRepaintEvent::wsRepaintEvent(nsIWebShell* webShell, PRBool forceRepaint) :
        nsActionEvent(),
        mWebShell(webShell),
        mForceRepaint(forceRepaint)
{
}


void *
wsRepaintEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv;
                nsCOMPtr<nsIBaseWindow> baseWindow;
                
                rv = mWebShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
                
                if (NS_FAILED(rv)) {
                    return nsnull;
                }
                rv = baseWindow->Repaint(mForceRepaint);
        }
        return nsnull;
} // handleEvent()



/*
 * wsCanBackEvent
 */

wsCanBackEvent::wsCanBackEvent(nsISessionHistory* yourSessionHistory) :
        wsHistoryActionEvent(yourSessionHistory)
{

}

void *
wsCanBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        nsresult rv;
        PRBool canGoBack;
        
        rv = mHistory->CanGoBack(&canGoBack);
        
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

wsCanForwardEvent::wsCanForwardEvent(nsISessionHistory* yourSessionHistory) :
        wsHistoryActionEvent(yourSessionHistory)
{
}


void *
wsCanForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        nsresult rv;
        PRBool canGoForward;
        
        rv = mHistory->CanGoForward(&canGoForward);
        
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

wsBackEvent::wsBackEvent(nsISessionHistory* yourSessionHistory, 
                         nsIWebShell *yourWebShell) :
    wsHistoryActionEvent(yourSessionHistory), mWebShell(yourWebShell)
{
}


void *
wsBackEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory && mWebShell) {
        nsresult rv = mHistory->GoBack(mWebShell);
        
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsForwardEvent
 */

wsForwardEvent::wsForwardEvent(nsISessionHistory *yourSessionHistory,
                               nsIWebShell* webShell) :
        wsHistoryActionEvent(yourSessionHistory),
        mWebShell(webShell)
{
}


void *
wsForwardEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory && mWebShell) {
                
        nsresult rv = mHistory->GoForward(mWebShell);
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGoToEvent
 */

wsGoToEvent::wsGoToEvent(nsISessionHistory *yourSessionHistory,
                         nsIWebShell* webShell, PRInt32 historyIndex) :
        wsHistoryActionEvent(yourSessionHistory),
        mWebShell(webShell), mHistoryIndex(historyIndex)
{
}


void *
wsGoToEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory && mWebShell) {
        nsresult rv = mHistory->Goto(mHistoryIndex, mWebShell, PR_TRUE);
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryLengthEvent
 */

wsGetHistoryLengthEvent::wsGetHistoryLengthEvent(nsISessionHistory* yourSessionHistory) :
    wsHistoryActionEvent(yourSessionHistory)
{
}


void *
wsGetHistoryLengthEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyLength = 0;
        
        nsresult rv = mHistory->GetHistoryLength(&historyLength);
        result = (void *) historyLength;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryIndexEvent
 */

wsGetHistoryIndexEvent::wsGetHistoryIndexEvent(nsISessionHistory *yourSessionHistory) :
        wsHistoryActionEvent(yourSessionHistory)
{
}


void *
wsGetHistoryIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mHistory) {
        PRInt32 historyIndex = 0;

        nsresult rv = mHistory->GetCurrentIndex(&historyIndex);
        result = (void *) historyIndex;
    }
    return result;
} // handleEvent()



/*
 * wsGetURLEvent
 */

wsGetURLEvent::wsGetURLEvent(nsISessionHistory* yourSessionHistory) :
        wsHistoryActionEvent(yourSessionHistory)
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

        rv = mHistory->GetCurrentIndex(&currentIndex);
        
        if (NS_FAILED(rv)) {
            return result;
        }
        
        // THIS STRING NEEDS TO BE deleted!!!!!!
        rv = mHistory->GetURLForIndex(currentIndex, &currentURL);

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

wsGetURLForIndexEvent::wsGetURLForIndexEvent(nsISessionHistory *yourSessionHistory, 
                                             PRInt32 historyIndex) :
        wsHistoryActionEvent(yourSessionHistory),
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
        rv = mHistory->GetURLForIndex(mHistoryIndex, &indexURL);

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

wsRefreshEvent::wsRefreshEvent(nsIWebShell* webShell, long yourLoadFlags) :
        nsActionEvent(),
        mWebShell(webShell)
{
    loadFlags = (nsLoadFlags) yourLoadFlags;
}


void *
wsRefreshEvent::handleEvent ()
{
        if (mWebShell) {
                nsresult rv = mWebShell->Reload(loadFlags);
                return (void *) rv;
        }
        return nsnull;
} // handleEvent()

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

// EOF

