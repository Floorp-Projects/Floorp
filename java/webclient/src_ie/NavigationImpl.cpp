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
 *                 Ron Capelli <capelli@us.ibm.com>
 */



#include "ie_globals.h"
#include "NavigationImpl.h"

#include "ie_util.h"
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeLoadURL
(JNIEnv *env, jobject obj, jint webShellPtr, jstring urlString)
{
    jobject jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
	::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeLoadURL");
    }

    const wchar_t * urlStringChars =  ::util_GetStringChars(env, urlString);

    if (::util_ExceptionOccurred(env)) {
	::util_ThrowExceptionToJava(env, "nativeLoadURL Exception: unable to extract Java string");
	if (urlStringChars != NULL)
	    ::util_ReleaseStringChars(env, urlString, urlStringChars);
	return;
    }

    initContext->wcharURL = _wcsdup((const wchar_t *) urlStringChars);

    HRESULT hr = PostMessage(initContext->browserHost, WM_NAVIGATE, 0, 0);
    ::util_ReleaseStringChars(env, urlString, urlStringChars);
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeRefresh
(JNIEnv *env, jobject obj, jint webShellPtr, jlong loadFlags)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
	::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeRefresh");
	return;
    }

    HRESULT hr = PostMessage(initContext->browserHost, WM_REFRESH,0, 0);

    return;
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeStop
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
	::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeStop");
	return;
    }

    HRESULT hr = PostMessage(initContext->browserHost, WM_STOP, 0, 0);
}

JNIEXPORT void JNICALL
Java_org_mozilla_webclient_wrapper_1native_NavigationImpl_nativeSetPrompt
(JNIEnv *env, jobject obj, jint webShellPtr, jobject userPrompt)
{
    JNIEnv * pEnv = env;
    jobject  jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
	::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeSetPrompt");
	return;
    }

    if (userPrompt == nsnull) {
	::util_ThrowExceptionToJava(env, "Exception: null properties passed to nativeSetPrompt");
	return;
    }

    if (!initContext->initComplete) {
	return;
    }

    // hr = PostMessage(initContext->browserHost, ?SETPROMPT?, 0, 0);
}

