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

#include "RDFEnumeration.h"

#include "RDFActionEvents.h"

#include "rdf_util.h"
#include "rdf_progids.h"
#include "ns_util.h"

#include "nsIRDFContainer.h"
#include "nsIServiceManager.h"

#include "prlog.h" // for PR_ASSERT

#include "nsRDFCID.h" // for NS_RDFCONTAINER_CID

static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);

// 
// JNI methods
//

JNIEXPORT jboolean JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFEnumeration_nativeHasMoreElements
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jboolean result = JNI_FALSE;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeIsContainer");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't see if isContainer");
        return result;
    }
    wsRDFHasMoreElementsEvent *actionEvent = 
        new wsRDFHasMoreElementsEvent(initContext,
                                      (PRUint32) nativeRDFNode,
                                      (void *) obj);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (0 != voidResult) ? JNI_TRUE : JNI_FALSE;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFEnumeration_nativeNextElement
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jint result = -1;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeIsContainer");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't see if isContainer");
        return result;
    }
    wsRDFNextElementEvent *actionEvent = 
        new wsRDFNextElementEvent(initContext,
                                  (PRUint32) nativeRDFNode,
                                  (void *) obj);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (jint) voidResult;
    
    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFEnumeration_nativeFinalize
(JNIEnv *env, jobject obj, jint webShellPtr)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeFinalize");
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't finalize");
    }
    wsRDFFinalizeEvent *actionEvent = 
        new wsRDFFinalizeEvent((void *) obj);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    if (NS_FAILED((nsresult) voidResult)) {
		::util_ThrowExceptionToJava(env, "Exception: Can't Finalize");
	}
    return;
}



