/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCopyMessageStreamListener.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMailboxUrl.h"
#include "nsIMsgHdr.h"
#include "nsIMessage.h"
#include "nsIRDFService.h"
#include "nsIRDFNode.h"
#include "nsRDFCID.h"
#include "nsIMsgImapMailFolder.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_THREADSAFE_ADDREF(nsCopyMessageStreamListener)
NS_IMPL_THREADSAFE_RELEASE(nsCopyMessageStreamListener)

NS_INTERFACE_MAP_BEGIN(nsCopyMessageStreamListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsICopyMessageStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

static nsresult GetMessage(nsIURI *aURL, nsIMessage **message)
{
	nsCOMPtr<nsIMsgMessageUrl> uriURL;
	char* uri;
	nsresult rv;

	if(!message)
		return NS_ERROR_NULL_POINTER;

	//Need to get message we are about to copy
	uriURL = do_QueryInterface(aURL, &rv);
	if(NS_FAILED(rv))
		return rv;

	rv = uriURL->GetUri(&uri);
	if(NS_FAILED(rv))
		return rv;

	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFResource> messageResource;
		if(NS_SUCCEEDED(rdfService->GetResource(uri, getter_AddRefs(messageResource))))
		{
			messageResource->QueryInterface(NS_GET_IID(nsIMessage), (void**)message);
		}
	}
	nsMemory::Free(uri);

	return rv;
}


static nsresult DeleteMessage(nsIURI *aURL, nsIMsgFolder *srcFolder)
{
	nsCOMPtr<nsIMessage> message;
	nsresult rv;

	rv = GetMessage(aURL, getter_AddRefs(message));
	if(NS_SUCCEEDED(rv) && srcFolder)
	{
		nsCOMPtr<nsISupportsArray> messageArray;
		NS_NewISupportsArray(getter_AddRefs(messageArray));
		nsCOMPtr<nsISupports> messageSupports(do_QueryInterface(message));
		if(messageSupports)
			messageArray->AppendElement(messageSupports);
		rv = srcFolder->DeleteMessages(messageArray, nsnull, PR_TRUE, PR_TRUE);
	}
	return rv;
}

nsCopyMessageStreamListener::nsCopyMessageStreamListener()
{
  /* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();
}

nsCopyMessageStreamListener::~nsCopyMessageStreamListener()
{
	//All member variables are nsCOMPtr's.
}

NS_IMETHODIMP nsCopyMessageStreamListener::Init(nsIMsgFolder *srcFolder, nsICopyMessageListener *destination, nsISupports *listenerData)
{
	mSrcFolder = dont_QueryInterface(srcFolder);
	mDestination = dont_QueryInterface(destination);
	mListenerData = dont_QueryInterface(listenerData);
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::StartMessage()
{
	if (mDestination)
		mDestination->StartMessage();

	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::EndMessage(nsMsgKey key)
{
	if (mDestination)
		mDestination->EndMessage(key);

	return NS_OK;
}


NS_IMETHODIMP nsCopyMessageStreamListener::OnDataAvailable(nsIChannel * /* aChannel */, nsISupports *ctxt, nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
{
	nsresult rv;
	rv = mDestination->CopyData(aIStream, aLength);
	return rv;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt)
{
	nsCOMPtr<nsIMessage> message;
	nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> uri = do_QueryInterface(ctxt, &rv);

  NS_ASSERTION(NS_SUCCEEDED(rv), "ahah...someone didn't pass in the expected context!!!");
	
	if (NS_SUCCEEDED(rv))
		rv = GetMessage(uri, getter_AddRefs(message));
	if(NS_SUCCEEDED(rv))
		rv = mDestination->BeginCopy(message);

	return rv;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult aStatus, const PRUnichar *aMsg)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> uri = do_QueryInterface(ctxt, &rv);

	if (NS_FAILED(rv)) return rv;
	PRBool copySucceeded = (aStatus == NS_BINDING_SUCCEEDED);
	rv = mDestination->EndCopy(copySucceeded);
	//If this is a move and we finished the copy, delete the old message.
	if(NS_SUCCEEDED(rv) && copySucceeded)
	{
		PRBool moveMessage = PR_FALSE;

		nsCOMPtr<nsIMsgMailNewsUrl> mailURL(do_QueryInterface(uri));
		if(mailURL)
			rv = mailURL->IsUrlType(nsIMsgMailNewsUrl::eMove, &moveMessage);

		if(NS_FAILED(rv))
			moveMessage = PR_FALSE;

		// OK, this is wrong if we're moving to an imap folder, for example. This really says that
		// we were able to pull the message from the source, NOT that we were able to
		// put it in the destination!
		if(moveMessage)
		{
			// don't do this if we're moving to an imap folder - that's handled elsewhere.
			nsCOMPtr <nsIMsgImapMailFolder> destImap = do_QueryInterface(mDestination);
			if (!destImap)
			{
        // if the destination is a local folder, it will handle the delete from the source in EndMove
//				rv = DeleteMessage(uri, mSrcFolder);
//				if(NS_SUCCEEDED(rv))
					rv = mDestination->EndMove();
			}
		}
	}
	//Even if the above actions failed we probably still want to return NS_OK.  There should probably
	//be some error dialog if either the copy or delete failed.
	return NS_OK;
}

