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
#include "nsISHistory.h"
#include "nsIBaseWindow.h"
#include "nsIWebNavigation.h"
#include "nsIFindComponent.h"
#include "nsISearchContext.h"
#include "nsIContentViewerEdit.h"
#include "nsString.h"
#include "plevent.h"



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
 * declared in ns_util.h.  See the comments for those functions for
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
    wsHistoryActionEvent(nsISHistory *yourHistory);
    virtual ~wsHistoryActionEvent() {};

protected:
    nsISHistory *mHistory;
};

class wsResizeEvent : public nsActionEvent {
public:
                        wsResizeEvent  (nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
        PRInt32         mLeft;
        PRInt32         mBottom;
        PRInt32         mWidth;
        PRInt32         mHeight;
};


class wsLoadURLEvent : public nsActionEvent {
public:
                        wsLoadURLEvent (nsIWebNavigation* webNavigation, PRUnichar * urlString, PRInt32 urlLength);
                       ~wsLoadURLEvent ();
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
        nsString    *   mURL;
};


class wsStopEvent : public nsActionEvent {
public:
                        wsStopEvent    (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
};



class wsShowEvent : public nsActionEvent {
public:
                        wsShowEvent    (nsIBaseWindow* baseWindow, PRBool state);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
  PRBool mState;
};



class wsHideEvent : public nsActionEvent {
public:
                        wsHideEvent    (nsIBaseWindow* baseWindow);
        void    *       handleEvent    (void);

protected:
       nsIBaseWindow *   mBaseWindow;
};



class wsMoveToEvent : public nsActionEvent {
public:
                        wsMoveToEvent  (nsIBaseWindow* baseWindow, PRInt32 x, PRInt32 y);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
        PRInt32         mX;
        PRInt32         mY;
};



class wsSetFocusEvent : public nsActionEvent {
public:
                        wsSetFocusEvent(nsIBaseWindow* baseWindow);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
};



class wsRemoveFocusEvent : public nsActionEvent {
public:
                        wsRemoveFocusEvent(nsIBaseWindow* baseWindow);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
};



class wsRepaintEvent : public nsActionEvent {
public:
                        wsRepaintEvent (nsIBaseWindow* baseWindow, PRBool forceRepaint);
        void    *       handleEvent    (void);

protected:
        nsIBaseWindow *   mBaseWindow;
        PRBool          mForceRepaint;
};



//class wsCanBackEvent : public wsHistoryActionEvent {
class wsCanBackEvent : public nsActionEvent {
public:
                        wsCanBackEvent (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);
protected:
        nsIWebNavigation * mWebNavigation;
};



//class wsCanForwardEvent : public wsHistoryActionEvent {
class wsCanForwardEvent : public nsActionEvent { 
public:
                        wsCanForwardEvent(nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);
protected:
        nsIWebNavigation * mWebNavigation;
};


class wsBackEvent : public nsActionEvent {
public:
                        wsBackEvent    (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);

protected:
    nsIWebNavigation * mWebNavigation;
};


class wsForwardEvent : public nsActionEvent {
public:
                        wsForwardEvent (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);

protected:
    nsIWebNavigation * mWebNavigation;
};


class wsGoToEvent : public nsActionEvent {
public:
                        wsGoToEvent    (nsIWebNavigation* webNavigation, 
                                        PRInt32 historyIndex);
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
        PRInt32         mHistoryIndex;
};



class wsGetHistoryLengthEvent : public nsActionEvent {
public:
                       wsGetHistoryLengthEvent(nsISHistory * sHistory);
        void    *      handleEvent     (void);

protected:
    nsISHistory * mHistory;
};



class wsGetHistoryIndexEvent : public nsActionEvent {
public:
                       wsGetHistoryIndexEvent (nsISHistory * sHistory);
        void    *      handleEvent     (void);
protected:
    nsISHistory * mHistory;
};



class wsGetURLEvent : public nsActionEvent {
public:
                       wsGetURLEvent   (nsISHistory * sHistory);
        void    *      handleEvent     (void);
protected:
    nsISHistory * mHistory;
};


class wsGetURLForIndexEvent : public nsActionEvent {
public:
                 wsGetURLForIndexEvent(nsISHistory * sHistory, 
                                       PRInt32 historyIndex);
        void    *       handleEvent    (void);

protected:
  nsISHistory * mHistory;
  PRInt32         mHistoryIndex;
};



// added by Mark Goddard OTMP 9/2/1999 
class wsRefreshEvent : public nsActionEvent {
public:
                        wsRefreshEvent (nsIWebNavigation* webNavigation, 
                                        PRInt32 reloadType);
        void    *       handleEvent    (void);

protected:
        nsIWebNavigation *   mWebNavigation;
        PRInt32 mReloadType;
};

class wsViewSourceEvent : public nsActionEvent {
public:
    wsViewSourceEvent (nsIDocShell * docShell, PRBool viewMode);
    void * handleEvent (void);

protected:
    nsIDocShell * mDocShell;
    PRBool mViewMode;
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

struct WebShellInitContext;

class wsDeallocateInitContextEvent : public nsActionEvent {
public:
    wsDeallocateInitContextEvent(WebShellInitContext *yourInitContext);

    void    *       handleEvent    (void);

protected:
    WebShellInitContext *mInitContext;
};

class wsFindEvent : public nsActionEvent {
public:
    wsFindEvent(nsIFindComponent *findComponent, 
		  nsISearchContext * srchcontext);
    void * handleEvent (void);

protected:
    nsIFindComponent * mFindComponent;
    nsISearchContext * mSearchContext;
};


class wsSelectAllEvent : public nsActionEvent {
public:
    wsSelectAllEvent(nsIContentViewerEdit * contentViewerEdit);
    void * handleEvent (void);

protected:
    nsIContentViewerEdit * mContentViewerEdit;
};


class wsCopySelectionEvent : public nsActionEvent {
public:
    wsCopySelectionEvent(nsIContentViewerEdit * contentViewerEdit);
    void * handleEvent (void);

protected:
    nsIContentViewerEdit * mContentViewerEdit;
};


#endif /* nsActions_h___ */

      
// EOF
