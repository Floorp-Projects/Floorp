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

#include "NavigationImpl.h"

#include "jni_util.h"

#include "nsActions.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeLoadURL
(JNIEnv *env, jobject obj, jint webShellPtr, jstring urlString)
{
    jobject			jobj = obj;

#if DEBUG_RAPTOR_CANVAS
    const char	*	urlChars = (char *) ::util_GetStringUTFChars(env, 
                                                                 urlString);
    if (prLogModuleInfo) {
        PR_LOG(prLogModuleInfo, 3, 
               ("Native URL = \"%s\"\n", urlChars));
    }
    ::util_ReleaseStringUTFChars(env, urlString, urlChars);
#endif
    
    PRUnichar	*	urlStringChars = (PRUnichar *) ::util_GetStringChars(env,
                                                                         urlString);
    
    if (::util_ExceptionOccurred(env)) {
	::util_ThrowExceptionToJava(env, "raptorWebShellLoadURL Exception: unable to extract Java string");
	if (urlStringChars != nsnull)
	  ::util_ReleaseStringChars(env, urlString, urlStringChars);
	return;
      }
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellLoadURL");
      if (urlStringChars != nsnull)
	::util_ReleaseStringChars(env, urlString, urlStringChars);
      return;
    }
    
    if (initContext->initComplete) {
      wsLoadURLEvent	* actionEvent = new wsLoadURLEvent(initContext->webShell, urlStringChars);
      PLEvent			* event       = (PLEvent*) *actionEvent;
      
      ::util_PostEvent(initContext, event);
    }
    
    ::util_ReleaseStringChars(env, urlString, urlStringChars);
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeRefresh
(JNIEnv *env, jobject obj, jint webShellPtr, jlong loadFlags)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	void	*	voidResult = nsnull;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellRefresh");
		return;
	}

	if (initContext->initComplete) {
		wsRefreshEvent	* actionEvent = new wsRefreshEvent(initContext->webShell, (long) loadFlags);
        PLEvent	   	* event       = (PLEvent*) *actionEvent;

        voidResult = ::util_PostSynchronousEvent(initContext, event);

		return;
	}

	return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeStop
(JNIEnv *env, jobject obj, jint webShellPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
	
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellStop");
		return;
	}

	if (initContext->initComplete) {
		wsStopEvent		* actionEvent = new wsStopEvent(initContext->webShell);
        PLEvent			* event       = (PLEvent*) *actionEvent;

        ::util_PostEvent(initContext, event);
	}
}
