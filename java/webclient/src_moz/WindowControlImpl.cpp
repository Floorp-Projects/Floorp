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

#include "WindowControlImpl.h"
#include <jni.h>
#include "jni_util.h"
#include "nsActions.h"

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
            new wsResizeEvent(initContext->webShell, x, y, w, h);
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
    initContext->embeddedThread = nsnull;
    initContext->sessionHistory = nsnull;
    initContext->actionQueue = nsnull;
    initContext->env = env;
    initContext->nativeEventThread = nsnull;
    initContext->stopThread = FALSE;
    initContext->x = x;
    initContext->y = y;
    initContext->w = width;
    initContext->h = height;

#ifdef XP_UNIX
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

    initContext->parentHWnd = nsnull;
    //    ((nsISupports *)initContext->docShell)->Release();
    initContext->docShell = nsnull;
    //    ((nsISupports *)initContext->webShell)->Release();
    initContext->webShell = nsnull;

    //NOTE we don't de-allocate the global session history here.
    initContext->sessionHistory = nsnull;

    // PENDING(edburns): not sure if these need to be deleted
    initContext->actionQueue = nsnull;
    initContext->embeddedThread = nsnull;
    initContext->env = nsnull;
    if (nsnull != initContext->nativeEventThread) {
        ::util_DeleteGlobalRef(env, initContext->nativeEventThread);
        initContext->nativeEventThread = nsnull;
    }
    initContext->stopThread = -1;
    initContext->initComplete = FALSE;
    initContext->initFailCode = 0;
    initContext->x = -1;
    initContext->y = -1;
    initContext->w = -1;
    initContext->h = -1;    
    initContext->gtkWinPtr = nsnull;

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
		wsMoveToEvent	* actionEvent = new wsMoveToEvent(initContext->webShell, x, y);
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
		wsRemoveFocusEvent	* actionEvent = new wsRemoveFocusEvent(initContext->webShell);
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
		wsRepaintEvent	* actionEvent = new wsRepaintEvent(initContext->webShell, (PRBool) forceRepaint);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}

}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetVisible
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean newState)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    nsCOMPtr<nsIBaseWindow> baseWindow;
    nsresult rv;
    
    rv = initContext->webShell->QueryInterface(NS_GET_IID(nsIBaseWindow),
                                               getter_AddRefs(baseWindow));
    
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
        return;
    }
    rv = baseWindow->SetVisibility(JNI_TRUE == newState ? PR_TRUE : PR_FALSE);
    if (NS_FAILED(rv)) {
        initContext->initFailCode = kShowWebShellError;
		::util_ThrowExceptionToJava(env, "Exception: can't SetVisibility");
        return;
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
		wsSetFocusEvent	* actionEvent = new wsSetFocusEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

		::util_PostEvent(initContext, event);
	}
}



