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

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsCopyMessageStreamListener)
NS_IMPL_RELEASE(nsCopyMessageStreamListener)
NS_IMPL_QUERY_INTERFACE(nsCopyMessageStreamListener, nsIStreamListener::GetIID()); /* we need to pass in the interface ID of this interface */

static nsresult GetMessage(nsIURL *aURL, nsIMessage **message)
{
	nsIMsgUriUrl *uriURL;
	char* uri;
	nsresult rv;

	if(!message)
		return NS_ERROR_NULL_POINTER;

	//Need to get message we are about to copy
	rv = aURL->QueryInterface(nsIMsgUriUrl::GetIID(), (void**)&uriURL);
	if(NS_FAILED(rv))
		return rv;

	rv = uriURL->GetURI(&uri);
	NS_RELEASE(uriURL);
	if(NS_FAILED(rv))
		return rv;

	nsIRDFService *rdfService;

	if(NS_SUCCEEDED(rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsIRDFService::GetIID(),
                                      (nsISupports**) &rdfService)))
	{
		nsIRDFResource *messageResource;
		if(NS_SUCCEEDED(rdfService->GetResource(uri, &messageResource)))
		{
			messageResource->QueryInterface(nsIMessage::GetIID(), (void**)message);
			NS_RELEASE(messageResource);
		}
		nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
	}
	delete[] uri;

	return rv;
}

static nsresult IsMoveMessage(nsIURL *aURL, PRBool *isMoveMessage)
{
	if(!isMoveMessage)
		return NS_ERROR_NULL_POINTER;

	*isMoveMessage = PR_FALSE;

	nsIMailboxUrl *mailboxURL;
	if(NS_SUCCEEDED(aURL->QueryInterface(nsIMailboxUrl::GetIID(), (void**)&mailboxURL)))
	{
		nsMailboxAction mailboxAction;
		mailboxURL->GetMailboxAction(&mailboxAction);

		NS_IF_RELEASE(mailboxURL);
		*isMoveMessage = (mailboxAction == nsMailboxActionMoveMessage);
	}


	return NS_OK;

}

static nsresult DeleteMessage(nsIURL *aURL, nsIMsgFolder *srcFolder)
{
	nsIMessage *message = nsnull;
	nsresult rv;

	rv = GetMessage(aURL, &message);
	if(NS_SUCCEEDED(rv) && srcFolder)
		rv = srcFolder->DeleteMessage(message);
	NS_IF_RELEASE(message);
	return rv;
}

nsCopyMessageStreamListener::nsCopyMessageStreamListener(nsIMsgFolder *srcFolder,
														 nsICopyMessageListener *destination,
														 nsISupports *listenerData)
{
  /* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();

	if(srcFolder)
	{
		mSrcFolder = srcFolder;
		mSrcFolder->AddRef();
	}

	if(destination)
	{
		mDestination = destination;
		mDestination->AddRef();
	}

	if(listenerData)
	{
		mListenerData = listenerData;
		mListenerData->AddRef();
	}
}

nsCopyMessageStreamListener::~nsCopyMessageStreamListener()
{
	if(mSrcFolder)
	{
		mSrcFolder->Release();
	}

	if(mDestination)
	{
		mDestination->Release();
	}

	if(mListenerData)
	{
		mListenerData->Release();
	}
}

NS_IMETHODIMP nsCopyMessageStreamListener::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo)
{
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, 
                               PRUint32 aLength)
{
	nsresult rv;
	rv = mDestination->CopyData(aIStream, aLength);
	return rv;
}
NS_IMETHODIMP nsCopyMessageStreamListener::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	nsIMessage *message = nsnull;
	nsresult rv;


	rv = GetMessage(aURL, &message);
	rv = mDestination->BeginCopy(message);
	if(message)
		NS_RELEASE(message);

	return rv;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
	return NS_OK;
}

NS_IMETHODIMP nsCopyMessageStreamListener::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	//If this is a move and we finished the copy, delete the old message.
	if(aStatus == NS_BINDING_SUCCEEDED)
	{
		PRBool moveMessage;
		IsMoveMessage(aURL, &moveMessage);
		if(moveMessage)
		{
			DeleteMessage(aURL, mSrcFolder);
		}
	}

	return mDestination->EndCopy(aStatus == NS_BINDING_SUCCEEDED);
}
