/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCopyMessageStreamListener.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsIMailboxUrl.h"
#include "nsIRDFService.h"
#include "nsIRDFNode.h"
#include "nsRDFCID.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_BEGIN_EXTERN_C

nsresult
NS_NewCopyMessageStreamListener(const nsIID& iid, void **result)
{
	nsCopyMessageStreamListener *listener = new nsCopyMessageStreamListener();
	if(!listener)
		return NS_ERROR_OUT_OF_MEMORY;
	return listener->QueryInterface(iid, result);
}

NS_END_EXTERN_C

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsCopyMessageStreamListener)
NS_IMPL_RELEASE(nsCopyMessageStreamListener)

NS_IMETHODIMP nsCopyMessageStreamListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID()) || aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
	}              
	else if(aIID.Equals(nsCOMTypeInfo<nsICopyMessageStreamListener>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsICopyMessageStreamListener*, this);
	}

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return NS_ERROR_NO_INTERFACE;
}

static nsresult GetMessage(nsIURI *aURL, nsIMessage **message)
{
	nsCOMPtr<nsIMsgUriUrl> uriURL;
	char* uri;
	nsresult rv;

	if(!message)
		return NS_ERROR_NULL_POINTER;

	//Need to get message we are about to copy
	uriURL = do_QueryInterface(aURL, &rv);
	if(NS_FAILED(rv))
		return rv;

	rv = uriURL->GetURI(&uri);
	if(NS_FAILED(rv))
		return rv;

	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFResource> messageResource;
		if(NS_SUCCEEDED(rdfService->GetResource(uri, getter_AddRefs(messageResource))))
		{
			messageResource->QueryInterface(nsCOMTypeInfo<nsIMessage>::GetIID(), (void**)message);
		}
	}
	delete[] uri;

	return rv;
}

static nsresult IsMoveMessage(nsIURI *aURL, PRBool *isMoveMessage)
{
	if(!isMoveMessage)
		return NS_ERROR_NULL_POINTER;

	*isMoveMessage = PR_FALSE;

	nsresult rv;
	nsCOMPtr<nsIMailboxUrl> mailboxURL(do_QueryInterface(aURL, &rv));
	if(NS_SUCCEEDED(rv))
	{
		nsMailboxAction mailboxAction;
		rv = mailboxURL->GetMailboxAction(&mailboxAction);

		if(NS_SUCCEEDED(rv))
			*isMoveMessage = (mailboxAction == nsIMailboxUrl::ActionMoveMessage);
	}


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
		rv = srcFolder->DeleteMessages(messageArray, nsnull, PR_TRUE);
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
	if(copySucceeded)
	{
		PRBool moveMessage;
		IsMoveMessage(uri, &moveMessage);
		if(moveMessage)
		{
			rv = DeleteMessage(uri, mSrcFolder);
		}
	}
	//Even if the above actions failed we probably still want to return NS_OK.  There should probably
	//be some error dialog if either the copy or delete failed.
	return NS_OK;
}
