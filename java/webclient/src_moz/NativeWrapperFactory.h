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

#ifndef NativeWrapperFactory_h
#define NativeWrapperFactory_h

#include "nscore.h"
#include "jni_util.h"

class nsIProfile;
class nsIProfileInternal;
class nsIPref;
class nsIAppShell;
class NativeBrowserControl;
struct PLEventQueue;
struct PRThread;

class NativeWrapperFactory {
    
public:
    NativeWrapperFactory();
    ~NativeWrapperFactory();

    //
    // public API
    // 

    nsresult    Init          (JNIEnv *env, jobject nativeEventThread);
    void        Destroy       (void);

    /*
     * This function processes methods inserted into the actionQueue.
     * It is called once during the initialization of the
     * NativeEventThread java thread, and infinitely in
     * NativeEventThread.run()'s event loop.  The call to PL_HandleEvent
     * below, ends up calling the nsActionEvent subclass's handleEvent()
     * method.  After which it calls nsActionEvent::destroyEvent().
     */
    
    PRUint32    ProcessEventLoop(void);
    nsresult    GetFailureCode  (void);
    static PRBool IsInitialized (void);

    //
    // public ivars
    // 

    JNIEnv *                       mEnv;
    jobject                        mNativeEventThread;
    ShareInitContext               shareContext;

    //
    // Class vars
    // 
    nsIProfile *                   sProfile;
    nsIProfileInternal *           sProfileInternal;
    nsIPref *                      sPrefs;
    nsIAppShell *                  sAppShell;

    static PLEventQueue	*	   sActionQueue; 
    static PRThread *              sEmbeddedThread; 

private:
    nsresult                       mFailureCode;
    static PRBool                  sInitComplete;


};


#endif // NativeWrapperFactory_h
