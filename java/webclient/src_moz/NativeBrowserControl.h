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

class EmbedWindow;

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
    
    nsresult    Init            (JNIEnv *env, jobject nativeEventThread);
    nsresult    Realize         (void* parentWinPtr, PRBool *aAlreadyRealized);
    void        Unrealize       (void);
    void        Show            (void);
    void        Hide            (void);
    void        Resize          (PRUint32 aWidth, PRUint32 aHeight);
    void        Destroy         (void);

/**

 * This function processes methods inserted into NativeBrowserControl's
 * actionQueue.  It is called once during the initialization of the
 * NativeEventThread java thread, and infinitely in
 * NativeEventThread.run()'s event loop.  The call to PL_HandleEvent
 * below, ends up calling the nsActionEvent subclass's handleEvent()
 * method.  After which it calls nsActionEvent::destroyEvent().

 */

    PRUint32    ProcessEventLoop(void);
    nsresult    GetFailureCode  (void);
    static PRBool IsInitialized (void);

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

    EmbedWindow *                  mWindow;
    nsCOMPtr<nsISupports>          mWindowGuard;
    JNIEnv *                       mEnv;
    jobject                        mNativeEventThread;
    ShareInitContext               mShareContext;

    //
    // Class vars
    // 
    static PLEventQueue	*	   sActionQueue; 
    static PRThread *              sEmbeddedThread; 

private:
    nsresult                       mFailureCode;
    static PRBool                  sInitComplete;

};

#endif // NativeBrowserControl_h
