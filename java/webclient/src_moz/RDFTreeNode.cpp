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

#include "org_mozilla_webclient_impl_wrapper_0005fnative_RDFTreeNode.h"

#include "rdf_util.h"
#include "rdf_progids.h"
#include "ns_util.h"
#include "wsRDFObserver.h"

#include <nsIServiceManager.h>
#include <nsString.h> // for nsCAutoString

#include <prlog.h> // for PR_ASSERT
#include <nsRDFCID.h> // for NS_RDFCONTAINER_CID


static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kSupportsArrayCID, NS_SUPPORTSARRAY_CID);

//
// Local function prototypes
//

// 
// JNI methods
//

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsLeaf
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeIsLeaf: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    jboolean result = JNI_FALSE;

    PRInt32 count;
    nsresult rv;
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return nsnull;
    }
    result = (count == 0) ? JNI_TRUE : JNI_FALSE;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeIsLeaf: exiting\n"));

    return result;
}

JNIEXPORT jboolean JNICALL Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeIsContainer
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeIsContainer: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRBool prBool;
    
    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeIsContainer: nativeRDFNode is not an RDFResource.");
        return nsnull;
    }
    rv = gRDFCU->IsContainer(gBookmarksDataSource, nodeResource, 
                             &prBool);
    result = (prBool == PR_FALSE) ? JNI_FALSE : JNI_TRUE;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeIsContainer: exiting\n"));
    return result;
}



JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildAt
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode, 
 jint childIndex)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetChildAt: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    
    jint result = -1;
    nsresult rv;
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> child;
    
    rv = rdf_getChildAt(childIndex, parent, getter_AddRefs(child));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildAt: Can't get child.");
        return nsnull;
    }
    result = (jint)child.get();
    ((nsISupports *)result)->AddRef();

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetChildAt: exiting\n"));
    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetChildCount
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetChildCount: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    jint result = -1;
    
    PRInt32 count;
    nsresult rv;
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;

    rv = rdf_getChildCount(parent, &count);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildCount: Can't get child count.");
        return result;
    }
    result = (jint) count;

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetChildCount: exiting\n"));

    return result;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeGetIndex
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode, 
 jint childRDFNode)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetIndex: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    jint result = -1;
    PRInt32 index;
    nsresult rv;
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> child = (nsIRDFResource *) childRDFNode;
    
    rv = rdf_getIndexOfChild(parent, child, &index);
    result = (jint) index;
    
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeGetIndex: exiting\n"));

    return result;
}

JNIEXPORT jstring JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeToString
(JNIEnv *env, jobject obj, jint nativeContext, jint nativeRDFNode)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeToString: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    
    nsCOMPtr<nsIRDFResource> currentResource = 
        (nsIRDFResource *) nativeRDFNode;
    nsCOMPtr<nsIRDFNode> node;
    nsCOMPtr<nsIRDFLiteral> literal;
    PRBool isContainer = PR_FALSE;
    jstring result = nsnull;
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

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeToString: exiting\n"));

    return result;
}

