/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsSubscribeDataSource.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIMsgIncomingServer.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

// this is used for notification of observers using nsVoidArray
typedef struct _nsSubscribeNotification {
  nsIRDFDataSource *datasource;
  nsIRDFResource *subject;
  nsIRDFResource *property;
  nsIRDFNode *object;
} nsSubscribeNotification;

nsSubscribeDataSource::nsSubscribeDataSource()
{
	NS_INIT_REFCNT();
}

nsSubscribeDataSource::~nsSubscribeDataSource()
{
}

NS_IMPL_THREADSAFE_ADDREF(nsSubscribeDataSource);
NS_IMPL_THREADSAFE_RELEASE(nsSubscribeDataSource);

NS_IMPL_QUERY_INTERFACE2(nsSubscribeDataSource, nsIRDFDataSource, nsISubscribeDataSource) 

nsresult
nsSubscribeDataSource::Init()
{
    nsresult rv;

    mRDFService = do_GetService(kRDFServiceCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv) && mRDFService, "failed to get rdf service");
    NS_ENSURE_SUCCESS(rv,rv);
    if (!mRDFService) return NS_ERROR_FAILURE;

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "child",getter_AddRefs(kNC_Child));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "Name",getter_AddRefs(kNC_Name));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "LeafName",getter_AddRefs(kNC_LeafName));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "Subscribed",getter_AddRefs(kNC_Subscribed));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetResource(NC_NAMESPACE_URI "ServerType",getter_AddRefs(kNC_ServerType));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mRDFService->GetLiteral(NS_LITERAL_STRING("true").get(),getter_AddRefs(kTrueLiteral));
    NS_ENSURE_SUCCESS(rv,rv);
  
    rv = mRDFService->GetLiteral(NS_LITERAL_STRING("false").get(),getter_AddRefs(kFalseLiteral));
    NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}

