/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSubscribableServer.h"

#include "nsCOMPtr.h"

#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIFolder.h"
#include "prmem.h"

#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFLiteral.h"
#include "nsIServiceManager.h"

#include "nsMsgUtf7Utils.h"
#include "nsMsgUtils.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

MOZ_DECL_CTOR_COUNTER(nsSubscribableServer)

nsSubscribableServer::nsSubscribableServer(void)
{
    NS_INIT_REFCNT();
    mDelimiter = '.';
    mShowFullName = PR_TRUE;
    mTreeRoot = nsnull;
    mStopped = PR_FALSE;
}

nsresult
nsSubscribableServer::Init()
{
    nsresult rv;

    rv = EnsureRDFService();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "child",getter_AddRefs(kNC_Child));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "Subscribed",getter_AddRefs(kNC_Subscribed));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2("true").get(),getter_AddRefs(kTrueLiteral));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2("false").get(),getter_AddRefs(kFalseLiteral));
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

nsSubscribableServer::~nsSubscribableServer(void)
{
    nsresult rv = NS_OK;
#ifdef DEBUG_seth
    printf("free subscribe tree\n");
#endif
    rv = FreeSubtree(mTreeRoot);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to free tree");
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSubscribableServer, nsISubscribableServer)

NS_IMETHODIMP
nsSubscribableServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
	mIncomingServer = aServer;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::GetDelimiter(char *aDelimiter)
{
    if (!aDelimiter) return NS_ERROR_NULL_POINTER;
    *aDelimiter = mDelimiter;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetDelimiter(char aDelimiter)
{
	mDelimiter = aDelimiter;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetAsSubscribed(const char *path)
{
    NS_ASSERTION(path,"no path");
    if (!path) return NS_ERROR_NULL_POINTER;
    nsresult rv = NS_OK;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    node->isSubscribable = PR_TRUE;
    node->isSubscribed = PR_TRUE;

    rv = NotifyChange(node, kNC_Subscribed, node->isSubscribed);
    NS_ENSURE_SUCCESS(rv,rv);

    return rv;
}

NS_IMETHODIMP
nsSubscribableServer::AddTo(const char *aName, PRBool addAsSubscribed, PRBool changeIfExists)
{
    nsresult rv = NS_OK;
    
    if (mStopped) {
#ifdef DEBUG_seth
        printf("stopped!\n");
#endif
        return NS_ERROR_FAILURE;
    }

    SubscribeTreeNode *node = nsnull;

    // todo, shouldn't we pass in addAsSubscribed, for the 
    // default value if we create it?
    rv = FindAndCreateNode(aName, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    if (changeIfExists) {
        node->isSubscribed = addAsSubscribed;
        rv = NotifyChange(node, kNC_Subscribed, node->isSubscribed);
        NS_ENSURE_SUCCESS(rv,rv);
    }

    node->isSubscribable = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
nsSubscribableServer::SetState(const char *path, PRBool state, PRBool *stateChanged)
{
	nsresult rv = NS_OK;
    NS_ASSERTION(path && stateChanged, "no path or stateChanged");
    if (!path || !stateChanged) return NS_ERROR_NULL_POINTER;

    *stateChanged = PR_FALSE;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    NS_ASSERTION(node->isSubscribable, "fix this");
    if (!node->isSubscribable) {
        return NS_OK;
    }

    if (node->isSubscribed == state) {
        return NS_OK;
    }
    else {
        node->isSubscribed = state;
        *stateChanged = PR_TRUE;
        rv = NotifyChange(node, kNC_Subscribed, node->isSubscribed);
        NS_ENSURE_SUCCESS(rv,rv);
    }

    return rv;
}

void
nsSubscribableServer::BuildURIFromNode(SubscribeTreeNode *node, nsCAutoString &uri)
{
    if (node->parent) {
        BuildURIFromNode(node->parent, uri);
        if (node->parent == mTreeRoot) {
            uri += "/";
        }
        else {
            uri += mDelimiter;
        }
    }

    uri += node->name;
    return;
}

nsresult
nsSubscribableServer::NotifyAssert(SubscribeTreeNode *subjectNode, nsIRDFResource *property, SubscribeTreeNode *objectNode)
{
    nsresult rv;

    PRBool hasObservers = PR_TRUE;
    rv = EnsureSubscribeDS();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mSubscribeDS->GetHasObservers(&hasObservers);
    NS_ENSURE_SUCCESS(rv,rv);
    // no need to do all this work, there are no observers
    if (!hasObservers) {
        return NS_OK;
    }
    
    nsCAutoString subjectUri;
    BuildURIFromNode(subjectNode, subjectUri);

    // we could optimize this, since we know that objectUri == subjectUri + mDelimiter + object->name
    // is it worth it?
    nsCAutoString objectUri;
    BuildURIFromNode(objectNode, objectUri);

    nsCOMPtr <nsIRDFResource> subject;
    nsCOMPtr <nsIRDFResource> object;

    rv = EnsureRDFService();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource((const char *)subjectUri, getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mRDFService->GetResource((const char *)objectUri, getter_AddRefs(object));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = Notify(subject, property, object, PR_TRUE, PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

nsresult
nsSubscribableServer::EnsureRDFService()
{
    nsresult rv;

    if (!mRDFService) {
        mRDFService = do_GetService(kRDFServiceCID, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv) && mRDFService, "failed to get rdf service");
        NS_ENSURE_SUCCESS(rv,rv);
        if (!mRDFService) return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult
nsSubscribableServer::NotifyChange(SubscribeTreeNode *subjectNode, nsIRDFResource *property, PRBool value)
{
    nsresult rv;
    nsCOMPtr <nsIRDFResource> subject;

    PRBool hasObservers = PR_TRUE;
    rv = EnsureSubscribeDS();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mSubscribeDS->GetHasObservers(&hasObservers);
    NS_ENSURE_SUCCESS(rv,rv);
    // no need to do all this work, there are no observers
    if (!hasObservers) {
        return NS_OK;
    }

    nsCAutoString subjectUri;
    BuildURIFromNode(subjectNode, subjectUri);

    rv = EnsureRDFService();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource((const char *)subjectUri, getter_AddRefs(subject));
    NS_ENSURE_SUCCESS(rv,rv);

    if (value) {
        rv = Notify(subject,property,kTrueLiteral,PR_FALSE,PR_TRUE);
    }
    else {
        rv = Notify(subject,property,kFalseLiteral,PR_FALSE,PR_TRUE);
    }

    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

nsresult
nsSubscribableServer::EnsureSubscribeDS()
{
    nsresult rv = NS_OK;

    if (!mSubscribeDS) {
        nsCOMPtr<nsIRDFDataSource> ds;

        rv = EnsureRDFService();
        NS_ENSURE_SUCCESS(rv,rv);

        rv = mRDFService->GetDataSource("rdf:subscribe", getter_AddRefs(ds));
        NS_ENSURE_SUCCESS(rv,rv);
        if (!ds) return NS_ERROR_FAILURE;

        mSubscribeDS = do_QueryInterface(ds, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
        if (!mSubscribeDS) return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult
nsSubscribableServer::Notify(nsIRDFResource *subject, nsIRDFResource *property, nsIRDFNode *object, PRBool isAssert, PRBool isChange)
{
    nsresult rv = NS_OK;

    rv = EnsureSubscribeDS();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mSubscribeDS->NotifyObservers(subject, property, object, isAssert, isChange);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

NS_IMETHODIMP
nsSubscribableServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
	mSubscribeListener = aListener;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
	if (!aListener) return NS_ERROR_NULL_POINTER;
	if (mSubscribeListener) {
			*aListener = mSubscribeListener;
			NS_ADDREF(*aListener);
	}
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SubscribeCleanup()
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::StartPopulatingWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer, const char *uri)
{
    mStopped = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::StartPopulating(nsIMsgWindow *aMsgWindow, PRBool aForceToServer)
{
    nsresult rv = NS_OK;

    mStopped = PR_FALSE;

    rv = FreeSubtree(mTreeRoot);
    mTreeRoot = nsnull;
    NS_ENSURE_SUCCESS(rv,rv);
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::StopPopulating(nsIMsgWindow *aMsgWindow)
{
    mStopped = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsSubscribableServer::UpdateSubscribed()
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::Subscribe(const PRUnichar *aName)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::Unsubscribe(const PRUnichar *aName)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::SetShowFullName(PRBool showFullName)
{
	mShowFullName = showFullName;
	return NS_OK;
}
     
nsresult 
nsSubscribableServer::FreeSubtree(SubscribeTreeNode *node)
{
    nsresult rv = NS_OK;

    if (node) {
        // recursively free the children
        if (node->firstChild) {
            // will free node->firstChild      
            rv = FreeSubtree(node->firstChild);
            NS_ENSURE_SUCCESS(rv,rv);
            node->firstChild = nsnull;
        }

        // recursively free the siblings
        if (node->nextSibling) {
            // will free node->nextSibling
            rv = FreeSubtree(node->nextSibling);
            NS_ENSURE_SUCCESS(rv,rv);
            node->nextSibling = nsnull;
        }

#ifdef HAVE_SUBSCRIBE_DESCRIPTION
        NS_ASSERTION(node->description == nsnull, "you need to free the description");
#endif
        CRTFREEIF(node->name);
#if 0 
        node->name = nsnull;
        node->parent = nsnull;
        node->lastChild = nsnull;
        node->cachedChild = nsnull;
#endif

        PR_Free(node);  
    }

    return NS_OK;
}

nsresult 
nsSubscribableServer::CreateNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **result)
{
    NS_ASSERTION(result && name, "result or name is null");
    if (!result || !name) return NS_ERROR_NULL_POINTER;

    *result = (SubscribeTreeNode *) PR_Malloc(sizeof(SubscribeTreeNode));
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;

    (*result)->name = nsCRT::strdup(name);
    if (!(*result)->name) return NS_ERROR_OUT_OF_MEMORY;

    (*result)->parent = parent;
    (*result)->prevSibling = nsnull;
    (*result)->nextSibling = nsnull;
    (*result)->firstChild = nsnull;
    (*result)->lastChild = nsnull;
    (*result)->isSubscribed = PR_FALSE;
    (*result)->isSubscribable = PR_FALSE;
#ifdef HAVE_SUBSCRIBE_DESCRIPTION
    (*result)->description = nsnull;
#endif
#ifdef HAVE_SUBSCRIBE_MESSAGES
    (*result)->messages = 0;
#endif
    (*result)->cachedChild = nsnull;

    if (parent) {
        parent->cachedChild = *result;
    }

    return NS_OK;
}

nsresult
nsSubscribableServer::AddChildNode(SubscribeTreeNode *parent, const char *name, SubscribeTreeNode **child)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(parent && child && name, "parent, child or name is null");
    if (!parent || !child || !name) return NS_ERROR_NULL_POINTER;

    if (!parent->firstChild) {
        // CreateNode will set the parent->cachedChild
        rv = CreateNode(parent, name, child);
        NS_ENSURE_SUCCESS(rv,rv);

        parent->firstChild = *child;
        parent->lastChild = *child;

        rv = NotifyAssert(parent, kNC_Child, *child);
        NS_ENSURE_SUCCESS(rv,rv);   

        return NS_OK;
    }
    else {
        if (parent->cachedChild) {
            if (nsCRT::strcmp(parent->cachedChild->name,name) == 0) {
                *child = parent->cachedChild;
                return NS_OK;
            }
        }

        SubscribeTreeNode *current = parent->firstChild;

        /*
         * insert in reverse alphabetical order
         * this will reduce the # of strcmps
         * since this is faster assuming:
         *  1) the hostinfo.dat feeds us the groups in alphabetical order
         *     since we control the hostinfo.dat file, we can guarantee this.
         *  2) the server gives us the groups in alphabetical order
         *     we can't guarantee this, but it seems to be a common thing
         *
         * because we have firstChild, lastChild, nextSibling, prevSibling
         * we can efficiently reverse the order when dumping to hostinfo.dat
         * or to GetTargets()
         */
        PRInt32 compare = nsCRT::strcmp(current->name, name);

        while (current && (compare != 0)) {
            if (compare < 0) {
                // CreateNode will set the parent->cachedChild
                rv = CreateNode(parent, name, child);
                NS_ENSURE_SUCCESS(rv,rv);

                (*child)->nextSibling = current;
                (*child)->prevSibling = current->prevSibling;
                current->prevSibling = (*child);
                if (!(*child)->prevSibling) {
                    parent->firstChild = (*child);
                }
                else {
                    (*child)->prevSibling->nextSibling = (*child);
                }

                rv = NotifyAssert(parent, kNC_Child, *child);
                NS_ENSURE_SUCCESS(rv,rv);
                return NS_OK;
            }
            current = current->nextSibling;
            if (current) {
                NS_ASSERTION(current->name, "no name!");
                compare = nsCRT::strcmp(current->name,name);
            }
            else {
                compare = -1; // anything but 0, since that would be a match
            }
        }

        if (compare == 0) {
            // already exists;
            *child = current;

            // set the cachedChild
            parent->cachedChild = *child;
            return NS_OK;
        }

        // CreateNode will set the parent->cachedChild
        rv = CreateNode(parent, name, child);
        NS_ENSURE_SUCCESS(rv,rv);

        (*child)->prevSibling = parent->lastChild;
        (*child)->nextSibling = nsnull;
        parent->lastChild->nextSibling = *child;
        parent->lastChild = *child;

        rv = NotifyAssert(parent, kNC_Child, *child);
        NS_ENSURE_SUCCESS(rv,rv);
        return NS_OK;
    }
    return NS_OK;
}

nsresult
nsSubscribableServer::FindAndCreateNode(const char *path, SubscribeTreeNode **result)
{
	nsresult rv = NS_OK;
    NS_ASSERTION(result, "no result");
    if (!result) return NS_ERROR_NULL_POINTER;

    if (!mTreeRoot) {
        nsXPIDLCString serverUri;
        rv = mIncomingServer->GetServerURI(getter_Copies(serverUri));
        NS_ENSURE_SUCCESS(rv,rv);
        // the root has no parent, and its name is server uri
        rv = CreateNode(nsnull, (const char *)serverUri, &mTreeRoot);
        NS_ENSURE_SUCCESS(rv,rv);
    }

    if (!path || (path[0] == '\0')) {
        *result = mTreeRoot;
        return NS_OK;
    }

    char *pathStr = nsCRT::strdup(path);
    char *token = nsnull;
    char *rest = pathStr;
    
    // todo do this only once
    char delimstr[2];
    delimstr[0] = mDelimiter;
    delimstr[1] = '\0';
    
    *result = nsnull;

    SubscribeTreeNode *parent = mTreeRoot;
    SubscribeTreeNode *child = nsnull;

    token = nsCRT::strtok(rest, delimstr, &rest);
    while (token && *token) {
        rv = AddChildNode(parent, token, &child);
        if (NS_FAILED(rv)) {
            CRTFREEIF(pathStr);
	        return rv;
        }
        token = nsCRT::strtok(rest, delimstr, &rest);
        parent = child;
    }   
    CRTFREEIF(pathStr);

    // the last child we add is the result
    *result = child;
	return rv;
}

NS_IMETHODIMP
nsSubscribableServer::HasChildren(const char *path, PRBool *aHasChildren)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aHasChildren, "no aHasChildren");
    if (!aHasChildren) return NS_ERROR_NULL_POINTER;

    *aHasChildren = PR_FALSE;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;
 
    *aHasChildren = (node->firstChild != nsnull);
    return NS_OK;
}


NS_IMETHODIMP
nsSubscribableServer::IsSubscribed(const char *path, PRBool *aIsSubscribed)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aIsSubscribed, "no aIsSubscribed");
    if (!aIsSubscribed) return NS_ERROR_NULL_POINTER;

    *aIsSubscribed = PR_FALSE;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    *aIsSubscribed = node->isSubscribed;
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::GetLeafName(const char *path, PRUnichar **aLeafName)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aLeafName, "no aLeafName");
    if (!aLeafName) return NS_ERROR_NULL_POINTER;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    // XXX TODO FIXME
    // I'm assuming that mShowFullName is true for NNTP, false for IMAP.
    // for imap, the node name is in modified UTF7
    // for news, the path is escaped UTF8
    //
    // when we switch to using the outliner, this hack will go away.
    if (mShowFullName) {
	rv = NS_MsgDecodeUnescapeURLPath(path, aLeafName);
    }
    else {
        rv = CreateUnicodeStringFromUtf7(node->name, aLeafName);
    }
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

NS_IMETHODIMP
nsSubscribableServer::GetFirstChildURI(const char * path, char **aResult)
{
    nsresult rv = NS_OK;
    if (!aResult) return NS_ERROR_NULL_POINTER;
    
    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    // no children
    if (!node->firstChild) return NS_ERROR_FAILURE;
    
    nsCAutoString uri;
    BuildURIFromNode(node->firstChild, uri);

    *aResult = nsCRT::strdup((const char *)uri);
    if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::GetChildren(const char *path, nsISupportsArray *array)
{
    nsresult rv = NS_OK;
    if (!array) return NS_ERROR_NULL_POINTER;

    SubscribeTreeNode *node = nsnull;
    rv = FindAndCreateNode(path, &node);
    NS_ENSURE_SUCCESS(rv,rv);

    NS_ASSERTION(node,"didn't find the node");
    if (!node) return NS_ERROR_FAILURE;

    nsCAutoString uriPrefix;
    NS_ASSERTION(mTreeRoot, "no tree root!");
    if (!mTreeRoot) return NS_ERROR_UNEXPECTED;
    uriPrefix = mTreeRoot->name; // the root's name is the server uri
    uriPrefix += "/";
    if (path && (path[0] != '\0')) {
        uriPrefix += path;
        uriPrefix += mDelimiter;
    }

    // we inserted them in reverse alphabetical order.
    // so pull them out in reverse to get the right order
    // in the subscribe dialog
    SubscribeTreeNode *current = node->lastChild;
    // return failure if there are no children.
    if (!current) return NS_ERROR_FAILURE;  

    while (current) {
        nsCAutoString uri;
        uri = uriPrefix;
        NS_ASSERTION(current->name, "no name");
        if (!current->name) return NS_ERROR_FAILURE;
        uri += current->name;

        nsCOMPtr <nsIRDFResource> res;
        rv = EnsureRDFService();
        NS_ENSURE_SUCCESS(rv,rv);

        // todo, is this creating nsMsgFolders?
        mRDFService->GetResource((const char *)uri, getter_AddRefs(res));
        array->AppendElement(res);
    
        current = current->prevSibling;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::CommitSubscribeChanges()
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::SetSearchValue(const char *searchValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSubscribableServer::GetSupportsSubscribeSearch(PRBool *retVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
