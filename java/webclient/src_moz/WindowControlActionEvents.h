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
 
#ifndef WindowControlActionEvents_h___
#define WindowControlActionEvents_h___

#include "nsActions.h"

#include "nsIBaseWindow.h"

struct NativeBrowserControl;

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

class wsDeallocateInitContextEvent : public nsActionEvent {
public:
    wsDeallocateInitContextEvent(NativeBrowserControl *yourInitContext);

    void    *       handleEvent    (void);

protected:
    NativeBrowserControl *mInitContext;
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


#endif /* WindowControlActionEvents_h___ */

      
// EOF
