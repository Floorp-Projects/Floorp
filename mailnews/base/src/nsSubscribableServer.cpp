/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsSubscribableServer.h"

#include "nsCOMPtr.h"

#include "nsIRDFService.h"
#include "nsRDFCID.h"

#include "nsEnumeratorUtils.h" 
#include "nsXPIDLString.h"
#include "nsIFolder.h"

#if defined(DEBUG_sspitzer) || defined(DEBUG_seth)
#define DEBUG_SUBSCRIBE 1
#endif

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

MOZ_DECL_CTOR_COUNTER(nsSubscribableServer);

nsSubscribableServer::nsSubscribableServer(void)
{
  NS_INIT_REFCNT();
  mDelimiter = '.';
}

nsSubscribableServer::~nsSubscribableServer(void)
{
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsSubscribableServer, NS_GET_IID(nsISubscribableServer));

NS_IMETHODIMP
nsSubscribableServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
	mIncomingServer = aServer;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetDelimiter(char aDelimiter)
{
	mDelimiter = aDelimiter;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetAsSubscribedInSubscribeDS(const char *aName)
{
    nsresult rv;

    NS_ASSERTION(aName,"no name");
    if (!aName) return NS_ERROR_FAILURE;

    nsXPIDLCString serverUri;

    rv = mIncomingServer->GetServerURI(getter_Copies(serverUri));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString uri;
    uri = (const char *)serverUri;
    uri += "/";
    uri += aName;

    nsCOMPtr<nsIRDFResource> resource;
    rv = mRDFService->GetResource((const char *) uri, getter_AddRefs(resource));

    nsCOMPtr<nsIRDFDataSource> ds;
    rv = mRDFService->GetDataSource("rdf:subscribe",getter_AddRefs(mSubscribeDatasource));
    if(NS_FAILED(rv)) return rv;
    if (!mSubscribeDatasource) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFNode> oldLiteral;
    rv = mSubscribeDatasource->GetTarget(resource, kNC_Subscribed, PR_TRUE, getter_AddRefs(oldLiteral));
    if(NS_FAILED(rv)) return rv;

    rv = mSubscribeDatasource->Change(resource, kNC_Subscribed, oldLiteral, kTrueLiteral);
    if(NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::UpdateSubscribedInSubscribeDS()
{
#ifdef DEBUG_SUBSCRIBE
	printf("UpdateSubscribedInSubscribeDS()");
#endif

	nsresult rv;
    nsCOMPtr<nsIEnumerator> subFolders;
    nsCOMPtr<nsIFolder> rootFolder;
    nsCOMPtr<nsIFolder> currFolder;
 
    rv = mIncomingServer->GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;

	rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));
    if (NS_FAILED(rv)) return rv;

    nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
    if (simpleEnumerator == nsnull) return NS_ERROR_OUT_OF_MEMORY;

    PRBool moreFolders;
        
    while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders) {
        nsCOMPtr<nsISupports> child;
        rv = simpleEnumerator->GetNext(getter_AddRefs(child));
        if (NS_SUCCEEDED(rv) && child) {
            currFolder = do_QueryInterface(child, &rv);
            if (NS_SUCCEEDED(rv) && currFolder) {
				nsXPIDLString name;
				rv = currFolder->GetName(getter_Copies(name));
				if (NS_SUCCEEDED(rv) && name) {
					nsCAutoString asciiName; asciiName.AssignWithConversion(name);
					rv = SetAsSubscribedInSubscribeDS((const char *)asciiName);
				}
            }
        }
    }

    delete simpleEnumerator;
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::AddToSubscribeDS(const char *aName)
{
	nsresult rv;

	NS_ASSERTION(aName,"attempting to add something with no name");
	if (!aName) return NS_ERROR_FAILURE;

#ifdef DEBUG_SUBSCRIBE
	printf("AddToSubscribeDS(%s)\n",aName);
#endif
	nsXPIDLCString serverUri;

	rv = mIncomingServer->GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

	nsCAutoString uri;
	uri = (const char *)serverUri;
	uri += "/";
	uri += aName;

	nsCOMPtr<nsIRDFResource> resource;
	rv = mRDFService->GetResource((const char *) uri, getter_AddRefs(resource));
	if(NS_FAILED(rv)) return rv;

	rv = SetPropertiesInSubscribeDS((const char *) uri, aName, resource);
	if (NS_FAILED(rv)) return rv;

	rv = FindAndAddParentToSubscribeDS((const char *) uri, (const char *)serverUri, aName, resource);
	if(NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetPropertiesInSubscribeDS(const char *uri, const char *aName, nsIRDFResource *aResource)
{
	nsresult rv;

#ifdef DEBUG_SUBSCRIBE
	printf("SetPropertiesInSubscribeDS(%s,%s,??)\n",uri,aName);
#endif
		
	nsCOMPtr<nsIRDFLiteral> nameLiteral;
	nsAutoString nameString; 
	nameString.AssignWithConversion(aName);
	rv = mRDFService->GetLiteral(nameString.GetUnicode(), getter_AddRefs(nameLiteral));
	if(NS_FAILED(rv)) return rv;

	rv = mSubscribeDatasource->Assert(aResource, kNC_Name, nameLiteral, PR_TRUE);
	if(NS_FAILED(rv)) return rv;

	rv = mSubscribeDatasource->Assert(aResource, kNC_Subscribed, kFalseLiteral, PR_TRUE);
	if(NS_FAILED(rv)) return rv;

	return rv;
}

NS_IMETHODIMP
nsSubscribableServer::FindAndAddParentToSubscribeDS(const char *uri, const char *serverUri, const char *aName, nsIRDFResource *aChildResource)
{
	nsresult rv;
#ifdef DEBUG_SUBSCRIBE
	printf("FindAndAddParentToSubscribeDS(%s,%s,%s,??)\n",uri,serverUri,aName);
#endif

	nsCOMPtr <nsIRDFResource> parentResource;

	nsCAutoString uriCStr(uri);

	PRInt32 startpos = nsCRT::strlen(serverUri) + 1;
	PRInt32 delimpos = uriCStr.RFindChar(mDelimiter,PR_TRUE);

	if (delimpos > startpos) {
		uriCStr.Truncate(delimpos);

		nsCAutoString nameCStr(aName);
		PRInt32 namedelimpos = nameCStr.RFindChar(mDelimiter,PR_TRUE);
		nameCStr.Truncate(namedelimpos);
	
		rv = mRDFService->GetResource((const char *) uriCStr, getter_AddRefs(parentResource));
		if(NS_FAILED(rv)) return rv;

		PRBool prune = PR_FALSE;
#if 0
		rv = mSubscribeDatasource->HasAssertion(parentResource, kNC_Subscribed, kFalseLiteral, PR_TRUE, &prune);
		if(NS_FAILED(rv)) return rv;
#endif

		if (!prune) {
			rv = SetPropertiesInSubscribeDS((const char *)uriCStr, (const char *)nameCStr, parentResource);
			if(NS_FAILED(rv)) return rv;
		}

		// assert the group as a child of the group above
		rv = mSubscribeDatasource->Assert(parentResource, kNC_Child, aChildResource, PR_TRUE);
		if(NS_FAILED(rv)) return rv;

		// recurse
		if (!prune) {
			rv = FindAndAddParentToSubscribeDS((const char *)uriCStr, serverUri, (const char *)nameCStr, parentResource);
			if(NS_FAILED(rv)) return rv;
		}
	}
	else {
		rv = mRDFService->GetResource(serverUri, getter_AddRefs(parentResource));
		if(NS_FAILED(rv)) return rv;

		// assert the group as a child of the server
		rv = mSubscribeDatasource->Assert(parentResource, kNC_Child, aChildResource, PR_TRUE);
		if(NS_FAILED(rv)) return rv;
	}

	return rv;
}


NS_IMETHODIMP
nsSubscribableServer::StopPopulatingSubscribeDS()
{
	mRDFService = nsnull;
	mSubscribeDatasource = nsnull;
	kNC_Name = nsnull;
	kNC_Child = nsnull;
	kNC_Subscribed = nsnull;
	
	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::StartPopulatingSubscribeDS()
{
  nsresult rv;
  mRDFService = do_GetService(kRDFServiceCID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv) && mRDFService,"no rdf server");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetDataSource("rdf:subscribe",getter_AddRefs(mSubscribeDatasource));
  NS_ASSERTION(NS_SUCCEEDED(rv) && mSubscribeDatasource,"no subscribe datasource");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#Name", getter_AddRefs(kNC_Name));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_Name,"no name resource");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(kNC_Child));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_Child,"no child resource");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#Subscribed", getter_AddRefs(kNC_Subscribed));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_Subscribed, "no subscribed resource");	
  if (NS_FAILED(rv)) return rv;
 
  nsAutoString trueString; 
  trueString.AssignWithConversion("true");
  rv = mRDFService->GetLiteral(trueString.GetUnicode(), getter_AddRefs(kTrueLiteral));
  if(NS_FAILED(rv)) return rv;

  nsAutoString falseString; 
  falseString.AssignWithConversion("false");
  rv = mRDFService->GetLiteral(falseString.GetUnicode(), getter_AddRefs(kFalseLiteral));
  if(NS_FAILED(rv)) return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
	if (!aListener) return NS_ERROR_NULL_POINTER;
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
nsSubscribableServer::PopulateSubscribeDatasource(nsIMsgWindow *aMsgWindow, PRBool aForceToServer)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::Subscribe(const char *aName)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::Unsubscribe(const char *aName)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}
