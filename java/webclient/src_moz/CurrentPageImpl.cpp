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
 *               Kyle Yuan <kyle.yuan@sun.com>
 */

/*
 * CurrentPageImpl.cpp
 */

#include "org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl.h"

#include "ns_util.h"
#include "rdf_util.h"
#include "NativeBrowserControl.h"
#include "EmbedWindow.h"

#include "nsCRT.h"

#if 0 // convenience

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed nativeGetSelection");
        return;
    }
    
    nsresult rv = nativeBrowserControl->mWindow->CopySelection();
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't get Selection from browser");
        return;
    }

}

#endif // if 0

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetSelection
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject selection)
{
    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed nativeGetSelection");
        return;
    }

    if (selection == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null Selection object passed to nativeGetSelection");
        return;
    }
    nsresult rv = nativeBrowserControl->mWindow->GetSelection(env,
                                                              selection);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't get Selection from browser");
        return;
    }

    
}

#if 0 // convenience

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeHighlightSelection
(JNIEnv *env, jobject obj, jint nativeBCPtr, jobject startContainer, jobject endContainer, jint startOffset, jint endOffset)
{
    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeHighlightSelection");
        return;
    }

    PR_ASSERT(nativeBrowserControl->initComplete);

    wsHighlightSelectionEvent *actionEvent = new wsHighlightSelectionEvent(env, nativeBrowserControl, startContainer, endContainer, (PRInt32) startOffset, (PRInt32) endOffset);
    PLEvent *event = (PLEvent *) *actionEvent;
    ::util_PostSynchronousEvent(nativeBrowserControl, event);
}


JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeClearAllSelections
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl *nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to nativeClearAllSelections");
        return;
    }

    PR_ASSERT(nativeBrowserControl->initComplete);

    wsClearAllSelectionEvent *actionEvent = new wsClearAllSelectionEvent(nativeBrowserControl);

    PLEvent *event = (PLEvent *) *actionEvent;
    ::util_PostSynchronousEvent(nativeBrowserControl, event);
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindInPage
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindInPage
(JNIEnv *env, jobject obj, jint nativeBCPtr, jstring searchString, jboolean forward, jboolean matchCase)
{

  NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;


  jstring searchStringGlobalRef = (jstring) ::util_NewGlobalRef(env, searchString);
  if (!searchStringGlobalRef) {
      nativeBrowserControl->initFailCode = kFindComponentError;
      ::util_ThrowExceptionToJava(env, "Exception: Can't create global ref for search string");
      return;
  }

  if (nativeBrowserControl->initComplete) {
     wsFindEvent * actionEvent = new wsFindEvent(nativeBrowserControl, searchStringGlobalRef,
                                                 forward, matchCase);
      PLEvent           * event       = (PLEvent*) *actionEvent;
      ::util_PostEvent(nativeBrowserControl, event);
    }


}



/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindNextInPage
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeFindNextInPage
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{

  NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
  //First get the FindComponent object

  PRBool found = PR_TRUE;

  if (nativeBrowserControl->initComplete) {
      wsFindEvent * actionEvent = new wsFindEvent(nativeBrowserControl);
      PLEvent           * event       = (PLEvent*) *actionEvent;
      ::util_PostEvent(nativeBrowserControl, event);
  }

}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetCurrentURL
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetCurrentURL
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    JNIEnv  *   pEnv = env;
    jobject     jobj = obj;
    char    *   charResult = nsnull;
    jstring     urlString = nsnull;

    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellGetURL");
        return nsnull;
    }

    if (nativeBrowserControl->initComplete) {
        wsGetURLEvent   * actionEvent = new wsGetURLEvent(nativeBrowserControl);
        PLEvent         * event       = (PLEvent*) *actionEvent;

        charResult = (char *) ::util_PostSynchronousEvent(nativeBrowserControl, event);

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

JNIEXPORT jobject JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetDOM
(JNIEnv *env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    jobject result = nsnull;
    jlong documentLong = nsnull;
    jclass clazz = nsnull;
    jmethodID mid = nsnull;

    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null nativeBCPtr passed to raptorWebShellGetDOM");
        return nsnull;
    }
    if (nsnull == nativeBrowserControl->currentDocument ||
        nsnull == (documentLong = (jlong) nativeBrowserControl->currentDocument.get())){
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
    PLEvent         * event       = (PLEvent*) *actionEvent;
    result = (jobject) ::util_PostSynchronousEvent(nativeBrowserControl, event);


    return result;
}


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSource
 * Signature: ()Ljava/lang/String;
 */

/* PENDING(ashuk): remove this from here and in the motif directory
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetSource
(JNIEnv * env, jobject jobj)
{
    jstring result = nsnull;

    return result;
}
*/


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSourceBytes
 * Signature: ()[B
 */

/* PENDING(ashuk): remove this from here and in the motif directory
JNIEXPORT jbyteArray JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeGetSourceBytes
(JNIEnv * env, jobject jobj, jint nativeBCPtr, jboolean viewMode)
{


  NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

  if (nativeBrowserControl->initComplete) {
      wsViewSourceEvent * actionEvent =
          new wsViewSourceEvent(nativeBrowserControl->docShell, ((JNI_TRUE == viewMode)? PR_TRUE : PR_FALSE));
      PLEvent       * event       = (PLEvent*) *actionEvent;

      ::util_PostEvent(nativeBrowserControl, event);
  }

    jbyteArray result = nsnull;
    return result;
}
*/


/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeResetFind
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeResetFind
(JNIEnv * env, jobject obj, jint nativeBCPtr)
{
  NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;

}


#endif // if 0

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeSelectAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativeSelectAll
(JNIEnv * env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    
    if (nativeBrowserControl == nsnull) {
        ::util_ThrowExceptionToJava(env, "Exception: null passed to nativeSelectAll");
        return;
    }
    nsresult rv = nativeBrowserControl->mWindow->SelectAll();
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: Can't selectAll");
        return;
    }
}

#if 0 // convenience

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativePrint
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativePrint
(JNIEnv * env, jobject obj, jint nativeBCPtr)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    if (nativeBrowserControl->initComplete) {
        wsPrintEvent * actionEvent = new wsPrintEvent(nativeBrowserControl);
        PLEvent         * event       = (PLEvent*) *actionEvent;
        ::util_PostEvent(nativeBrowserControl, event);
    }
}

/*
 * Class:     org_mozilla_webclient_impl_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativePrintPreview
 * Signature: (IZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_CurrentPageImpl_nativePrintPreview
(JNIEnv * env, jobject obj, jint nativeBCPtr, jboolean preview)
{
    NativeBrowserControl* nativeBrowserControl = (NativeBrowserControl *) nativeBCPtr;
    if (nativeBrowserControl->initComplete) {
        wsPrintPreviewEvent * actionEvent = new wsPrintPreviewEvent(nativeBrowserControl, preview);
        PLEvent         * event       = (PLEvent*) *actionEvent;
        ::util_PostEvent(nativeBrowserControl, event);
    }
}

# endif // if 0
