/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is RaptorCanvas
 * 
 * The Initial Developer of the Original Code is Kirk Baker <kbaker@eb.com> and * Ian Wilkinson <iw@ennoble.com
 */

/*
 * nsActions.cpp
 */

#include "nsActions.h"



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

        if (actionEvent != NULL) {
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
        if (mWebShell) {
                
                printf("handleEvent(resize(x = %d y = %d w = %d h = %d))\n", mLeft, mBottom, mWidth, mHeight);

                nsresult rv = mWebShell->SetBounds(mLeft, mBottom, mWidth, mHeight);
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
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
                
                printf("handleEvent(loadURL))\n");

                nsresult rv = mWebShell->LoadURL(mURL->GetUnicode());

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(Stop))\n");

                nsresult rv = mWebShell->Stop();

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(Show))\n");

                nsresult rv = mWebShell->Show();
                
                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(Hide))\n");

                nsresult rv = mWebShell->Hide();

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(MoveTo(%ld, %ld))\n", mX, mY);

                nsresult rv = mWebShell->MoveTo(mX, mY);

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(SetFocus())\n");

                nsresult rv = mWebShell->SetFocus();

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(RemoveFocus())\n");

                nsresult rv = mWebShell->RemoveFocus();

                printf("result = %lx\n", rv);
        }
        return NULL;
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
                
                printf("handleEvent(Repaint(%d))\n", mForceRepaint);

                nsresult rv = mWebShell->Repaint(mForceRepaint);

                printf("result = %lx\n", rv);
        }
        return NULL;
} // handleEvent()



/*
 * wsCanBackEvent
 */

wsCanBackEvent::wsCanBackEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsCanBackEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(CanBack())\n");

                nsresult rv = mWebShell->CanBack();
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



/*
 * wsCanForwardEvent
 */

wsCanForwardEvent::wsCanForwardEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsCanForwardEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(CanForward())\n");

                nsresult rv = mWebShell->CanForward();
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



/*
 * wsBackEvent
 */

wsBackEvent::wsBackEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsBackEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(Back())\n");

                nsresult rv = mWebShell->Back();
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



/*
 * wsForwardEvent
 */

wsForwardEvent::wsForwardEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsForwardEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(Forward())\n");

                nsresult rv = mWebShell->Forward();
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



/*
 * wsGoToEvent
 */

wsGoToEvent::wsGoToEvent(nsIWebShell* webShell, PRInt32 historyIndex) :
        nsActionEvent(),
        mWebShell(webShell),
        mHistoryIndex(historyIndex)
{
}


void *
wsGoToEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(GoTo(%ld))\n", mHistoryIndex);

                nsresult rv = mWebShell->GoTo(mHistoryIndex);
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



/*
 * wsGetHistoryLengthEvent
 */

wsGetHistoryLengthEvent::wsGetHistoryLengthEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsGetHistoryLengthEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(wsGetHistoryLengthEvent())\n");

                PRInt32 historyLength = 0;

                nsresult rv = mWebShell->GetHistoryLength(historyLength);
                
                printf("result = %lx\n", rv);

                return (void *) historyLength;
        }
        return NULL;
} // handleEvent()



/*
 * wsGetHistoryIndexEvent
 */

wsGetHistoryIndexEvent::wsGetHistoryIndexEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsGetHistoryIndexEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(wsGetHistoryIndexEvent())\n");

                PRInt32 historyIndex = 0;

                nsresult rv = mWebShell->GetHistoryIndex(historyIndex);
                
                printf("result = %lx\n", rv);

                return (void *) historyIndex;
        }
        return NULL;
} // handleEvent()



/*
 * wsGetURLEvent
 */

wsGetURLEvent::wsGetURLEvent(nsIWebShell* webShell, PRInt32 historyIndex) :
        nsActionEvent(),
        mWebShell(webShell),
        mHistoryIndex(historyIndex)
{
}


void *
wsGetURLEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(wsGetURLEvent(%ld))\n", mHistoryIndex);

                const PRUnichar * url = nsnull;

                // returns PRUninchar * URL in <url>.  No need to delete.  References internal buffer of an nsString
                nsresult rv = mWebShell->GetURL(mHistoryIndex, &url);
                
                printf("result = %lx, url = %lx\n", rv, url);

                return (void *) url;
        }
        return NULL;
} // handleEvent()

// Added by Mark Goddard OTMP 9/2/1999

/*
 * wsRefreshEvent
 */

wsRefreshEvent::wsRefreshEvent(nsIWebShell* webShell) :
        nsActionEvent(),
        mWebShell(webShell)
{
}


void *
wsRefreshEvent::handleEvent ()
{
        if (mWebShell) {
                
                printf("handleEvent(Refresh())\n");

                nsresult rv = mWebShell->Reload(nsIChannel::LOAD_NORMAL);
                
                printf("result = %lx\n", rv);

                return (void *) rv;
        }
        return NULL;
} // handleEvent()



// EOF

