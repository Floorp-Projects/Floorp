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

#include "rdf_util.h"
#include "jni_util.h"


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
    nsresult rv;
    jint result = -1;

    rv = rdf_InitRDFUtils();
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: can't initialize RDF Utils");
        return result;
    }
        
    result = (jint) kNC_BookmarksRoot.get();
    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_BookmarksImpl_nativeNewRDFNode
(JNIEnv *env, jobject obj, jstring urlString, jboolean isFolder)
{
    nsCOMPtr<nsIRDFResource> newNode;
    nsresult rv;
    jint result = -1;
	nsAutoString uri("NC:BookmarksRoot");
    
    const char *url = ::util_GetStringUTFChars(env, urlString);
	uri.Append("#$");
	uri.Append(url);
    printf("debug: edburns: nativeNewRDFNode: url: %s\n", url);
    
    rv = gRDF->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(newNode));
    ::util_ReleaseStringUTFChars(env, urlString, url);
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

    result = (jint)newNode.get();
    ((nsISupports *)result)->AddRef();

    return result;
}
