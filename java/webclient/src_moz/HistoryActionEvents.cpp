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
 * HistoryActionEvents.cpp
 */

#include "HistoryActionEvents.h"
#include "ns_util.h"
#include "nsIHistoryEntry.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsReadableUtils.h"

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
        nsresult rv = mWebNavigation->GotoIndex(mHistoryIndex);
        result = (void *) rv;
    }
    return result;
} // handleEvent()



/*
 * wsGetHistoryLengthEvent
 */

wsGetHistoryLengthEvent::wsGetHistoryLengthEvent(NativeBrowserControl 
                                                 *yourInitContext) :
     nsActionEvent(),
     mInitContext(yourInitContext)
{
}


void *
wsGetHistoryLengthEvent::handleEvent ()
{
    void *result = nsnull;

    if (mInitContext) {
        nsISHistory* sHistory;
        PRInt32 historyLength = 0;
        nsresult rv = mInitContext->webNavigation->
            GetSessionHistory(&sHistory);
        
        if ( NS_FAILED(rv)) {
            return result;
        }
        rv = sHistory->GetCount(&historyLength);
        result = (void *) historyLength;
    }
    return result;
} // handleEvent()


/*
 * wsGetHistoryIndexEvent
 */

wsGetHistoryIndexEvent::wsGetHistoryIndexEvent(NativeBrowserControl *yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}


void *
wsGetHistoryIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mInitContext) {
        PRInt32 historyIndex = 0;
        nsISHistory* sHistory;
        nsresult rv = mInitContext->webNavigation->GetSessionHistory(&sHistory);
        
        rv = sHistory->GetIndex(&historyIndex);
        result = (void *) historyIndex;
    }
    return result;
} // handleEvent()




/*
 * wsGetURLForIndexEvent
 */

wsGetURLForIndexEvent::wsGetURLForIndexEvent(NativeBrowserControl *yourInitContext, 
                                             PRInt32 historyIndex) :
  nsActionEvent(),
  mInitContext(yourInitContext),
  mHistoryIndex(historyIndex)
{
}


void *
wsGetURLForIndexEvent::handleEvent ()
{
    void *result = nsnull;
    if (mInitContext) {
        nsresult rv;
        nsISHistory* sHistory;
        rv = mInitContext->webNavigation->GetSessionHistory(&sHistory);
        char *indexURL = nsnull;
        
        nsIHistoryEntry * Entry;
        rv = sHistory->GetEntryAtIndex(mHistoryIndex, PR_FALSE, &Entry);
        if (NS_FAILED(rv)) {
            return result;
        }
        
        nsIURI * URI;
        rv = Entry->GetURI(&URI);
        
        if (NS_FAILED(rv)) {
            return result;
        }

        nsCString urlSpecString;

        rv = URI->GetSpec(urlSpecString);
        if (NS_FAILED(rv)) {
            return result;
        }	
        
        indexURL = ToNewCString(urlSpecString);
        result = (void *) indexURL;
    }
    return result;
} // handleEvent()


