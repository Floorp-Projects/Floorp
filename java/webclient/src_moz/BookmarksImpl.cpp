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

#include "org_mozilla_webclient_impl_wrapper_0005fnative_BookmarksImpl.h"

#include "rdf_util.h"
#include "ns_util.h"

#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeStartup
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeStartup: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    nsresult rv;
    
    rv = rdf_startup();
    
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't initialize bookmarks.");
        return;
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeStartup: exiting\n"));
}

JNIEXPORT void JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeShutdown
(JNIEnv *env, jobject obj, jint nativeContext)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeShutdown: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    nsresult rv;
    
    rv = rdf_shutdown();
    
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Can't shutdown bookmarks.");
        return;
    }
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeShutdown: exiting\n"));
}


JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeAddBookmark
(JNIEnv *, jobject, jint, jobject)
{

}

JNIEXPORT jint JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeGetBookmarks
(JNIEnv *env, jobject obj, jint nativeContext)
{
    jint result = -1;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeGetBookmarks: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    PR_ASSERT(kNC_BookmarksRoot);

    result = (jint) kNC_BookmarksRoot.get();
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeGetBookmarks: exiting\n"));

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_BookmarksImpl_nativeNewRDFNode
(JNIEnv *env, jobject obj, jint nativeContext, jstring urlString, 
 jboolean isFolder)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeNewRDFNode: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    jint result = -1;
    
    PRUnichar *url = (PRUnichar *) ::util_GetStringChars(env, urlString);
    if (!url) {
        ::util_ThrowExceptionToJava(env, "Exception: can't get new RDFNode, can't create url string");
        return result;
    }
    
    nsresult rv;
    nsCOMPtr<nsIRDFResource> newNode;
    
    rv = gRDF->GetUnicodeResource(nsDependentString(url), 
                                  getter_AddRefs(newNode));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create new nsIRDFResource.");
        return result;
    }

    if (isFolder) {
        rv = gRDFCU->MakeSeq(gBookmarksDataSource, newNode, nsnull);
        if (NS_FAILED(rv)) {
            ::util_ThrowExceptionToJava(env, "Exception: unable to make new folder as a sequence.");
            return result;
        }
        rv = gBookmarksDataSource->Assert(newNode, kRDF_type, 
                                          kNC_Folder, PR_TRUE);
        if (rv != NS_OK) {
            ::util_ThrowExceptionToJava(env, "Exception: unable to mark new folder as folder.");
            
            return result;
        }
    }
    
    /*

     * Do the AddRef here.

     */
    
    result = (jint) newNode.get();
    ((nsISupports *)result)->AddRef();
    
    ::util_ReleaseStringChars(env, urlString, url);
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("BookmarksImpl_nativeNewRDFNode: exiting\n"));
    
    return result;
}