NS_IMETHODIMP 
nsSubscribeDataSource::GetURI(char * *aURI)
{
  if ((*aURI = nsCRT::strdup("rdf:subscribe")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  else
    return NS_OK;
}

NS_IMETHODIMP 
nsSubscribeDataSource::GetSource(nsIRDFResource *property, nsIRDFNode *target, PRBool tv, nsIRDFResource **source)
{
    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    *source = nsnull;
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsSubscribeDataSource::GetTarget(nsIRDFResource *source,
                                nsIRDFResource *property,
                                PRBool tv,
                                nsIRDFNode **target /* out */)
{
	nsresult rv = NS_RDF_NO_VALUE;

	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	*target = nsnull;

	// we only have positive assertions in the subscribe data source.
	if (! tv) return NS_RDF_NO_VALUE;

    nsCOMPtr<nsISubscribableServer> server;
    nsXPIDLCString relativePath;

    rv = GetServerAndRelativePathFromResource(source, getter_AddRefs(server), getter_Copies(relativePath));
    if (NS_FAILED(rv) || !server) {
        return NS_RDF_NO_VALUE;
    }

    if (property == kNC_Name.get()) {
        nsCOMPtr<nsIRDFLiteral> name;
        rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2((const char *)relativePath).get(), getter_AddRefs(name));
        NS_ENSURE_SUCCESS(rv,rv);

        if (!name) rv = NS_RDF_NO_VALUE;
        if (rv == NS_RDF_NO_VALUE) return(rv);
        return name->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
    }
    else if (property == kNC_Child.get()) {
        nsXPIDLCString childUri;
        rv = server->GetFirstChildURI(relativePath, getter_Copies(childUri));
        if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;
        if (!(const char *)childUri) return NS_RDF_NO_VALUE;

        nsCOMPtr <nsIRDFResource> childResource;
        rv = mRDFService->GetResource((const char *)childUri, getter_AddRefs(childResource));
        NS_ENSURE_SUCCESS(rv,rv);
        
        return childResource->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
    }
    else if (property == kNC_Subscribed.get()) {
        PRBool isSubscribed;
        rv = server->IsSubscribed(relativePath, &isSubscribed);
        NS_ENSURE_SUCCESS(rv,rv);
    
        if (isSubscribed) {
            *target = kTrueLiteral;
            NS_IF_ADDREF(*target);
            return NS_OK;
        }
        else {
            *target = kFalseLiteral;
            NS_IF_ADDREF(*target);
            return NS_OK;
        }
    }
    else if (property == kNC_ServerType.get()) {
        nsXPIDLCString serverTypeStr;
        rv = GetServerType(server, getter_Copies(serverTypeStr));
        NS_ENSURE_SUCCESS(rv,rv);

        nsCOMPtr<nsIRDFLiteral> serverType;
        rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2((const char *)serverTypeStr).get(), getter_AddRefs(serverType));
        NS_ENSURE_SUCCESS(rv,rv);

        if (!serverType) rv = NS_RDF_NO_VALUE;
        if (rv == NS_RDF_NO_VALUE)	return(rv);
        return serverType->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
    }
    else if (property == kNC_LeafName.get()) {
        nsXPIDLString leafNameStr;
        rv = server->GetLeafName(relativePath, getter_Copies(leafNameStr));
        NS_ENSURE_SUCCESS(rv,rv);
   
        nsCOMPtr<nsIRDFLiteral> leafName;
        rv = mRDFService->GetLiteral(leafNameStr, getter_AddRefs(leafName));
        NS_ENSURE_SUCCESS(rv,rv);

        if (!leafName) rv = NS_RDF_NO_VALUE;
        if (rv == NS_RDF_NO_VALUE)	return(rv);
        return leafName->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
    }
    else {
        // do nothing
    }

	return(NS_RDF_NO_VALUE);
}

nsresult
nsSubscribeDataSource::GetChildren(nsISubscribableServer *server, const char *relativePath, nsISimpleEnumerator** aResult)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(server && aResult, "no server or result");
    if (!server || !aResult) return NS_ERROR_NULL_POINTER;       

    nsCOMPtr<nsISupportsArray> children;
    rv = NS_NewISupportsArray(getter_AddRefs(children));
    NS_ENSURE_SUCCESS(rv,rv);
    if (!children) return NS_ERROR_FAILURE;

    rv = server->GetChildren(relativePath, children);
    // GetChildren() can fail if there are no children
    if (NS_FAILED(rv)) return rv;

    nsISimpleEnumerator* result = new nsArrayEnumerator(children);
    if (!result) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
nsSubscribeDataSource::GetTargets(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsISimpleEnumerator **targets /* out */)
{
	nsresult rv = NS_OK;

	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(targets != nsnull, "null ptr");
	if (! targets)
		return NS_ERROR_NULL_POINTER;

    *targets = nsnull;

	// we only have positive assertions in the file system data source.
	if (!tv) return NS_RDF_NO_VALUE;

    nsCOMPtr<nsISubscribableServer> server;
    nsXPIDLCString relativePath;

    rv = GetServerAndRelativePathFromResource(source, getter_AddRefs(server), getter_Copies(relativePath));
    if (NS_FAILED(rv) || !server) {
	    return NS_NewEmptyEnumerator(targets);
    }

    if (property == kNC_Child.get()) {
        rv = GetChildren(server, relativePath, targets);
        if (NS_FAILED(rv)) {
            return NS_NewEmptyEnumerator(targets);
        }
        return rv;
    }
    else if (property == kNC_LeafName.get()) {
        nsXPIDLString leafNameStr;
        rv = server->GetLeafName(relativePath, getter_Copies(leafNameStr));
        NS_ENSURE_SUCCESS(rv,rv);
    
        nsCOMPtr<nsIRDFLiteral> leafName;
        rv = mRDFService->GetLiteral(leafNameStr, getter_AddRefs(leafName));
        NS_ENSURE_SUCCESS(rv,rv);

        nsISimpleEnumerator* result = new nsSingletonEnumerator(leafName);
        if (!result) return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *targets = result;
        return NS_OK;
    }
    else if (property == kNC_Subscribed.get()) {
        PRBool isSubscribed;
        rv = server->IsSubscribed(relativePath, &isSubscribed);
        NS_ENSURE_SUCCESS(rv,rv);

        nsISimpleEnumerator* result = nsnull;
        if (isSubscribed) {
            result = new nsSingletonEnumerator(kTrueLiteral);
        }
        else {
            result = new nsSingletonEnumerator(kFalseLiteral); 
        }
        if (!result) return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *targets = result;
        return NS_OK;
    }
    else if (property == kNC_Name.get()) {
        nsCOMPtr<nsIRDFLiteral> name;
        rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2((const char *)relativePath).get(), getter_AddRefs(name));
        NS_ENSURE_SUCCESS(rv,rv);

        nsISimpleEnumerator* result = new nsSingletonEnumerator(name);
        if (!result) return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *targets = result;
        return NS_OK;
    }
    else if (property == kNC_ServerType.get()) {
        nsXPIDLCString serverTypeStr;
        rv = GetServerType(server, getter_Copies(serverTypeStr));
        NS_ENSURE_SUCCESS(rv,rv);

        nsCOMPtr<nsIRDFLiteral> serverType;
        rv = mRDFService->GetLiteral(NS_ConvertASCIItoUCS2((const char *)serverTypeStr).get(), getter_AddRefs(serverType));
        NS_ENSURE_SUCCESS(rv,rv);

        nsISimpleEnumerator* result = new nsSingletonEnumerator(serverType);
        if (!result) return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(result);
        *targets = result;
        return NS_OK;
    }
    else {
        // do nothing
    }

	return NS_NewEmptyEnumerator(targets);
}

