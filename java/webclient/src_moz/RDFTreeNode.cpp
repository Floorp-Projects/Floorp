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

#include "RDFTreeNode.h"

#include "RDFActionEvents.h"

#include "rdf_util.h"
#include "rdf_progids.h"
#include "ns_util.h"

#include "nsIServiceManager.h"
#include "nsString.h" // for nsCAutoString

#include "prlog.h" // for PR_ASSERT


//
// Local function prototypes
//

// 
// JNI methods
//

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeIsLeaf
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jint childCount = -1;
    jboolean result = JNI_FALSE;
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeGetChildCount");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't getChildAt");
        return result;
    }

    PRInt32 count;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return nsnull;
    }
    result = (count == 0) ? JNI_TRUE : JNI_FALSE;


    return result;
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeIsContainer
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
    wsRDFIsContainerEvent *actionEvent = new wsRDFIsContainerEvent(initContext,
                                                                   (PRUint32) nativeRDFNode);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (0 != voidResult) ? JNI_TRUE : JNI_FALSE;

    return result;
}



JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetChildAt
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode, 
 jint childIndex)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jint result = -1;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeGetChildAt");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't getChildAt");
        return result;
    }
    wsRDFGetChildAtEvent *actionEvent = 
        new wsRDFGetChildAtEvent(initContext,
                                 (PRUint32) nativeRDFNode,
                                 (PRUint32) childIndex);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (jint) voidResult;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetChildCount
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jint result = -1;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeGetChildCount");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't getChildAt");
        return result;
    }
    PRInt32 count;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return result;
    }
    result = (jint) count;

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetIndex
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode, 
 jint childRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jint result = -1;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeGetChildIndex");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't getChildIndex");
        return result;
    }
    wsRDFGetChildIndexEvent *actionEvent = 
        new wsRDFGetChildIndexEvent(initContext,
                                    (PRUint32) nativeRDFNode, 
                                    (PRUint32) childRDFNode);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    result = (jint) voidResult;
    
    return result;
}

JNIEXPORT jstring JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeToString
(JNIEnv *env, jobject obj, jint webShellPtr, jint nativeRDFNode)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jstring result = nsnull;

	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeToString");
		return result;
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't toString");
        return result;
    }

    nsCOMPtr<nsIRDFResource> currentResource = 
        (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFNode> node;
    nsCOMPtr<nsIRDFLiteral> literal;
    PRBool isContainer = PR_FALSE;
    nsresult rv;
    const PRUnichar *textForNode = nsnull;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, currentResource, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeToString: Can't tell if RDFResource is container.");
        return nsnull;
    }
    
    if (isContainer) {
        // It's a bookmarks folder
        rv = gBookmarksDataSource->GetTarget(currentResource,
                                             kNC_Name, PR_TRUE, 
                                             getter_AddRefs(node));
        // get the name of the folder
        if (rv == 0) {
            // if so, make sure it's an nsIRDFLiteral
            rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                      getter_AddRefs(literal));
            if (NS_SUCCEEDED(rv)) {
                rv = literal->GetValueConst(&textForNode);
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("nativeToString: node is not an nsIRDFLiteral.\n"));
                }
            }
        }
    }
    else {
        // It's a bookmark or a Separator
        rv = gBookmarksDataSource->GetTarget(currentResource,
                                             kNC_URL, PR_TRUE, 
                                             getter_AddRefs(node));
        // See if it has a Name
        if (0 != rv) {
            rv = gBookmarksDataSource->GetTarget(currentResource,
                                                 kNC_Name, PR_TRUE, 
                                                 getter_AddRefs(node));
        }
        
        if (0 == rv) {
            rv = node->QueryInterface(NS_GET_IID(nsIRDFLiteral), 
                                      getter_AddRefs(literal));
            if (NS_SUCCEEDED(rv)) {
                // get the value of the literal
                rv = literal->GetValueConst(&textForNode);
                if (NS_FAILED(rv)) {
                    if (prLogModuleInfo) {
                        PR_LOG(prLogModuleInfo, 3, 
                               ("nativeToString: node doesn't have a value.\n"));
                    }
                }
            }
            else {
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("nativeToString: node is not an nsIRDFLiteral.\n"));
                }
            }
        }
        else {
            if (prLogModuleInfo) {
                PR_LOG(prLogModuleInfo, 3, 
                       ("nativeToString: node doesn't have a URL.\n"));
            }
        }
    }

    if (nsnull != textForNode) {
        nsString * string = new nsString(textForNode);
        int length = 0;
        if (nsnull != string) {
            length = string->Length();
        }

        result = ::util_NewString(env, (const jchar *) textForNode, length);
    }
    else {
        result = ::util_NewStringUTF(env, "");
    }

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeInsertElementAt
(JNIEnv *env, jobject obj, jint webShellPtr, jint parentRDFNode, 
 jint childRDFNode, jobject childProps, jint childIndex)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jobject propsGlobalRef;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeInsertElementAt");
	}
    
    if (!initContext->initComplete) {
        ::util_ThrowExceptionToJava(env, "Exception: can't InsertElementAt");
    }
    propsGlobalRef = ::util_NewGlobalRef(env, childProps);
    wsRDFInsertElementAtEvent *actionEvent = 
        new wsRDFInsertElementAtEvent(initContext,
                                      (PRUint32) parentRDFNode,
                                      (PRUint32) childRDFNode,
                                      (void *) propsGlobalRef,
                                      (PRUint32) childIndex);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    ::util_DeleteGlobalRef(env, propsGlobalRef);
    if (NS_FAILED((nsresult) voidResult)) {
		::util_ThrowExceptionToJava(env, "Exception: Can't InsertElementAt");
	}
    return;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeNewFolder
(JNIEnv *env, jobject obj, jint webShellPtr, jint parentRDFNode, 
 jobject childProps)
{
    WebShellInitContext* initContext = (WebShellInitContext *) webShellPtr;
    void	*	voidResult = nsnull;
    jobject propsGlobalRef;
    jint retVal = 0;
    
	if (initContext == nsnull) {
		::util_ThrowExceptionToJava(env, "Exception: null webShellPtr passed to nativeNewFolder");
	}
    
    if (!initContext->initComplete || !childProps) {
        ::util_ThrowExceptionToJava(env, "Exception: can't NewFolder");
    }
    propsGlobalRef = ::util_NewGlobalRef(env, childProps);
    if (!propsGlobalRef) {
        ::util_ThrowExceptionToJava(env, "Exception: can't NewFolder");
    }
    
    wsRDFNewFolderEvent *actionEvent = 
        new wsRDFNewFolderEvent(initContext,
                                (PRUint32) parentRDFNode,
                                (void *) propsGlobalRef,
                                (PRUint32 *) &retVal);
    PLEvent	   	* event       = (PLEvent*) *actionEvent;
    
    voidResult = ::util_PostSynchronousEvent(initContext, event);
    ::util_DeleteGlobalRef(env, propsGlobalRef);
    if (NS_FAILED((nsresult) voidResult)) {
		::util_ThrowExceptionToJava(env, "Exception: Can't do NewFolder");
	}
    return retVal;
}




//
// Local functions
//
