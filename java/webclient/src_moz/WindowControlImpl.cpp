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

#include "org_mozilla_webclient_impl_wrapper_0005fnative_WindowControlImpl.h"

#include "WindowControlActionEvents.h"

#include "ns_util.h"

#include "nsIThread.h" // for PRThread

#include "nsCOMPtr.h" // to get nsIBaseWindow from webshell
#include "nsIBaseWindow.h" // to get methods like SetVisibility

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetBounds
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y, jint w, jint h)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;
    
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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeMoveWindowTo
(JNIEnv *env, jobject obj, jint webShellPtr, jint x, jint y)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRemoveFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeRepaint
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean forceRepaint)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetVisible
(JNIEnv *env, jobject obj, jint webShellPtr, jboolean newState)
{

  NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;
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

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_WindowControlImpl_nativeSetFocus
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    NativeBrowserControl* initContext = (NativeBrowserControl *) webShellPtr;

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
