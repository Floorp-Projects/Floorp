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
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

#include "BookmarksImpl.h"

#include "RDFActionEvents.h"

#include "rdf_util.h"
#include "ns_util.h"

#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_BookmarksImpl_nativeAddBookmark
(JNIEnv *, jobject, jint, jobject)
{

}

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_wrapper_1native_BookmarksImpl_nativeGetBookmarks
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    jint result = -1;
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeGetBookmarks");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't initialize RDF Utils");
        return result;
    }
    
    wsInitBookmarksEvent *actionEvent = new wsInitBookmarksEvent(initContext);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (jint) voidResult;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_BookmarksImpl_nativeNewRDFNode
(JNIEnv *env, jobject obj, jint webShellPtr, jstring urlString, 
 jboolean isFolder)
{
    jint result = -1;
	nsCAutoString uri("NC:BookmarksRoot");
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    
    if (initContext == nsnull) {
      ::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeNewRDFNode");
      return result;
    }
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't get new RDFNode");
        return result;
    }
    
    const char *url = ::util_GetStringUTFChars(env, urlString);
    if (!url) {
        ::util_ThrowExceptionToJava(env, "Exception: can't get new RDFNode, can't create url string");
        return result;
    }
    
    wsNewRDFNodeEvent *actionEvent = new wsNewRDFNodeEvent(initContext,
                                                           url, 
                                                           (PRBool) isFolder);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (jint) voidResult;
    
    ::util_ReleaseStringUTFChars(env, urlString, url);
    return result;
}
