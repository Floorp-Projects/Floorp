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

/*
 * CurrentPageImpl.cpp
 */

#include "CurrentPageImpl.h"

#include "CurrentPageActionEvents.h"

#include "ns_util.h"
#include "rdf_util.h"

#include "nsCRT.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    
    if (initContext->initComplete) {
        wsCopySelectionEvent * actionEvent = new wsCopySelectionEvent(initContext);
        PLEvent			* event       = (PLEvent*) *actionEvent;      
        ::util_PostEvent(initContext, event);
    }	
 
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindInPage
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindInPage
(JNIEnv *env, jobject obj, jint webShellPtr, jstring searchString, jboolean forward, jboolean matchCase)
{

  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

  
  jstring searchStringGlobalRef = (jstring) ::util_NewGlobalRef(env, searchString);
  if (!searchStringGlobalRef) {
      initContext->initFailCode = kFindComponentError;
      ::util_ThrowExceptionToJava(env, "Exception: Can't create global ref for search string");
      return;
  }

  if (initContext->initComplete) {
     wsFindEvent * actionEvent = new wsFindEvent(initContext, searchStringGlobalRef, 
                                                 forward, matchCase);
      PLEvent			* event       = (PLEvent*) *actionEvent;      
      ::util_PostEvent(initContext, event);
    }


}



/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindNextInPage
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindNextInPage
(JNIEnv *env, jobject obj, jint webShellPtr)
{

  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
  //First get the FindComponent object
  
  PRBool found = PR_TRUE;

  if (initContext->initComplete) {
      wsFindEvent * actionEvent = new wsFindEvent(initContext);
      PLEvent			* event       = (PLEvent*) *actionEvent;      
      ::util_PostEvent(initContext, event);
  }

}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetCurrentURL
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL
(JNIEnv *env, jobject obj, jint webShellPtr)
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
		wsGetURLEvent	* actionEvent = new wsGetURLEvent(initContext);
        PLEvent	   	  	* event       = (PLEvent*) *actionEvent;
        
		charResult = (char *) ::util_PostSynchronousEvent(initContext, event);
        
		if (charResult != nsnull) {
			urlString = ::util_NewStringUTF(env, (const char *) charResult);
		}
		else {
			::util_ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned NULL");
			return nsnull;
		}
        
        nsMemory::Free(charResult);
	}
    
	return urlString;
}

JNIEXPORT jobject JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetDOM
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    jobject result = nsnull;
    jlong documentLong = nsnull;
    jclass clazz = nsnull;
    jmethodID mid = nsnull;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to raptorWebShellGetDOM");
		return nsnull;
	}
    if (nsnull == initContext->currentDocument ||
        nsnull == (documentLong = (jlong) initContext->currentDocument.get())){
        return nsnull;
    }
    
	if (nsnull == (clazz = ::util_FindClass(env, 
                                            "org/mozilla/dom/DOMAccessor"))) {
		::util_ThrowExceptionToJava(env, "Exception: Can't get DOMAccessor class");
		return nsnull;
	}
    if (nsnull == (mid = env->GetStaticMethodID(clazz, "getNodeByHandle",
                                                "(J)Lorg/w3c/dom/Node;"))) {
		::util_ThrowExceptionToJava(env, "Exception: Can't get DOM Node.");
		return nsnull;
	}

    wsGetDOMEvent * actionEvent = new wsGetDOMEvent(env, clazz, mid, documentLong);
    PLEvent			* event       = (PLEvent*) *actionEvent;      
    result = (jobject) ::util_PostSynchronousEvent(initContext, event);

    
    return result;
}


/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSource
 * Signature: ()Ljava/lang/String;
 */

/* PENDING(ashuk): remove this from here and in the motif directory
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSource
(JNIEnv * env, jobject jobj)
{
    jstring result = nsnull;
    
    return result;
}
*/


/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSourceBytes
 * Signature: ()[B
 */

/* PENDING(ashuk): remove this from here and in the motif directory
JNIEXPORT jbyteArray JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSourceBytes
(JNIEnv * env, jobject jobj, jint webShellPtr, jboolean viewMode)
{


  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

  if (initContext->initComplete) {
      wsViewSourceEvent * actionEvent = 
          new wsViewSourceEvent(initContext->docShell, ((JNI_TRUE == viewMode)? PR_TRUE : PR_FALSE));
      PLEvent	   	* event       = (PLEvent*) *actionEvent;

      ::util_PostEvent(initContext, event);
  }

    jbyteArray result = nsnull;
    return result;
}
*/


/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeResetFind
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeResetFind
(JNIEnv * env, jobject obj, jint webShellPtr)
{
  WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;

}



/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeSelectAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeSelectAll
(JNIEnv * env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
	if (initContext->initComplete) {
        wsSelectAllEvent * actionEvent = new wsSelectAllEvent(initContext);
        PLEvent			* event       = (PLEvent*) *actionEvent;      
        ::util_PostEvent(initContext, event);
    }	
}