JNIEXPORT void JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeInsertElementAt
(JNIEnv *env, jobject obj, jint nativeContext, jint parentRDFNode, 
 jint childRDFNode, jobject childProps, jint childIndex)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeInsertElementAt: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);
    PR_ASSERT(childProps); // PENDING(edburns): do we need to NewGlobalRef this?

    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) parentRDFNode;
    nsCOMPtr<nsIRDFResource> newChild = (nsIRDFResource *) childRDFNode;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsCOMPtr<nsIRDFLiteral> urlLiteral;

    jstring name;
    const jchar *nameJchar;
    
    jstring url;
    const jchar *urlJchar;

    nsresult rv;
    PRBool isContainer;
    
    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &isContainer);

    // PENDING(edburns): I don't think we can throw exceptions from
    // here, no?
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: RDFResource is not a container.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

    // get a container in order to create a child
    container = do_CreateInstance(kRDFContainerCID);

    if (!container) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create container.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create container.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

    // pull the info from the properties object and add it to the new
    // node.
    if (nsnull == (name = (jstring) ::util_GetFromPropertiesObject(env, 
                                                                   childProps,
                                                                   BM_NAME_VALUE,
                                                                   (jobject) &(wcContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }
    
    if (nsnull == (nameJchar = ::util_GetStringChars(env, name))) {
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }
    
    if (nsnull == (url = (jstring) ::util_GetFromPropertiesObject(env, childProps,
                                                                  BM_URL_VALUE,
                                                                  (jobject) &(wcContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }
    
    if (nsnull == (urlJchar = ::util_GetStringChars(env, url))) {
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }
    // if we get here, we have valid nameJchar and urlJchar strings.

    // create literals for the name and url
    rv = gRDF->GetLiteral((const PRUnichar *) nameJchar,
                          getter_AddRefs(nameLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

    rv = gRDF->GetLiteral((const PRUnichar *) urlJchar,
                          getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

    // now Assert them to add the to the newChild
    rv = gBookmarksDataSource->Assert(newChild, kNC_Name, nameLiteral, 
                                      PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't add name literal to new node.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

    // + 1 because for some reason the 1 is the first, not 0.
    rv = container->InsertElementAt(newChild, childIndex + 1, PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't insert element into parent container.");
        rv = NS_ERROR_UNEXPECTED;
        goto omwiwnrtnniea_CLEANUP;
    }

 omwiwnrtnniea_CLEANUP:
    ::util_ReleaseStringChars(env, name, nameJchar);
    ::util_ReleaseStringChars(env, url, urlJchar);

    if (NS_FAILED(rv)) {
		::util_ThrowExceptionToJava(env, "Exception: Can't InsertElementAt");
	}

    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeInsertElementAt: exiting\n"));

    return;
}

JNIEXPORT jint JNICALL 
Java_org_mozilla_webclient_impl_wrapper_1native_RDFTreeNode_nativeNewFolder
(JNIEnv *env, jobject obj, jint nativeContext, jint parentRDFNode, 
 jobject childProps)
{
    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeNewFolder: entering\n"));
    NativeWrapperFactory *wcContext = (NativeWrapperFactory *) nativeContext;
    
    PR_ASSERT(wcContext);

    jint result = -1;
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) parentRDFNode;
    nsCOMPtr<nsIRDFResource> newChildFromObserver;
    nsCOMPtr<nsISupportsArray> selectionArray;
    nsCOMPtr<nsISupportsArray> argumentsArray;
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    nsresult rv = NS_ERROR_UNEXPECTED;
    PRBool isContainer;
    nsCOMPtr<nsIRDFObserver> observer = new wsRDFObserver();
    wsRDFObserver *wsO = nsnull;
    
    jstring name;
    const jchar *nameJchar;
    
    jstring url;
    const jchar *urlJchar;
    
    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: RDFResource is not a container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    // pull out the necessary keys from the properties table
    if (nsnull == (name = (jstring) ::util_GetFromPropertiesObject(env, 
                                                                   childProps,
                                                                   BM_NAME_VALUE,
                                                                   (jobject) &(wcContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    if (nsnull == (nameJchar = ::util_GetStringChars(env, name))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    if (nsnull == (url = (jstring) ::util_GetFromPropertiesObject(env, childProps,
                                                                  BM_URL_VALUE,
                                                                (jobject) &(wcContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    if (nsnull == (urlJchar = ::util_GetStringChars(env, url))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    // if we get here, we have valid nameJchar and urlJchar strings.
    
    // use the magic "command interface" as in bookmarks.js
    // http://lxr.mozilla.org/mozilla/source/xpfe/components/bookmarks/resources/bookmarks.js#1190
    
    // set up selection nsISupportsArray
    selectionArray = do_CreateInstance(kSupportsArrayCID);
    if (!selectionArray) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: can't create selection nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    // set up arguments nsISupportsArray
    argumentsArray = do_CreateInstance(kSupportsArrayCID);
    if (argumentsArray) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: can't create arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    // get various arguments (parent, name)
    // kNC_parent
    // kNC_Name
    // kNC_URL
    
    // add parent into selection array
    selectionArray->AppendElement(parent);
    
    // add multiple arguments into arguments array
    argumentsArray->AppendElement(kNC_parent);
    argumentsArray->AppendElement(parent);
    
    rv = gRDF->GetLiteral((const PRUnichar *) nameJchar,
                          getter_AddRefs(nameLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: can't arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    argumentsArray->AppendElement(kNC_Name);
    argumentsArray->AppendElement(nameLiteral);
    
    rv = gRDF->GetLiteral((const PRUnichar *) urlJchar,
                          getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: can't create arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    argumentsArray->AppendElement(kNC_URL);
    argumentsArray->AppendElement(urlLiteral);
    
    // at this point, selectionArray contains the parent
    // and argumentsArray contains arcs and literals for the name and the 
    // url of the node to be inserted.
    
    if (observer) {
        gBookmarksDataSource->AddObserver(observer);
    }
    
    // find out if it's a folder
    if (nsnull != ::util_GetFromPropertiesObject(env, childProps, 
                                                 BM_IS_FOLDER_VALUE, (jobject)
                                                 (jobject) &(wcContext->shareContext))){
        // do the command
        rv = gBookmarksDataSource->DoCommand(selectionArray, 
                                             kNewFolderCommand, 
                                             argumentsArray);
    }
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't execute bookmarks command to add folder.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }

    if (observer) {
        gBookmarksDataSource->RemoveObserver(observer);
        wsO = (wsRDFObserver *) observer.get();
        newChildFromObserver = wsO->getFolder();
        if (newChildFromObserver) {
            result = (jint) newChildFromObserver.get();
            ((nsISupports *)result)->AddRef();
        }
    }

 RNFEHE_CLEANUP:
    ::util_ReleaseStringChars(env, name, nameJchar);
    ::util_ReleaseStringChars(env, url, urlJchar);


    PR_LOG(prLogModuleInfo, PR_LOG_DEBUG, 
           ("RDFTreeNode_nativeNewFolder: exiting\n"));

    return result;
}




//
// Local functions
//
