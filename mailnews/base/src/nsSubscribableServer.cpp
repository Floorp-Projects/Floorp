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

#include "rdf.h"
#include "nsICharsetConverterManager.h"
#include "nslog.h"

NS_IMPL_LOG(nsSubscribableServerLog)
#define PRINTF NS_LOG_PRINTF(nsSubscribableServerLog)
#define FLUSH  NS_LOG_FLUSH(nsSubscribableServerLog)

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_SUBSCRIBE 1
#endif

#define TRUE_LITERAL_STR "true"
#define FALSE_LITERAL_STR "false"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

MOZ_DECL_CTOR_COUNTER(nsSubscribableServer);

nsSubscribableServer::nsSubscribableServer(void)
{
  NS_INIT_REFCNT();
  mDelimiter = '.';
  mShowFullName = PR_TRUE;
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
nsSubscribableServer::SetAsSubscribedInSubscribeDS(const char *aURI)
{
    nsresult rv;

    NS_ASSERTION(aURI,"no URI");
    if (!aURI) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFResource> resource;
    rv = mRDFService->GetResource(aURI, getter_AddRefs(resource));

    nsCOMPtr<nsIRDFDataSource> ds;
    rv = mRDFService->GetDataSource("rdf:subscribe",getter_AddRefs(mSubscribeDatasource));
    if(NS_FAILED(rv)) return rv;
    if (!mSubscribeDatasource) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFNode> oldNode;
    rv = mSubscribeDatasource->GetTarget(resource, kNC_Subscribed, PR_TRUE, getter_AddRefs(oldNode));
    if(NS_FAILED(rv)) return rv;

	//check if literal is already true
	nsCOMPtr<nsIRDFLiteral> oldLiteral = do_QueryInterface(oldNode);
	if (!oldLiteral) return NS_ERROR_FAILURE;
	const PRUnichar *subscribedLiteralStr;
	rv = oldLiteral->GetValueConst(&subscribedLiteralStr);
    if(NS_FAILED(rv)) return rv;

	// if it was already "true", do nothing
	if (nsCRT::strcmp(subscribedLiteralStr,TRUE_LITERAL_STR)) {
		rv = mSubscribeDatasource->Change(resource, kNC_Subscribed, oldNode, kTrueLiteral);
		if(NS_FAILED(rv)) return rv;
	}

    return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::UpdateSubscribedInSubscribeDS()
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}


// copied code, this needs to be put in msgbaseutil.
nsresult CreateUnicodeStringFromUtf7(const char *aSourceString, PRUnichar **aUnicodeStr)
{
  if (!aUnicodeStr)
	  return NS_ERROR_NULL_POINTER;

  PRUnichar *convertedString = NULL;
  nsresult res;
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    // convert utf7 to unicode
    nsIUnicodeDecoder* decoder = nsnull;

    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
    {
      PRInt32 srcLen = PL_strlen(aSourceString);
      res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
      // temporary buffer to hold unicode string
      unichars = new PRUnichar[unicharLength + 1];
      if (unichars == nsnull) 
      {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else 
      {
        res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
        unichars[unicharLength] = 0;
      }
      NS_IF_RELEASE(decoder);
      nsString unicodeStr(unichars);
      convertedString = unicodeStr.ToNewUnicode();
	  delete [] unichars;
    }
  }
  *aUnicodeStr = convertedString;
  return (convertedString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
NS_IMETHODIMP
nsSubscribableServer::AddToSubscribeDS(const char *aName, PRBool addAsSubscribed, PRBool changeIfExists)
{
	nsresult rv;

	NS_ASSERTION(aName,"attempting to add something with no name");
	if (!aName) return NS_ERROR_FAILURE;

#ifdef DEBUG_SUBSCRIBE
	PRINTF("AddToSubscribeDS(%s,%d,%d)\n",aName,addAsSubscribed,changeIfExists);
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
	
	nsXPIDLString unicodeName;
	rv = CreateUnicodeStringFromUtf7(aName, getter_Copies(unicodeName));
	if (NS_FAILED(rv)) return rv;

	rv = SetPropertiesInSubscribeDS((const char *) uri, (const PRUnichar *)unicodeName, resource, addAsSubscribed, changeIfExists);
	if (NS_FAILED(rv)) return rv;

	rv = FindAndAddParentToSubscribeDS((const char *) uri, (const char *)serverUri, aName, resource);
	if(NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsSubscribableServer::SetPropertiesInSubscribeDS(const char *uri, const PRUnichar *aName, nsIRDFResource *aResource, PRBool subscribed, PRBool changeIfExists)
{
	nsresult rv;

#ifdef DEBUG_SUBSCRIBE
	PRINTF("SetPropertiesInSubscribeDS(%s,??,??,%d,%d)\n",uri,subscribed,changeIfExists);
#endif
		
	nsCOMPtr<nsIRDFLiteral> nameLiteral;
	rv = mRDFService->GetLiteral(aName, getter_AddRefs(nameLiteral));
	if(NS_FAILED(rv)) return rv;

	PRBool nameExists = PR_TRUE;
	nsCOMPtr <nsIRDFNode> nameNode;
	// should be HasAssertion(), since we don't need to get at the literal
	rv = mSubscribeDatasource->GetTarget(aResource, kNC_Name, PR_TRUE, getter_AddRefs(nameNode));
	if(NS_FAILED(rv)) return rv;
	if ((!nameNode) && (rv == NS_RDF_NO_VALUE)) {
		nameExists = PR_FALSE;
	}

	if (!nameExists) {
		rv = mSubscribeDatasource->Assert(aResource, kNC_Name, nameLiteral, PR_TRUE);
		if(NS_FAILED(rv)) return rv;

		if (mShowFullName) {
			rv = mSubscribeDatasource->Assert(aResource, kNC_LeafName, nameLiteral, PR_TRUE);
			if(NS_FAILED(rv)) return rv;
		}
		else {
			nsAutoString leafStr(aName);
			PRInt32 leafPos = leafStr.RFindChar(mDelimiter,PR_TRUE);
			if (leafPos != -1) {
				leafStr.Cut(0,leafPos + 1);
			}

			nsCOMPtr<nsIRDFLiteral> leafLiteral;
			rv = mRDFService->GetLiteral(leafStr.GetUnicode(), getter_AddRefs(leafLiteral));
			if(NS_FAILED(rv)) return rv;

			rv = mSubscribeDatasource->Assert(aResource, kNC_LeafName, leafLiteral, PR_TRUE);
			if(NS_FAILED(rv)) return rv;
		}
	}

	PRBool subscribedExists = PR_TRUE;
	nsCOMPtr <nsIRDFNode> subscribedNode;
	rv = mSubscribeDatasource->GetTarget(aResource, kNC_Subscribed, PR_TRUE, getter_AddRefs(subscribedNode));
	if(NS_FAILED(rv)) return rv;
	if ((!subscribedNode) && (rv == NS_RDF_NO_VALUE)) {
		subscribedExists = PR_FALSE;
	}

	if (subscribed) {
		if (!subscribedExists) {
			rv = mSubscribeDatasource->Assert(aResource, kNC_Subscribed, kTrueLiteral, PR_TRUE);
			if(NS_FAILED(rv)) return rv;
		}
		else if (changeIfExists) {
			nsCOMPtr<nsIRDFLiteral> subscribedLiteral = do_QueryInterface(subscribedNode);
			if (!subscribedLiteral) return NS_ERROR_FAILURE;
			const PRUnichar *subscribedLiteralStr;
			rv = subscribedLiteral->GetValueConst(&subscribedLiteralStr);
			if(NS_FAILED(rv)) return rv;

			if (nsCRT::strcmp(subscribedLiteralStr,TRUE_LITERAL_STR)) {
				rv = mSubscribeDatasource->Change(aResource, kNC_Subscribed, subscribedNode, kTrueLiteral);
				if(NS_FAILED(rv)) return rv;
			}
		}
	}
	else {
		if (!subscribedExists) {
			rv = mSubscribeDatasource->Assert(aResource, kNC_Subscribed, kFalseLiteral, PR_TRUE);
			if(NS_FAILED(rv)) return rv;
		}
		else if (changeIfExists) {
			nsCOMPtr<nsIRDFLiteral> subscribedLiteral = do_QueryInterface(subscribedNode);
			if (!subscribedLiteral) return NS_ERROR_FAILURE;
			const PRUnichar *subscribedLiteralStr;
			rv = subscribedLiteral->GetValueConst(&subscribedLiteralStr);
			if(NS_FAILED(rv)) return rv;

			if (nsCRT::strcmp(subscribedLiteralStr,FALSE_LITERAL_STR)) {
				rv = mSubscribeDatasource->Change(aResource, kNC_Subscribed, subscribedNode, kFalseLiteral);
				if(NS_FAILED(rv)) return rv;
			}
		}
	}


	return rv;
}

NS_IMETHODIMP
nsSubscribableServer::FindAndAddParentToSubscribeDS(const char *uri, const char *serverUri, const char *aName, nsIRDFResource *aChildResource)
{
	nsresult rv;
#ifdef DEBUG_SUBSCRIBE
	PRINTF("FindAndAddParentToSubscribeDS(%s,%s,%s,??)\n",uri,serverUri,aName);
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

		PRBool parentExists = PR_TRUE;
		nsCOMPtr <nsIRDFNode> firstChild;
		// see if the parent already exists.
		// for the in memory datasource. HasAssertion() will be O(n), 
		// see bug #35817
		// to make the check for a parent constant time, I have a trick:
		// call GetTarget() to get the first child of the parent
		// (note, this current child has not been asserted.
		//
		// that will be constant time since the parent,
		// if it exists, will have #Name, #Subscribe, and 0 to <n> #child
		// 
		// if that fails, then do HasAssertion(), which will be constant
		// since <n> will be zero.
		rv = mSubscribeDatasource->GetTarget(parentResource, kNC_Child, PR_TRUE, getter_AddRefs(firstChild));
		if(NS_FAILED(rv)) return rv;
		if ((!firstChild) && (rv == NS_RDF_NO_VALUE)) {
			rv = mSubscribeDatasource->HasAssertion(parentResource, kNC_Subscribed, kFalseLiteral, PR_TRUE, &parentExists);
			if(NS_FAILED(rv)) return rv;
		}

		if (!parentExists) {
			nsXPIDLString unicodeName;
			rv = CreateUnicodeStringFromUtf7((const char *)nameCStr, getter_Copies(unicodeName));
			if (NS_FAILED(rv)) return rv;

			rv = SetPropertiesInSubscribeDS((const char *)uriCStr, (const PRUnichar *)unicodeName, parentResource, PR_FALSE, PR_FALSE /* change if exists */);
			if(NS_FAILED(rv)) return rv;
		}

		// assert the group as a child of the group above
		rv = mSubscribeDatasource->Assert(parentResource, kNC_Child, aChildResource, PR_TRUE);
		if(NS_FAILED(rv)) return rv;

		// recurse upwards
		if (!parentExists) {
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
	kNC_LeafName = nsnull;
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

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#LeafName", getter_AddRefs(kNC_LeafName));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_LeafName,"no leaf name resource");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(kNC_Child));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_Child,"no child resource");
  if (NS_FAILED(rv)) return rv;

  rv = mRDFService->GetResource("http://home.netscape.com/NC-rdf#Subscribed", getter_AddRefs(kNC_Subscribed));
  NS_ASSERTION(NS_SUCCEEDED(rv) && kNC_Subscribed, "no subscribed resource");	
  if (NS_FAILED(rv)) return rv;
 
  nsAutoString trueString; 
  trueString.AssignWithConversion(TRUE_LITERAL_STR);
  rv = mRDFService->GetLiteral(trueString.GetUnicode(), getter_AddRefs(kTrueLiteral));
  if(NS_FAILED(rv)) return rv;

  nsAutoString falseString; 
  falseString.AssignWithConversion(FALSE_LITERAL_STR);
  rv = mRDFService->GetLiteral(falseString.GetUnicode(), getter_AddRefs(kFalseLiteral));
  if(NS_FAILED(rv)) return rv;

  return NS_OK;
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
nsSubscribableServer::PopulateSubscribeDatasourceWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer, const char *uri)
{
	NS_ASSERTION(PR_FALSE,"override this.");
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSubscribableServer::PopulateSubscribeDatasource(nsIMsgWindow *aMsgWindow, PRBool aForceToServer)
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
