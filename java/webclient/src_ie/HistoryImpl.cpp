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

#include "HistoryImpl.h"
#include "ie_util.h"
#include "ie_globals.h"

#include "CMyDialog.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellBack");
      return;
    }
    
    HRESULT hr = PostMessage(initContext->browserHost, WM_BACK, 0, 0);
}

JNIEXPORT jboolean 
JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{

	WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to CanBack");
      return JNI_FALSE;
    }
    	

	jboolean result = initContext->browserObject->GetBackState();
	
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetBackList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = 0;

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeClearHistory
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeForward
(JNIEnv *env, jobject obj, jint webShellPtr)
{
	
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellBack");
    }
    
    HRESULT hr = PostMessage(initContext->browserHost, WM_FORWARD, 0, 0);
    
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanForward
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    
	WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == NULL) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to CanForward");
      return JNI_FALSE;
    }

    jboolean result = initContext->browserObject->GetForwardState();
 
    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetForwardList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = 0;

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistory
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = 0;

    return result;
}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryEntry
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    jobject result = 0;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jint result = -1;
   

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryLength
(JNIEnv *env, jobject obj, jint webShellPtr)
{
   jint result = -1;
  
    return result;
}

JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetURLForIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    jstring		urlString = 0;
    
    return urlString;
}
