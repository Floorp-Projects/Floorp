/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
*/

#include "msgCore.h"
#include "nntpCore.h"
#include "nsNewsUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsCOMPtr.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgUtils.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"
#include "nsIMsgNewsFolder.h"
#include "nsIFileSpec.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static nsresult
nsGetNewsServer(const char* username, const char *hostname,
                nsIMsgIncomingServer** aResult)
{
#ifdef DEBUG_NEWS
  printf("nsGetNewsServer(%s,%s,??)\n",username,hostname);
#endif
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  // find the news host
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = accountManager->FindServer(username,
                                  hostname,
                                  "nntp",
                                  getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv; 

  *aResult = server;
  NS_ADDREF(*aResult);
  
  return rv;
}

/* parses NewsMessageURI */
nsresult 
nsParseNewsMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key)
{
    NS_ENSURE_ARG_POINTER(uri);
    NS_ENSURE_ARG_POINTER(key);

	nsCAutoString uriStr(uri);
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
    PRInt32 keyEndSeparator = uriStr.FindCharInSet("?&", 
                                                   keySeparator); 

		uriStr.Left(folderURI, keySeparator);
        folderURI.Cut(4, 8);    // cut out the _message part of news_message:

		nsCAutoString keyStr;
    if (keyEndSeparator != -1)
        uriStr.Mid(keyStr, keySeparator+1, 
                   keyEndSeparator-(keySeparator+1));
    else
        uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;
}

nsresult nsGetNewsGroupFromUri(const char *uri, nsIMsgNewsFolder **aFolder)
{
  NS_ENSURE_ARG(aFolder);
  NS_ENSURE_ARG(uri);
  nsresult rv;

  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) return(rv);

  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(uri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) return(rv);

  rv = resource->QueryInterface(NS_GET_IID(nsIMsgNewsFolder), (void **) aFolder);
  NS_ASSERTION(NS_SUCCEEDED(rv), "uri is not for a news folder!"); 
  if (NS_FAILED(rv)) return rv;

  if (!*aFolder) return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult nsCreateNewsBaseMessageURI(const char *baseURI, char **baseMessageURI)
{
	if(!baseMessageURI)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString tailURI(baseURI);

	// chop off mailbox:/
	if (tailURI.Find(kNewsRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kNewsRootURI));
	
	nsCAutoString baseURIStr(kNewsMessageRootURI);
	baseURIStr += tailURI;

	*baseMessageURI = baseURIStr.ToNewCString();
	if(!*baseMessageURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}
