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

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsCopyMessageStreamListener)
NS_IMPL_RELEASE(nsCopyMessageStreamListener)
NS_IMPL_QUERY_INTERFACE(nsCopyMessageStreamListener, nsIStreamListener::GetIID()); /* we need to pass in the interface ID of this interface */

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
	return mDestination->BeginCopy();
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
	return mDestination->EndCopy();
}
