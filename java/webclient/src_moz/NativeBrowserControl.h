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
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Ann Sunhachawee
 */

#ifndef NativeBrowserControl_h
#define NativeBrowserControl_h

#include <nsCOMPtr.h>
#include <nsIWebNavigation.h> // for all Navigation commands
#include <nsISHistory.h>
#include <nsIDOMEventReceiver.h>

#ifdef XP_PC
#include <windows.h> // for HWND
#endif

#include "ns_util.h"

class EmbedProgress;
class EmbedWindow;
class EmbedEventListener;
class NativeWrapperFactory;
class nsPIDOMWindow;


/**
 * <p>Native analog to BrowserControl.  Hosts per-window things.  Maps
 * closely to EmbedPrivate in GtkEmbed.</p>
 *
 */

class NativeBrowserControl {
    
public:
    
    NativeBrowserControl();
    ~NativeBrowserControl();

    //
    // public API
    // 
    
    nsresult    Init            (NativeWrapperFactory *yourWrapperFactory);
    nsresult    Realize         (jobject javaBrowserControl,
                                 void* parentWinPtr, 
                                 PRBool *aAlreadyRealized, 
                                 PRUint32 width, PRUint32 height);
    void        Unrealize       (void);
    void        Show            (void);
    void        Hide            (void);
    void        Resize          (PRUint32 x, PRUint32 y,
                                 PRUint32 aWidth, PRUint32 aHeight);
    void        Destroy         (void);

    NativeWrapperFactory * GetWrapperFactory();

    jobject     QueryInterfaceJava(WEBCLIENT_INTERFACES interface);

    // This is an upcall that will come from the progress listener
    // whenever there is a content state change.  We need this so we can
    // attach event listeners.
    void        ContentStateChange    (void);

private:

    void GetListener (void);

    void AttachListeners(void);
    void DetachListeners(void);

    // this will get the PIDOMWindow for this widget
    nsresult        GetPIDOMWindow   (nsPIDOMWindow **aPIWin);

public:

    //
    // Relationship ivars
    // 
    
#ifdef XP_UNIX
    GtkWidget *                    parentHWnd;
#else 
    HWND                           parentHWnd;
#endif
    
    nsCOMPtr<nsIWebNavigation>     mNavigation;
    nsCOMPtr<nsISHistory>          mSessionHistory;

    // our event receiver
    nsCOMPtr<nsIDOMEventReceiver>  mEventReceiver;

    EmbedWindow *                  mWindow;
    nsCOMPtr<nsISupports>          mWindowGuard;
    EmbedProgress *                mProgress;
    nsCOMPtr<nsISupports>          mProgressGuard;
    EmbedEventListener            *mEventListener;
    nsCOMPtr<nsISupports>          mEventListenerGuard;
    ShareInitContext               mShareContext;

    // chrome mask
    PRUint32                       mChromeMask;
    // is this a chrome window?
    PRBool                         mIsChrome;
    // has the chrome finished loading?
    PRBool                         mChromeLoaded;
    // has someone called Destroy() on us?
    PRBool                         mIsDestroyed;
    // is the chrome listener attached yet?
    PRBool                         mListenersAttached;

    jobject                        mJavaBrowserControl;

    NativeWrapperFactory *         wrapperFactory;

};

#endif // NativeBrowserControl_h
