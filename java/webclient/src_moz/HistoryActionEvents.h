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
 
#ifndef HistoryActionEvents_h___
#define HistoryActionEvents_h___

#include "nsActions.h"
#include "nsIWebNavigation.h"
#include "nsISHistory.h"

struct NativeBrowserControl;

class wsCanBackEvent : public nsActionEvent {
public:
                        wsCanBackEvent (nsIWebNavigation* webNavigation);
        void    *       handleEvent    (void);
protected:
        nsIWebNavigation * mWebNavigation;
};

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
    wsGetHistoryLengthEvent(NativeBrowserControl *yourInitContext);
    void    *      handleEvent     (void);
    
protected:
    NativeBrowserControl *mInitContext;
};

class wsGetHistoryIndexEvent : public nsActionEvent {
public:
                       wsGetHistoryIndexEvent (NativeBrowserControl *yourInitContext);
        void    *      handleEvent     (void);
protected:
    NativeBrowserControl *mInitContext;
};

class wsGetURLForIndexEvent : public nsActionEvent {
public:
                 wsGetURLForIndexEvent(NativeBrowserControl *yourInitContext, 
                                       PRInt32 historyIndex);
        void    *       handleEvent    (void);

protected:
    NativeBrowserControl *mInitContext;
    PRInt32         mHistoryIndex;
};

#endif /* HistoryActionEvents_h___ */

      
// EOF
