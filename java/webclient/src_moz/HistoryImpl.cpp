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
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_HistoryImpl.h"

#include "ns_util.h"
#include "NativeBrowserControl.h"

#include "nsCRT.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeBack
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeBack");
		return;
	}

    nsresult rv = 
        nativeBrowserControl->mNavigation->GoBack();
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't GoBack");
    }

    return;
}

JNIEXPORT jboolean 
JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanBack
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    PRBool result = PR_FALSE;
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
    
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeCanBack");
		return result ? JNI_TRUE : JNI_FALSE;
	}
    
    nsresult rv = 
        nativeBrowserControl->mNavigation->GetCanGoBack(&result);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't GetCanGoBack");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeForward
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;

    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeForward");
		return;
	}

    nsresult rv = 
        nativeBrowserControl->mNavigation->GoForward();
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't GoForward");
    }

    return;
}

JNIEXPORT jboolean 
JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeCanForward
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    PRBool result = PR_FALSE;
	JNIEnv	*	pEnv = env;
	jobject		jobj = obj;
    
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
	if (nativeBrowserControl == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeCanForward");
		return result ? JNI_TRUE : JNI_FALSE;
	}
    
    nsresult rv = 
        nativeBrowserControl->mNavigation->GetCanGoForward(&result);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't GetCanGoForward");
    }

    return result ? JNI_TRUE : JNI_FALSE;
}


/*******************

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetBackList
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    jobjectArray result = nsnull;

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeClearHistory
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetForwardList
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    jobjectArray result = nsnull;

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistory
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    jobjectArray result = nsnull;

    return result;
}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryEntry
(JNIEnv *env, jobject obj, jint nativeBCPtr, jint historyIndex)
{
    jobject result = nsnull;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void	*	voidResult = nsnull;
    jint result = -1;
    
    NativeBrowserControl* initContext = (NativeBrowserControl *) nativeBCPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellGetHistoryIndex");
      return result;
    }

    if (initContext->initComplete) {
      wsGetHistoryIndexEvent	* actionEvent = 
          new wsGetHistoryIndexEvent(initContext);
      PLEvent	   		* event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);

      result = (jint) voidResult;
    }

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint nativeBCPtr, jint historyIndex)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    
    NativeBrowserControl* initContext = (NativeBrowserControl *) nativeBCPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeSetCurrentHistoryIndex");
      return;
    }

    if (initContext->initComplete) {
      wsGoToEvent	* actionEvent = 
          new wsGoToEvent(initContext->webNavigation, historyIndex);
      PLEvent	   	* event       = (PLEvent*) *actionEvent;
      
      ::util_PostEvent(initContext, event);
    }
    
    return;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetHistoryLength
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void	*	voidResult = nsnull;
    jint result = -1;
    
    NativeBrowserControl* initContext = (NativeBrowserControl *) nativeBCPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellGetHistoryLength");
      return result;
    }

    if (initContext->initComplete) {
      wsGetHistoryLengthEvent	* actionEvent = 
          new wsGetHistoryLengthEvent(initContext);
      PLEvent	   	        * event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);
      
      result = (jint) voidResult;
    }
    
    return result;
}

JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_HistoryImpl_nativeGetURLForIndex
(JNIEnv *env, jobject obj, jint nativeBCPtr, jint historyIndex)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    char	*	charResult = nsnull;
    jstring		urlString = nsnull;
    
    NativeBrowserControl* initContext = (NativeBrowserControl *) nativeBCPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellGetURL");
      return nsnull;
    }

    if (initContext->initComplete) {
        wsGetURLForIndexEvent * actionEvent = 
            new wsGetURLForIndexEvent(initContext, historyIndex);
        PLEvent	   	  	* event       = (PLEvent*) *actionEvent;
        
        charResult = (char *) ::util_PostSynchronousEvent(initContext, 
                                                                event);
        
        if (charResult != nsnull) {
            urlString = ::util_NewStringUTF(env, (const char *) charResult);
        }
        else {
            ::util_ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned nsnull");
            return nsnull;
        }
        nsMemory::Free((void *) charResult);
    }
    
    return urlString;
}

**********************/
