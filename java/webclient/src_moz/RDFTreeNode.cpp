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

#include "rdf_util.h"
#include "rdf_progids.h"
#include "ns_util.h"

#include "nsIServiceManager.h"

#include "prlog.h" // for PR_ASSERT

#include "nsRDFCID.h" // for NS_RDFCONTAINER_CID

static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);

//
// Local function prototypes
//

// 
// JNI methods
//

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeIsLeaf
(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRInt32 count;

    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeIsLeaf: nativeRDFNode is not an RDFResource.");
        return result;
    }
    rv = rdf_getChildCount(nodeResource, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeIsLeaf: can't get child count from nativeRDFNode.");
        return result;
    }
    result = (0 == count) ? JNI_TRUE : JNI_FALSE;

    return result;
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeIsContainer
(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRBool prBool;

    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeIsContainer: nativeRDFNode is not an RDFResource.");
        return result;
    }
    rv = gRDFCU->IsContainer(gBookmarksDataSource, nodeResource, 
                             &prBool);
    result = (prBool == PR_FALSE) ? JNI_FALSE : JNI_TRUE;

    return result;
}



JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetChildAt
(JNIEnv *env, jobject obj, jint nativeRDFNode, jint childIndex)
{
    jint result = -1;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> child;

    rv = rdf_getChildAt(childIndex, parent, getter_AddRefs(child));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildAt: Can't get child.");
        return result;
    }
    result = (jint)child.get();
    ((nsISupports *)result)->AddRef();
    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetChildCount
(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    jint result = -1;
    PRInt32 count;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return result;
    }
    result = (jint)count;
    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeGetIndex
(JNIEnv *env, jobject obj, jint nativeRDFNode, jint childRDFNode)
{
    jint result = -1;
    PRInt32 index;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> child = (nsIRDFResource *) childRDFNode;

    rv = rdf_getIndexOfChild(parent, child, &index);
    result = (jint) index;

    return result;
}

JNIEXPORT jstring JNICALL 
Java_org_mozilla_webclient_wrapper_1native_RDFTreeNode_nativeToString
(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    nsCOMPtr<nsIRDFResource> currentResource = 
        (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFNode> node;
    nsCOMPtr<nsIRDFLiteral> literal;
    jstring result = nsnull;
    PRBool isContainer = PR_FALSE;
    nsresult rv;
    const PRUnichar *textForNode = nsnull;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, currentResource, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeToString: Can't tell if RDFResource is container.");
        return result;
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
                rdf_printArcLabels(currentResource);
                if (prLogModuleInfo) {
                    PR_LOG(prLogModuleInfo, 3, 
                           ("nativeToString: node is not an nsIRDFLiteral.\n"));
                }
            }
        }
        else {
            rdf_printArcLabels(currentResource);
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
(JNIEnv *env, jobject obj, jint parentRDFNode, 
 jint childRDFNode, jint childIndex)
{
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) parentRDFNode;
    nsCOMPtr<nsIRDFResource> newChild = (nsIRDFResource *) childRDFNode;
    nsCOMPtr<nsIRDFContainer> container;
    nsresult rv;
    PRBool isContainer;

    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: RDFResource is not a container.");
        return;
    }

    PR_ASSERT(gComponentManager);

    // get a container in order to create a child
    rv = gComponentManager->CreateInstance(kRDFContainerCID,
                                           nsnull,
                                           NS_GET_IID(nsIRDFContainer),
                                           getter_AddRefs(container));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create container.");
        return;
    }
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create container.");
        return;
    }

    rv = container->InsertElementAt(newChild, childIndex, PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't insert element into parent container.");
        return;
    }
    return;
}




//
// Local functions
//
