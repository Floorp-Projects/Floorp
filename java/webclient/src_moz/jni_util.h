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
 *               Ann Sunhachawee
 */


/**

 * Util methods

 */

#ifndef jni_util_h
#define jni_util_h

#include <jni.h>

#include "nsCOMPtr.h" // so we can save the docShell
#include "nsIDocShell.h" // so we can save our nsIDocShell
#include "nsISessionHistory.h" // so we can save our nsISessionHistory
#include "nsIThread.h" // for PRThread
#include "nsIWebShell.h" // for nsIWebShell
#include "nsIEventQueueService.h" // for PLEventQueue

// Ashu
#ifdef XP_UNIX
#include "nsIWidget.h" // for GTKWidget
#include <gtk/gtk.h>
#endif
//

//
// local classes
//

// PENDING(edburns): this should be a class, and we should define a
// constructor and destructor for it.

struct WebShellInitContext {
#ifdef XP_UNIX
    GtkWidget * parentHWnd;
#else 
    // PENDING(mark): Don't we need something for Mac?
    HWND				parentHWnd;
#endif
	nsCOMPtr<nsIWebShell> webShell;
    nsCOMPtr<nsIDocShell> docShell;
	nsISessionHistory*	sessionHistory;
	PLEventQueue	*	actionQueue;
	PRThread		*	embeddedThread;
	JNIEnv			*	env;
    jobject             nativeEventThread;
	int					stopThread;
	int					initComplete;
	int					initFailCode;
	int					x;
	int					y;
	int					w;
	int					h;
        int                                     gtkWinPtr;
};

enum {
	kEventQueueError = 1,
	kCreateWebShellError,
	kInitWebShellError,
	kShowWebShellError,
	kHistoryWebShellError,
    kClipboardWebShellError
};

extern JavaVM *gVm;

void    util_ThrowExceptionToJava (JNIEnv * env, const char * message);

/**

 * This method calls PL_PostEvent(),

 * http://lxr.mozilla.org/mozilla/source/xpcom/threads/plevent.c#248

 * which simply uses nice monitors to insert the event into the provided
 * event queue, which is from WebShellInitContext->actionQueue, which is
 * created in NativeEventThread.cpp:InitMozillaStuff().  The events are
 * processed in NativeEventThread.cpp:processEventLoop, which is called
 * from the Java NativeEventThread.run().  See the code and comments for
 * processEventLoop in NativeEventThread.cpp.

 */

void    util_PostEvent (WebShellInitContext * initContext, PLEvent * event);


/**

 * This method calls PL_PostSynchronousEvent(),

 * http://lxr.mozilla.org/mozilla/source/xpcom/threads/plevent.c#278

 * which, instead of putting the event in the queue, as in
 * util_PostEvent(), either calls the event's handler directly, or puts
 * it in the queue and waits for it to be processed so it can return the
 * result.

 */

void *  util_PostSynchronousEvent (WebShellInitContext * initContext, PLEvent * event);

/**

 * simply call the java method nativeEventOccurred on
 * eventRegistrationImpl, passing webclientEventListener and eventType
 * as arguments.

 */

void    util_SendEventToJava(JNIEnv *env, jobject eventRegistrationImpl,
                             jobject webclientEventListener, 
                             jlong eventType);

char *util_GetCurrentThreadName(JNIEnv *env);

void util_DumpJavaStack(JNIEnv *env);

//
// Functions from secret JDK files
//

JNIEXPORT jvalue JNICALL
JNU_CallMethodByName(JNIEnv *env, 
					 jboolean *hasException,
					 jobject obj, 
					 const char *name,
					 const char *signature,
					 ...);

JNIEXPORT jvalue JNICALL
JNU_CallMethodByNameV(JNIEnv *env, 
					  jboolean *hasException,
					  jobject obj, 
					  const char *name,
					  const char *signature, 
					  va_list args);

JNIEXPORT void * JNICALL
JNU_GetEnv(JavaVM *vm, jint version);

#endif // jni_util_h
