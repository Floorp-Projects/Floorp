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
 * nsActions.h
 */
 
#ifndef nsActions_h___
#define nsActions_h___

#ifndef XP_UNIX
#include <windows.h>
#endif
#include "nsIDocShell.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIWebShell.h"
#include "nsISessionHistory.h"
#include "nsString.h"
#include "plevent.h"

#include "DocumentLoaderObserverImpl.h"

/**

 * Concrete subclasses of nsActionEvent are used to safely convey an
 * action from Java to mozilla.

 * This class is kind of like a subclass of the C based PLEvent struct,
 * defined in
 * <http://lxr.mozilla.org/mozilla/source/xpcom/threads/plevent.h#455>.

 * Lifecycle

 * nsActionEvent subclass instances are usually created in a JNI method
 * implementation, such as
 * Java_org_mozilla*NavigationImpl_nativeLoadURL().  In nativeLoadURL(),
 * we create an instance of wsLoadURLEvent, which is a subclass of
 * nsActionEvent, passing in the nsIWebShell instance, and the url
 * string.  The arguments to the nsActionEvent constructor vary from one
 * subclass to the next, but they all have the property that they
 * provide information used in the subclass's handleEvent()
 * method.

 * The nsActionEvent subclass is then cast to a PLEvent struct, and
 * passed into either util_PostEvent() or util_PostSynchronous event,
 * declared in jni_util.h.  See the comments for those functions for
 * information on how they deal with the nsActionEvent, cast as a
 * PLEvent.

 * During event processing in util_Post*Event, the subclass's
 * handleEvent() method is called.  This is where the appropriate action
 * for that subclass takes place.  For example for wsLoadURLEvent,
 * handleEvent calls nsIWebShell::LoadURL().

 * The life of an nsActionEvent subclass ends shortly after the
 * handleEvent() method is called.  This happens when mozilla calls
 * nsActionEvent::destroyEvent(), which simply deletes the instance.

 */

class nsActionEvent {
public:
                        nsActionEvent  ();
        virtual        ~nsActionEvent  () {};
        virtual void *  handleEvent    (void) = 0; //{ return nsnull;};
                void    destroyEvent   (void) { delete this; };
            operator    PLEvent*       ()     { return &mEvent; };

protected:
        PLEvent         mEvent;
};

class wsHistoryActionEvent : public nsActionEvent {
public:
    wsHistoryActionEvent(nsISessionHistory *yourHistory);
    virtual ~wsHistoryActionEvent() {};

protected:
    nsISessionHistory *mHistory;
};

class wsResizeEvent : public nsActionEvent {
public:
                        wsResizeEvent  (nsIWebShell* webShell, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        PRInt32         mLeft;
        PRInt32         mBottom;
        PRInt32         mWidth;
        PRInt32         mHeight;
};


class wsLoadURLEvent : public nsActionEvent {
public:
                        wsLoadURLEvent (nsIWebShell* webShell, PRUnichar * urlString);
                       ~wsLoadURLEvent ();
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        nsString    *   mURL;
};


class wsStopEvent : public nsActionEvent {
public:
                        wsStopEvent    (nsIWebShell* webShell);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
};



class wsShowEvent : public nsActionEvent {
public:
                        wsShowEvent    (nsIWebShell* webShell);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
};



class wsHideEvent : public nsActionEvent {
public:
                        wsHideEvent    (nsIWebShell* webShell);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
};



class wsMoveToEvent : public nsActionEvent {
public:
                        wsMoveToEvent  (nsIWebShell* webShell, PRInt32 x, PRInt32 y);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        PRInt32         mX;
        PRInt32         mY;
};



class wsSetFocusEvent : public nsActionEvent {
public:
                        wsSetFocusEvent(nsIWebShell* webShell);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
};



class wsRemoveFocusEvent : public nsActionEvent {
public:
                        wsRemoveFocusEvent(nsIWebShell* webShell);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
};



class wsRepaintEvent : public nsActionEvent {
public:
                        wsRepaintEvent (nsIWebShell* webShell, PRBool forceRepaint);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        PRBool          mForceRepaint;
};



class wsCanBackEvent : public wsHistoryActionEvent {
public:
                        wsCanBackEvent (nsISessionHistory* yourSessionHistory);
        void    *       handleEvent    (void);
};



class wsCanForwardEvent : public wsHistoryActionEvent {
public:
                        wsCanForwardEvent(nsISessionHistory* yourSessionHistory);
        void    *       handleEvent    (void);
};


class wsBackEvent : public wsHistoryActionEvent {
public:
                        wsBackEvent    (nsISessionHistory* yourSessionHistory,
                                        nsIWebShell *yourWebShell);
        void    *       handleEvent    (void);

protected:
    nsIWebShell *mWebShell;
};


class wsForwardEvent : public wsHistoryActionEvent {
public:
                        wsForwardEvent (nsISessionHistory* yourSessionHistory,
                                        nsIWebShell *yourWebShell);
        void    *       handleEvent    (void);

protected:
    nsIWebShell *mWebShell;
};


class wsGoToEvent : public wsHistoryActionEvent {
public:
                        wsGoToEvent    (nsISessionHistory *yourSessionHistory,
                                        nsIWebShell* webShell, 
                                        PRInt32 historyIndex);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        PRInt32         mHistoryIndex;
};



class wsGetHistoryLengthEvent : public wsHistoryActionEvent {
public:
                       wsGetHistoryLengthEvent(nsISessionHistory* yourSessionHistory);
        void    *      handleEvent     (void);
};



class wsGetHistoryIndexEvent : public wsHistoryActionEvent {
public:
                       wsGetHistoryIndexEvent (nsISessionHistory *yourSessionHistory);
        void    *      handleEvent     (void);
};



class wsGetURLEvent : public wsHistoryActionEvent {
public:
                       wsGetURLEvent   (nsISessionHistory* yourSessionHistory);
        void    *      handleEvent     (void);
};

class wsGetURLForIndexEvent : public wsHistoryActionEvent {
public:
                 wsGetURLForIndexEvent(nsISessionHistory *yourSessionHistory, 
                                       PRInt32 historyIndex);
        void    *       handleEvent    (void);

protected:
        PRInt32         mHistoryIndex;
};



// added by Mark Goddard OTMP 9/2/1999 
class wsRefreshEvent : public nsActionEvent {
public:
                        wsRefreshEvent (nsIWebShell* webShell, 
                                        long yourLoadFlags);
        void    *       handleEvent    (void);

protected:
        nsIWebShell *   mWebShell;
        nsLoadFlags loadFlags;
};

class wsAddDocLoaderObserverEvent : public nsActionEvent {
public:
    wsAddDocLoaderObserverEvent(nsIDocShell *yourDocShell,
                                nsIDocumentLoaderObserver *yourObserver);

    void    *       handleEvent    (void);

protected:
    nsIDocShell *mDocShell;
    nsIDocumentLoaderObserver *mDocObserver;
};

#endif /* nsActions_h___ */

      
// EOF
