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
 *               Ashutosh Kulkarni <ashuk@eng.sun.com>
 *               Mark Lin <mark.lin@eng.sun.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

/*
 * RDFActionEvents.cpp
 */

#include "RDFActionEvents.h"

#include "rdf_util.h"
#include "ns_util.h"
#include "ns_globals.h"

#include "prlog.h"

#include "nsIComponentManager.h"
#include "nsISupportsArray.h"

#include "nsRDFCID.h" // for NS_RDFCONTAINER_CID

#include "wsRDFObserver.h"
#include "nsString.h" // for nsCAutoString
#include "nsReadableUtils.h" 
#include "nsCRT.h"


static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kSupportsArrayCID, NS_SUPPORTSARRAY_CID);

//
// Local function prototypes
//

/**

 * pull the int for the field nativeEnum from the java object obj.

 */

jint getNativeEnumFromJava(JNIEnv *env, jobject obj, jint nativeRDFNode);

wsInitBookmarksEvent::wsInitBookmarksEvent(WebShellInitContext* yourInitContext) :
        nsActionEvent(),
        mInitContext(yourInitContext)
{
}

void *
wsInitBookmarksEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    rv = rdf_InitRDFUtils();
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: can't initialize RDF Utils");
        return (void *) result;
    }
    result = (void *) kNC_BookmarksRoot.get();

    return result;
} // handleEvent()


wsNewRDFNodeEvent::wsNewRDFNodeEvent(WebShellInitContext* yourInitContext,
                                     const char * yourUrlString,
                                     PRBool yourIsFolder) :
        nsActionEvent(),
        mInitContext(yourInitContext), mUrlString(yourUrlString),
        mIsFolder(yourIsFolder)
{
}

void *
wsNewRDFNodeEvent::handleEvent ()
{
    void *result = nsnull;
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    nsCOMPtr<nsIRDFResource> newNode;
	nsCAutoString uri(mUrlString);
    JNIEnv *env = (JNIEnv*) JNU_GetEnv(gVm, JNI_VERSION);
    
    // PENDING(edburns): does this leak? 
    PRUnichar *uriUni = ToNewUnicode(uri);
    
    rv = gRDF->GetUnicodeResource(nsDependentString(uriUni), getter_AddRefs(newNode));
    nsCRT::free(uriUni);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewRDFNode: can't create new nsIRDFResource.");
        return result;
    }

    if (mIsFolder) {
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

    result = (void *)newNode.get();
    ((nsISupports *)result)->AddRef();

    return result;
} // handleEvent()

wsRDFIsContainerEvent::wsRDFIsContainerEvent(WebShellInitContext* yourInitContext, 
                                             PRUint32 yourNativeRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode)
{
}

void *
wsRDFIsContainerEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) mNativeRDFNode;
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
    
    return (void *) result;
} // handleEvent()

wsRDFGetChildAtEvent::wsRDFGetChildAtEvent(WebShellInitContext* yourInitContext, 
                                           PRUint32 yourNativeRDFNode, 
                                           PRUint32 yourChildIndex) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
        mChildIndex(yourChildIndex)
{
}

void *
wsRDFGetChildAtEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint result = -1;
    nsresult rv;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mNativeRDFNode;
    nsCOMPtr<nsIRDFResource> child;

    rv = rdf_getChildAt(mChildIndex, parent, getter_AddRefs(child));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeGetChildAt: Can't get child.");
        return nsnull;
    }
    result = (jint)child.get();
    ((nsISupports *)result)->AddRef();
    return (void *) result;
} // handleEvent()

wsRDFGetChildIndexEvent::wsRDFGetChildIndexEvent(WebShellInitContext* yourInitContext, 
                                                 PRUint32 yourNativeRDFNode, 
                                                 PRUint32 yourChildRDFNode) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode), 
        mChildRDFNode(yourChildRDFNode)
{
}

void *
wsRDFGetChildIndexEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint result = -1;
    PRInt32 index;
    nsresult rv;
    // PENDING(edburns): assert rdf_InitRDFUtils()
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mNativeRDFNode;
    nsCOMPtr<nsIRDFResource> child = (nsIRDFResource *) mChildRDFNode;
    
    rv = rdf_getIndexOfChild(parent, child, &index);
    result = (jint) index;
    
    return (void *) result;
} // handleEvent()

wsRDFInsertElementAtEvent::wsRDFInsertElementAtEvent(WebShellInitContext* yourInitContext, 
                                                     PRUint32 yourParentRDFNode,
                                                     PRUint32 yourChildRDFNode,
                                                     void *yourChildProperties,
                                                     PRUint32 yourChildIndex) :
        nsActionEvent(),
        mInitContext(yourInitContext), mParentRDFNode(yourParentRDFNode),
        mChildRDFNode(yourChildRDFNode), 
        mChildPropsJobject(yourChildProperties), mChildIndex(yourChildIndex)
{
}