NS_IMETHODIMP
nsSubscribeDataSource::Assert(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
nsSubscribeDataSource::Unassert(nsIRDFResource *source,
                         nsIRDFResource *property,
                         nsIRDFNode *target)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
nsSubscribeDataSource::Change(nsIRDFResource* aSource,
							 nsIRDFResource* aProperty,
							 nsIRDFNode* aOldTarget,
							 nsIRDFNode* aNewTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
nsSubscribeDataSource::Move(nsIRDFResource* aOldSource,
						   nsIRDFResource* aNewSource,
						   nsIRDFResource* aProperty,
						   nsIRDFNode* aTarget)
{
	return NS_RDF_ASSERTION_REJECTED;
}

nsresult
nsSubscribeDataSource::GetServerType(nsISubscribableServer *server, char **serverType)
{
    nsresult rv;

    if (!server || !serverType) return NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIMsgIncomingServer> incomingServer(do_QueryInterface(server, &rv));
    NS_ENSURE_SUCCESS(rv,rv);
    if (!incomingServer) return NS_ERROR_FAILURE;

    rv = incomingServer->GetType(serverType);
    NS_ENSURE_SUCCESS(rv,rv);
    
    return NS_OK;
}

nsresult
nsSubscribeDataSource::GetServerAndRelativePathFromResource(nsIRDFResource *source, nsISubscribableServer **server, char **relativePath)
{
    nsresult rv = NS_OK;

    const char *sourceURI = nsnull;
    rv = source->GetValueConst(&sourceURI);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(source, &rv));
    // we expect this to fail sometimes, so don't assert
    if (NS_FAILED(rv)) return rv;
    if (!folder) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIMsgIncomingServer> incomingServer;
    rv = folder->GetServer(getter_AddRefs(incomingServer));
    NS_ENSURE_SUCCESS(rv,rv);
    if (!incomingServer) return NS_ERROR_FAILURE;

    rv = incomingServer->QueryInterface(NS_GET_IID(nsISubscribableServer), (void**)server);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!*server) return NS_ERROR_FAILURE;

    nsXPIDLCString serverURI;
    rv = incomingServer->GetServerURI(getter_Copies(serverURI));
    NS_ENSURE_SUCCESS(rv,rv);
 
    PRUint32 serverURILen = nsCRT::strlen((const char *)serverURI);   
    if (serverURILen == nsCRT::strlen(sourceURI)) {
        *relativePath = nsnull;
    }
    else {
        *relativePath = nsCRT::strdup(sourceURI + serverURILen + 1);
        NS_ASSERTION(*relativePath,"no relative path");
        if (!*relativePath) return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsSubscribeDataSource::HasAssertion(nsIRDFResource *source,
                             nsIRDFResource *property,
                             nsIRDFNode *target,
                             PRBool tv,
                             PRBool *hasAssertion /* out */)
{
    nsresult rv = NS_OK;

	NS_PRECONDITION(source != nsnull, "null ptr");
	if (! source)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(property != nsnull, "null ptr");
	if (! property)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(target != nsnull, "null ptr");
	if (! target)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(hasAssertion != nsnull, "null ptr");
	if (! hasAssertion)
		return NS_ERROR_NULL_POINTER;

	// we only have positive assertions in the file system data source.
	*hasAssertion = PR_FALSE;

	if (!tv) return NS_OK;

	if (property == kNC_Child.get()) {
    nsCOMPtr<nsISubscribableServer> server;
    nsXPIDLCString relativePath;

    rv = GetServerAndRelativePathFromResource(source, getter_AddRefs(server), getter_Copies(relativePath));
    if (NS_FAILED(rv) || !server) {
        *hasAssertion = PR_FALSE;
        return NS_OK;
    }

        // not everything has children
        rv = server->HasChildren((const char *)relativePath, hasAssertion);
        NS_ENSURE_SUCCESS(rv,rv);
    }
    else if (property == kNC_Name.get()) {
        // everything has a name
        *hasAssertion = PR_TRUE;
    }
    else if (property == kNC_LeafName.get()) {
        // everything has a leaf name
        *hasAssertion = PR_TRUE;
    }
    else if (property == kNC_Subscribed.get()) {
        // everything is subscribed or not
        *hasAssertion = PR_TRUE;
    }
    else if (property == kNC_ServerType.get()) {
        // everything has a server type
        *hasAssertion = PR_TRUE;
    }
    else {
        // do nothing
    }

	return NS_OK;
}


NS_IMETHODIMP 
nsSubscribeDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsSubscribeDataSource::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsISubscribableServer> server;
    nsXPIDLCString relativePath;

    if (aArc == kNC_Child.get()) {
    rv = GetServerAndRelativePathFromResource(source, getter_AddRefs(server), getter_Copies(relativePath));
    if (NS_FAILED(rv) || !server) {
	    *result = PR_FALSE;
        return NS_OK;
    }

        PRBool hasChildren = PR_FALSE;
        rv = server->HasChildren((const char *)relativePath, &hasChildren);
        NS_ENSURE_SUCCESS(rv,rv);
        *result = hasChildren;
        return NS_OK;
    }
    else if ((aArc == kNC_Subscribed.get()) ||
             (aArc == kNC_LeafName.get()) ||
             (aArc == kNC_ServerType.get()) ||
             (aArc == kNC_Name.get())) {
        *result = PR_TRUE;
        return NS_OK;
    }

    *result = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
nsSubscribeDataSource::ArcLabelsIn(nsIRDFNode *node,
                            nsISimpleEnumerator ** labels /* out */)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsSubscribeDataSource::ArcLabelsOut(nsIRDFResource *source,
				   nsISimpleEnumerator **labels /* out */)
{
    nsresult rv = NS_OK;

    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
	return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(labels != nsnull, "null ptr");
    if (! labels)
	return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsISubscribableServer> server;
    nsXPIDLCString relativePath;

    rv = GetServerAndRelativePathFromResource(source, getter_AddRefs(server), getter_Copies(relativePath));
    if (NS_FAILED(rv) || !server) {
        return NS_NewEmptyEnumerator(labels);
    }

    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv,rv);

    array->AppendElement(kNC_Subscribed);
    array->AppendElement(kNC_Name);
    array->AppendElement(kNC_ServerType);
    array->AppendElement(kNC_LeafName);

    PRBool hasChildren = PR_FALSE;
    rv = server->HasChildren((const char *)relativePath, &hasChildren);
    NS_ENSURE_SUCCESS(rv,rv);

    if (hasChildren) {
        array->AppendElement(kNC_Child);
    }

    nsISimpleEnumerator* result = new nsArrayEnumerator(array);
    if (! result) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *labels = result;
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribeDataSource::GetAllResources(nsISimpleEnumerator** aCursor)
{
	NS_NOTYETIMPLEMENTED("sorry!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsSubscribeDataSource::AddObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

	if (! mObservers)
	{
		nsresult rv;
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		if (NS_FAILED(rv)) return rv;
	}
	mObservers->AppendElement(n);
	return NS_OK;
}


NS_IMETHODIMP
nsSubscribeDataSource::RemoveObserver(nsIRDFObserver *n)
{
    NS_PRECONDITION(n != nsnull, "null ptr");
    if (! n)
        return NS_ERROR_NULL_POINTER;

	if (! mObservers)
		return NS_OK;

	mObservers->RemoveElement(n);
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribeDataSource::GetHasObservers(PRBool *hasObservers)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(hasObservers, "null ptr");
    if (!hasObservers) return NS_ERROR_NULL_POINTER;
    
    if (!mObservers) {
        *hasObservers = PR_FALSE;
        return NS_OK;
    }
    
    PRUint32 count = 0;
    rv = mObservers->Count(&count);
    NS_ENSURE_SUCCESS(rv,rv);

    *hasObservers = (count > 0);
    return NS_OK;
}

NS_IMETHODIMP
nsSubscribeDataSource::GetAllCmds(nsIRDFResource* source,
                                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}

NS_IMETHODIMP
nsSubscribeDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                       PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
nsSubscribeDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}

NS_IMETHODIMP 
nsSubscribeDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
  NS_ASSERTION(PR_FALSE, "Not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsSubscribeDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSubscribeDataSource::NotifyObservers(nsIRDFResource *subject,
                                                nsIRDFResource *property,
                                                nsIRDFNode *object,
                                                PRBool assert, PRBool change)
{
    NS_ASSERTION(!(change && assert),
                 "Can't change and assert at the same time!\n");

    if(mObservers)
    {
        nsSubscribeNotification note = { this, subject, property, object };
        if(change)
            mObservers->EnumerateForwards(changeEnumFunc, &note);
        else if (assert)
            mObservers->EnumerateForwards(assertEnumFunc, &note);
        else
            mObservers->EnumerateForwards(unassertEnumFunc, &note);
  }
    return NS_OK;
}

PRBool
nsSubscribeDataSource::changeEnumFunc(nsISupports *aElement, void *aData)
{
  nsSubscribeNotification* note = (nsSubscribeNotification*)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnChange(note->datasource,
                     note->subject,
                     note->property,
                     nsnull, note->object);
  return PR_TRUE;
}

PRBool
nsSubscribeDataSource::assertEnumFunc(nsISupports *aElement, void *aData)
{
  nsSubscribeNotification* note = (nsSubscribeNotification*)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnAssert(note->datasource,
                     note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool
nsSubscribeDataSource::unassertEnumFunc(nsISupports *aElement, void *aData)
{
  nsSubscribeNotification* note = (nsSubscribeNotification*)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->datasource,
                       note->subject,
                       note->property,
                       note->object);
  return PR_TRUE;
}
