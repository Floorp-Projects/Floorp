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

#include "HistoryImpl.h"

#include "jni_util.h"

#include "nsActions.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellBack");
      return;
    }
    
    if (initContext->initComplete) {
      wsBackEvent	* actionEvent = 
          new wsBackEvent(initContext->sessionHistory, initContext->webShell);
      PLEvent	   	* event       = (PLEvent*) *actionEvent;

#ifdef XP_PC
    // debug: edburns:
      DWORD nativeThreadID = GetCurrentThreadId();
      printf("debug: edburns: HistoryImpl_nativeBack() nativeThreadID: %d\n",
             nativeThreadID);
#endif

      char *currentThreadName = nsnull;
      
      if (nsnull != (currentThreadName = util_GetCurrentThreadName(env))) {
          printf("debug: edburns: HistoryImpl_nativeBack() java threadName: %s\n",
                 currentThreadName);
          delete currentThreadName;
      }


      
      ::util_PostSynchronousEvent(initContext, event);
    }

    return;
}

JNIEXPORT jboolean 
JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanBack
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jboolean result = JNI_FALSE;
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void        *       voidResult;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellCanBack");
      return result;
    }
    
    if (initContext->initComplete) {
        wsCanBackEvent	* actionEvent = 
            new wsCanBackEvent(initContext->sessionHistory);
      PLEvent			* event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);
      
      result =  (PR_FALSE == ((PRBool) voidResult)) ? JNI_FALSE : JNI_TRUE;
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetBackList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = nsnull;

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
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;

    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellForward");
      return;
    }
    
    if (initContext->initComplete) {
      wsForwardEvent	* actionEvent = 
          new wsForwardEvent(initContext->sessionHistory,
                             initContext->webShell);
      PLEvent	   	* event       = (PLEvent*) *actionEvent;
      
      ::util_PostSynchronousEvent(initContext, event);
    }

    return;
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeCanForward
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jboolean result = JNI_FALSE;
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void        *       voidResult;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellCanForward");
      return result;
    }
    
    if (initContext->initComplete) {
      wsCanForwardEvent	* actionEvent = 
          new wsCanForwardEvent(initContext->sessionHistory);
      PLEvent			* event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);
      
      result =  (PR_FALSE == ((PRBool) voidResult)) ? JNI_FALSE : JNI_TRUE;
    }

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetForwardList
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = nsnull;

    return result;
}

JNIEXPORT jobjectArray JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistory
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jobjectArray result = nsnull;

    return result;
}

JNIEXPORT jobject JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryEntry
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    jobject result = nsnull;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void	*	voidResult = nsnull;
    jint result = -1;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetHistoryIndex");
      return result;
    }

    if (initContext->initComplete) {
      wsGetHistoryIndexEvent	* actionEvent = 
          new wsGetHistoryIndexEvent(initContext->sessionHistory);
      PLEvent	   		* event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);

      result = (jint) voidResult;
    }

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeSetCurrentHistoryIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void	*	voidResult = nsnull;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeSetCurrentHistoryIndex");
      return;
    }

    if (initContext->initComplete) {
      wsGoToEvent	* actionEvent = 
          new wsGoToEvent(initContext->sessionHistory,
                          initContext->webShell, historyIndex);
      PLEvent	   	* event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);
    }
    
    return;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetHistoryLength
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    void	*	voidResult = nsnull;
    jint result = -1;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetHistoryLength");
      return result;
    }
    
    if (initContext->initComplete) {
      wsGetHistoryLengthEvent	* actionEvent = 
          new wsGetHistoryLengthEvent(initContext->sessionHistory);
      PLEvent	   	        * event       = (PLEvent*) *actionEvent;
      
      voidResult = ::util_PostSynchronousEvent(initContext, event);
      
      result = (jint) voidResult;
    }
    
    return result;
}

JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_HistoryImpl_nativeGetURLForIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint historyIndex)
{
    JNIEnv	*	pEnv = env;
    jobject		jobj = obj;
    char	*	charResult = nsnull;
    jstring		urlString = nsnull;
    
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetURL");
      return nsnull;
    }
    
    if (initContext->initComplete) {
        wsGetURLForIndexEvent * actionEvent = 
            new wsGetURLForIndexEvent(initContext->sessionHistory,
                                      historyIndex);
      PLEvent	   	  	* event       = (PLEvent*) *actionEvent;
      
      charResult = (char *) ::util_PostSynchronousEvent(initContext, event);
      
      if (charResult != nsnull) {
	urlString = ::util_NewStringUTF(env, (const char *) charResult);
      }
      else {
	::util_ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned nsnull");
	return nsnull;
      }
      
      nsCRT::free(charResult);
    }
    
    return urlString;
}
