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

#include "WindowControlImpl.h"

#include "WindowControlActionEvents.h"

#include "ns_util.h"

#include "nsIThread.h" // for PRThread

#include "nsCOMPtr.h" // to get nsIBaseWindow from webshell
#include "nsIBaseWindow.h" // to get methods like SetVisibility




JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetBounds
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y, jint w, jint h)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeSetBounds");
		return;
	}
	if (initContext->initComplete) {
		wsResizeEvent	* actionEvent = 
            new wsResizeEvent(initContext->baseWindow, x, y, w, h);
        PLEvent			* event       = (PLEvent*) *actionEvent;
        
		::util_PostEvent(initContext, event);
	}
    
}

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeCreateInitContext
(JNIEnv *env, jobject obj, jint windowPtr, jint x, jint y, 
 jint width, jint height, jobject aBrowserControlImpl)
{
#ifdef XP_MAC
    WindowPtr		pWindow = (WindowPtr) windowPtr;
    Rect			webRect = pWindow->portRect;
    // nsIWidget	*	pWidget = (nsIWidget *) widgetPtr;
#elif defined(XP_PC)
    // elif defined(XP_WIN)
    HWND parentHWnd = (HWND)windowPtr;
#elif defined(XP_UNIX)
    GtkWidget * parentHWnd = (GtkWidget *) windowPtr;
#endif
    
    if (parentHWnd == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null window handle passed to raptorWebShellCreate");
      return (jint) 0;
    }
    WebShellInitContext* initContext = new WebShellInitContext;
    
    initContext->initComplete = FALSE;
    initContext->initFailCode = 0;
    initContext->parentHWnd = parentHWnd;
    initContext->webShell = nsnull;
    initContext->docShell = nsnull;
    initContext->baseWindow = nsnull;
    initContext->webNavigation = nsnull;
    initContext->presShell = nsnull;
    //    initContext->embeddedThread = nsnull;
    //    initContext->actionQueue = nsnull;
    initContext->env = env;
    initContext->nativeEventThread = nsnull;
    initContext->stopThread = FALSE;
    initContext->x = x;
    initContext->y = y;
    initContext->w = width;
    initContext->h = height;
    initContext->currentDocument = nsnull;
    initContext->browserContainer = nsnull;
    util_InitializeShareInitContext(&(initContext->shareContext));

#ifdef XP_UNIX
    /***** Uncomment this to debug on unix
    pid_t pid = getpid();
    printf("++++++++++++++++debug: edburns: pid is: %d\n", pid);
    sleep(7);
    **************/ 

    initContext->gtkWinPtr = 
        (int)::util_GetGTKWinPtrFromCanvas(env, aBrowserControlImpl);
#else
    initContext->gtkWinPtr = nsnull;
#endif
    
    return (jint) initContext;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeDestroyInitContext
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeDestroyInitContext");
		return;
	}
    wsDeallocateInitContextEvent	* actionEvent = 
            new wsDeallocateInitContextEvent(initContext);
    PLEvent			* event       = (PLEvent*) *actionEvent;
    nsresult rv;
    
    rv = (nsresult) ::util_PostSynchronousEvent(initContext, event);
    if (NS_FAILED(rv)) {
		::util_ThrowExceptionToJava(env, "Exception: Can't destroy initContext");
		return;
	}
    //    initContext->actionQueue = nsnull;
    delete initContext;
}


JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeMoveWindowTo
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellMoveTo");
		return;
	}

	if (initContext->initComplete) {
		wsMoveToEvent	* actionEvent = new wsMoveToEvent(initContext->baseWindow, x, y);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeRemoveFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRemoveFocus");
		return;
	}

	if (initContext->initComplete) {
		wsRemoveFocusEvent	* actionEvent = new wsRemoveFocusEvent(initContext->baseWindow);
        PLEvent				* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeRepaint
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean forceRepaint)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRepaint");
		return;
	}

	if (initContext->initComplete) {
		wsRepaintEvent	* actionEvent = new wsRepaintEvent(initContext->baseWindow, (PRBool) forceRepaint);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}

}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetVisible
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean newState)
{

  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
  if (initContext == nsnull) {
    ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRepaint");
    return;
  }
  if (initContext->initComplete) {
    wsShowEvent * actionEvent = new  wsShowEvent(initContext->baseWindow, JNI_TRUE == newState ? PR_TRUE : PR_FALSE);
    PLEvent			* event       = (PLEvent*) *actionEvent;
    ::util_PostEvent(initContext, event);
  }
  
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellSetFocus");
		return;
	}

	if (initContext->initComplete) {
		wsSetFocusEvent	* actionEvent = new wsSetFocusEvent(initContext->baseWindow);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}
}