void *
wsRDFInsertElementAtEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mParentRDFNode;
    nsCOMPtr<nsIRDFResource> newChild = (nsIRDFResource *) mChildRDFNode;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsCOMPtr<nsIRDFLiteral> urlLiteral;

    jstring name;
    const jchar *nameJchar;
    
    jstring url;
    const jchar *urlJchar;

    jobject childProps = (jobject) mChildPropsJobject;
    PR_ASSERT(childProps);

    nsresult rv;
    PRBool isContainer;
    
    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &isContainer);

    // PENDING(edburns): I don't think we can throw exceptions from
    // here, no?
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: RDFResource is not a container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

    PR_ASSERT(gComponentManager);

    // get a container in order to create a child
    rv = nsComponentManager::CreateInstance(kRDFContainerCID,
                                           nsnull,
                                           NS_GET_IID(nsIRDFContainer),
                                           getter_AddRefs(container));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }
    rv = container->Init(gBookmarksDataSource, parent);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

    // pull the info from the properties object and add it to the new
    // node.
    if (nsnull == (name = (jstring) ::util_GetFromPropertiesObject(env, 
                                                                   childProps,
                                                                   BM_NAME_VALUE,
                                                                   (jobject)
                                                                   &(mInitContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }
    
    if (nsnull == (nameJchar = ::util_GetStringChars(env, name))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }
    
    if (nsnull == (url = (jstring) ::util_GetFromPropertiesObject(env, childProps,
                                                                  BM_URL_VALUE,
                                                                  (jobject)
                                                                  &(mInitContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }
    
    if (nsnull == (urlJchar = ::util_GetStringChars(env, url))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }
    // if we get here, we have valid nameJchar and urlJchar strings.

    // create literals for the name and url
    rv = gRDF->GetLiteral((const PRUnichar *) nameJchar,
                          getter_AddRefs(nameLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

    rv = gRDF->GetLiteral((const PRUnichar *) urlJchar,
                          getter_AddRefs(urlLiteral));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't create arguments nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

    // now Assert them to add the to the newChild
    rv = gBookmarksDataSource->Assert(newChild, kNC_Name, nameLiteral, 
                                      PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't add name literal to new node.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

    // + 1 because for some reason the 1 is the first, not 0.
    rv = container->InsertElementAt(newChild, mChildIndex + 1, PR_TRUE);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeInsertElementAt: can't insert element into parent container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RIEAEHE_CLEANUP;
    }

 RIEAEHE_CLEANUP:
    ::util_ReleaseStringChars(env, name, nameJchar);
    ::util_ReleaseStringChars(env, url, urlJchar);

    return (void *) rv;
} // handleEvent()

wsRDFNewFolderEvent::wsRDFNewFolderEvent(WebShellInitContext* yourInitContext, 
                                         PRUint32 yourParentRDFNode, 
                                         void *yourChildPropsJobject,
                                         PRUint32 *yourRetVal) :
    nsActionEvent(),
    mInitContext(yourInitContext), mParentRDFNode(yourParentRDFNode),
    mChildPropsJobject(yourChildPropsJobject), mRetVal(yourRetVal)
{
}

/**

 * The adding of bookmark folders is done through the RDF DoCommand
 * interface.  The DoCommand interface creates the nsIRDFResource on
 * your behalf.  We use an nsIRDFObserver to obtain the created resource
 * as the DoCommand executes.

 */

void *
wsRDFNewFolderEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    PR_ASSERT(gComponentManager);
    PR_ASSERT(mRetVal);
    *mRetVal = -1;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    nsCOMPtr<nsIRDFResource> parent = (nsIRDFResource *) mParentRDFNode;
    nsCOMPtr<nsIRDFResource> newChildFromObserver;
    nsCOMPtr<nsISupportsArray> selectionArray;
    nsCOMPtr<nsISupportsArray> argumentsArray;
    nsCOMPtr<nsIRDFLiteral> nameLiteral;
    nsCOMPtr<nsIRDFLiteral> urlLiteral;
    nsresult rv = NS_ERROR_UNEXPECTED;
    PRBool isContainer;
    nsCOMPtr<nsIRDFObserver> o = new wsRDFObserver();
    wsRDFObserver *wsO = nsnull;
    
    jstring name;
    const jchar *nameJchar;
    
    jstring url;
    const jchar *urlJchar;
    
    jobject childProps = (jobject) mChildPropsJobject;
    PR_ASSERT(childProps);
    
    
    rv = gRDFCU->IsContainer(gBookmarksDataSource, parent, 
                             &isContainer);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: RDFResource is not a container.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    // pull out the necessary keys from the properties table
    if (nsnull == (name = (jstring) ::util_GetFromPropertiesObject(env, 
                                                                   childProps,
                                                                   BM_NAME_VALUE,
                                                                   (jobject)
                                                                   &(mInitContext->shareContext)))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    if (nsnull == (nameJchar = ::util_GetStringChars(env, name))) {
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }

    if (nsnull == (url = (jstring) ::util_GetFromPropertiesObject(env, childProps,
                                                                BM_URL_VALUE,
                                                                (jobject)
                                                                &(mInitContext->shareContext)))) {
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
    rv = nsComponentManager::CreateInstance(kSupportsArrayCID,
                                           nsnull,
                                           NS_GET_IID(nsISupportsArray),
                                           getter_AddRefs(selectionArray));
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNewFolder: can't create selection nsISupportsArray.");
        rv = NS_ERROR_UNEXPECTED;
        goto RNFEHE_CLEANUP;
    }
    
    // set up arguments nsISupportsArray
    rv = nsComponentManager::CreateInstance(kSupportsArrayCID,
                                           nsnull,
                                           NS_GET_IID(nsISupportsArray),
                                           getter_AddRefs(argumentsArray));
    if (NS_FAILED(rv)) {
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
    
    if (o) {
        gBookmarksDataSource->AddObserver(o);
    }
    
    // find out if it's a folder
    if (nsnull != ::util_GetFromPropertiesObject(env, childProps, 
                                                 BM_IS_FOLDER_VALUE, (jobject)
                                                 &(mInitContext->shareContext))){
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

    if (o) {
        gBookmarksDataSource->RemoveObserver(o);
        wsO = (wsRDFObserver *) o.get();
        newChildFromObserver = wsO->getFolder();
        if (newChildFromObserver) {
            *mRetVal = (PRUint32) newChildFromObserver.get();
            ((nsISupports *)*mRetVal)->AddRef();
        }
    }

 RNFEHE_CLEANUP:
    ::util_ReleaseStringChars(env, name, nameJchar);
    ::util_ReleaseStringChars(env, url, urlJchar);

    return (void *) rv;
} // handleEvent()


wsRDFHasMoreElementsEvent::wsRDFHasMoreElementsEvent(WebShellInitContext* yourInitContext,
                                                     PRUint32 yourNativeRDFNode,
                                                     void *yourJobject) :
        nsActionEvent(),
        mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
        mJobject(yourJobject)
{
}

void *
wsRDFHasMoreElementsEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    jboolean result = JNI_FALSE;
    PRBool prResult = PR_FALSE;
    // assert -1 != nativeRDFNode
    jint nativeEnum;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    if (-1 == (nativeEnum = getNativeEnumFromJava(env, (jobject) mJobject, 
                                                  mNativeRDFNode))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeHasMoreElements: Can't get nativeEnum from nativeRDFNode.");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    nsCOMPtr<nsISimpleEnumerator> enumerator = (nsISimpleEnumerator *)nativeEnum;
    rv = enumerator->HasMoreElements(&prResult);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeHasMoreElements: Can't ask nsISimpleEnumerator->HasMoreElements().");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    result = (PR_FALSE == prResult) ? JNI_FALSE : JNI_TRUE;
    
    return (void *) result;
    
} // handleEvent()

wsRDFNextElementEvent::wsRDFNextElementEvent(WebShellInitContext* yourInitContext,
                                             PRUint32 yourNativeRDFNode,
                                             void *yourJobject) :
    nsActionEvent(),
    mInitContext(yourInitContext), mNativeRDFNode(yourNativeRDFNode),
    mJobject(yourJobject)
{
}

void *
wsRDFNextElementEvent::handleEvent ()
{
    if (!mInitContext) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsresult rv;
    jint result = -1;
    PRBool hasMoreElements = PR_FALSE;
    // assert -1 != nativeRDFNode
    jint nativeEnum;
    nsCOMPtr<nsISupports> supportsResult;
    nsCOMPtr<nsIRDFNode> nodeResult;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);
    
    if (-1 == (nativeEnum = getNativeEnumFromJava(env, (jobject) mJobject, 
                                                  mNativeRDFNode))) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNextElement: Can't get nativeEnum from nativeRDFNode.");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    nsCOMPtr<nsISimpleEnumerator> enumerator = (nsISimpleEnumerator *)nativeEnum;
    rv = enumerator->HasMoreElements(&hasMoreElements);
    if (NS_FAILED(rv)) {
        ::util_ThrowExceptionToJava(env, "Exception: nativeNextElement: Can't ask nsISimpleEnumerator->HasMoreElements().");
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    if (!hasMoreElements) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    rv = enumerator->GetNext(getter_AddRefs(supportsResult));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("Exception: nativeNextElement: Can't get next from enumerator.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    // make sure it's an RDFNode
    rv = supportsResult->QueryInterface(NS_GET_IID(nsIRDFNode), 
                                        getter_AddRefs(nodeResult));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("Exception: nativeNextElement: next from enumerator is not an nsIRDFNode.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    
    result = (jint)nodeResult.get();
    ((nsISupports *)result)->AddRef();
    return (void *) result;
} // handleEvent()

wsRDFFinalizeEvent::wsRDFFinalizeEvent(void *yourJobject) :
        nsActionEvent(),
        mJobject(yourJobject)
{
}

void *
wsRDFFinalizeEvent::handleEvent ()
{
    if (!mJobject) {
        return (void *) NS_ERROR_UNEXPECTED;
    }
    jint nativeEnum, nativeContainer;
    JNIEnv *env = (JNIEnv *) JNU_GetEnv(gVm, JNI_VERSION);

    // release the nsISimpleEnumerator
    if (-1 == (nativeEnum = 
               ::util_GetIntValueFromInstance(env, (jobject) mJobject, 
                                              "nativeEnum"))) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("nativeFinalize: Can't get fieldID for nativeEnum.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsISimpleEnumerator> enumerator = 
        (nsISimpleEnumerator *) nativeEnum;
    ((nsISupports *)enumerator.get())->Release();
    
    // release the nsIRDFContainer
    if (-1 == (nativeContainer = 
               ::util_GetIntValueFromInstance(env, (jobject) mJobject, 
                                              "nativeContainer"))) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("nativeFinalize: Can't get fieldID for nativeContainerFieldID.\n"));
        }
        return (void *) NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<nsIRDFContainer> container = 
        (nsIRDFContainer *) nativeContainer;
    ((nsISupports *)container.get())->Release();

    return (void *) NS_OK;
} // handleEvent()

//
// Local functions
//

jint getNativeEnumFromJava(JNIEnv *env, jobject obj, jint nativeRDFNode)
{
    nsresult rv;
    jint result = -1;

    result = ::util_GetIntValueFromInstance(env, obj, "nativeEnum");

    // if the field has been initialized, just return the value
    if (-1 != result) {
        // NORMAL EXIT 1
        return result;
    }

    // else, we need to create the enum
    nsCOMPtr<nsIRDFNode> node = (nsIRDFNode *) nativeRDFNode;
    nsCOMPtr<nsIRDFResource> nodeResource;
    nsCOMPtr<nsIRDFContainer> container;
    nsCOMPtr<nsISimpleEnumerator> enumerator;

    rv = node->QueryInterface(NS_GET_IID(nsIRDFResource), 
                              getter_AddRefs(nodeResource));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Argument nativeRDFNode isn't an nsIRDFResource.\n"));
        }
        return -1;
    }

    PR_ASSERT(gComponentManager);
    
    // get a container in order to get the enum
    rv = nsComponentManager::CreateInstance(kRDFContainerCID,
                                           nsnull,
                                           NS_GET_IID(nsIRDFContainer),
                                           getter_AddRefs(container));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("recursiveResourceTraversal: can't get a new container\n"));
        }
        return -1;
    }
    
    rv = container->Init(gBookmarksDataSource, nodeResource);
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Can't Init container.\n"));
        }
        return -1;
    }

    rv = container->GetElements(getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) {
        if (prLogModuleInfo) {
            PR_LOG(prLogModuleInfo, 3, 
                   ("getNativeEnumFromJava: Can't get enumeration from container.\n"));
        }
        return -1;
    }

    // IMPORTANT: Store the enum back into java
    ::util_SetIntValueForInstance(env,obj,"nativeEnum",(jint)enumerator.get());
    // IMPORTANT: make sure it doesn't get deleted when it goes out of scope
    ((nsISupports *)enumerator.get())->AddRef(); 

    // PENDING(edburns): I'm not sure if we need to keep the
    // nsIRDFContainer from being destructed in order to maintain the
    // validity of the nsISimpleEnumerator that came from the container.
    // Just to be safe, I'm doing so.
    ::util_SetIntValueForInstance(env, obj, "nativeContainer", 
                                  (jint) container.get());
    ((nsISupports *)container.get())->AddRef();
    
    // NORMAL EXIT 2
    result = (jint)enumerator.get();    
    return result;
}
