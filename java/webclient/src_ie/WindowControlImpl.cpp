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
 * Contributor(s): Glenn Barney <gbarney@uiuc.edu>
 */
	
#include "WindowControlImpl.h"
#include <windows.h>
#include <jni.h>
#include "ie_util.h"
#include "ie_globals.h"

#include "CMyDialog.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetBounds
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y, jint w, jint h)
{

   WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
   
   if (initContext == NULL) {
       ::util_ThrowExceptionToJava(env, "Exception:  null Ptr passed to nativeSetBounds");
       return;
   }

   
   initContext->x = x;
   initContext->y = y;
   initContext->w = w;
   initContext->h = h;


   HRESULT hr = PostMessage(initContext->browserHost, WM_RESIZE, 0, 0);

}

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeCreateInitContext
(JNIEnv *env, jobject obj, jint windowPtr, jint x, jint y, 
 jint width, jint height, jobject aBrowserControlImpl)
{
#ifdef XP_MAC
    //MAC STUFF GOES HERE
#elif defined(XP_PC)
    HWND parentHWnd = (HWND)windowPtr;
#elif defined(XP_UNIX)
    //unix stuff here
#endif

    if (parentHWnd == NULL) {
      ::util_ThrowExceptionToJava(env, "Exception: null window handle passed to raptorWebShellCreate");
      return (jint) 0;
    }

    WebShellInitContext* initContext = new WebShellInitContext;

    initContext->initComplete = FALSE;
    initContext->initFailCode = 0;
    initContext->parentHWnd = parentHWnd;
	initContext->wcharURL=NULL;
    initContext->env = env;
    initContext->nativeEventThread = NULL;

    initContext->x = x;
    initContext->y = y;
    initContext->w = width;
    initContext->h = height;

    initContext->browserObject = new CMyDialog(initContext);

    return (jint) initContext;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeDestroyInitContext
(JNIEnv *env, jobject obj, jint webShellPtr)
{
 
	WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
	
	    
	if (initContext == NULL) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeDestroyInitContext");
		return;
	}

	initContext->parentHWnd = NULL;

	initContext->env = NULL;
	initContext->wcharURL = NULL;

       if (NULL != initContext->nativeEventThread) {
        ::util_DeleteGlobalRef(env, initContext->nativeEventThread);
        initContext->nativeEventThread = NULL;
    }
    initContext->initComplete = FALSE;
    initContext->initFailCode = 0;
    initContext->x = -1;
    initContext->y = -1;
    initContext->w = -1;
    initContext->h = -1;    

    delete initContext->browserObject;
    initContext->browserObject = NULL;

    delete initContext;
  
}


JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeMoveWindowTo
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y)
{
   WebShellInitContext * initContext = (WebShellInitContext *) webShellPtr;
   if (initContext == NULL) {
       ::util_ThrowExceptionToJava(env, "Exception:  null Ptr passed to nativeMoveWindowTo");
       return;
   }
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeRemoveFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeRepaint
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean forceRepaint)
{
  

}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetVisible
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean newState)
{


}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_WindowControlImpl_nativeSetFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
  
}



