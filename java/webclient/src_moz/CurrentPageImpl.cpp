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

/*
 * CurrentPageImpl.cpp
 */

#include "CurrentPageImpl.h"

#include "jni_util.h"
#include "nsActions.h"

#include "nsCRT.h"
#include "nsIPresShell.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeCopyCurrentSelectionToSystemClipboard
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    nsCOMPtr<nsIPresShell> presShell;
    nsresult rv;

    rv = initContext->docShell->GetPresShell(getter_AddRefs(presShell));
    // PENDING() should this be done using an nsActionEvent subclass?

    if (NS_FAILED(rv)) {
        initContext->initFailCode = kHistoryWebShellError;
        ::util_ThrowExceptionToJava(env, "Exception: can't Copy to Clipboard");
        return;
    }

    presShell->DoCopy();

    /***

        This looks like the right way to do it, but as of 01/13/00, it
        doesn't work.  See a post on n.p.m.embedding:

        Message-ID: <85ll4n$nli$1@nnrp1.deja.com>


     **/
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindInPage
 * Signature: (Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindInPage
(JNIEnv *, jobject, jstring, jboolean, jboolean)
{

    /****
    
    As of 01/13/00, Find is blocked on this post to n.p.m.embedding:

    Message-ID: <85ln9l$p97$1@nnrp1.deja.com>


     ***/
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeFindNextInPage
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeFindNextInPage
(JNIEnv *, jobject, jboolean)
{

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
        nsISessionHistory *yourHistory;
        nsresult rv;
        
        rv = initContext->webShell->GetSessionHistory(yourHistory);
        
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Exception: can't get SessionHistory from webshell");
            return urlString;
        }

		wsGetURLEvent	* actionEvent = new wsGetURLEvent(yourHistory);
        PLEvent	   	  	* event       = (PLEvent*) *actionEvent;

		charResult = (char *) ::util_PostSynchronousEvent(initContext, event);

		if (charResult != nsnull) {
			urlString = ::util_NewStringUTF(env, (const char *) charResult);
		}
		else {
			::util_ThrowExceptionToJava(env, "raptorWebShellGetURL Exception: GetURL() returned NULL");
			return nsnull;
		}

        nsCRT::free(charResult);
	}

	return urlString;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSource
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSource
(JNIEnv *, jobject)
{
    jstring result = nsnull;
    
    return result;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeGetSourceBytes
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeGetSourceBytes
(JNIEnv *, jobject)
{
    jbyteArray result = nsnull;

    return result;
}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeResetFind
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeResetFind
(JNIEnv *, jobject)
{

}

/*
 * Class:     org_mozilla_webclient_wrapper_0005fnative_CurrentPageImpl
 * Method:    nativeSelectAll
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_webclient_wrapper_1native_CurrentPageImpl_nativeSelectAll
(JNIEnv *, jobject)
{

}
