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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"    // precompiled header...
#include "nntpCore.h"

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif


#include "nsNntpService.h"
#include "nsINetService.h"
#include "nsINntpUrl.h"
#include "nsNNTPProtocol.h"

static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);

nsNntpService::nsNntpService()
{
    NS_INIT_REFCNT();
}

nsNntpService::~nsNntpService()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsNntpService, nsINntpService::GetIID());

NS_IMETHODIMP nsNntpService::RunNewsUrl(const nsString& urlString, nsISupports * aConsumer, 
										nsIUrlListener *aUrlListener, nsIURL ** aUrl)
{
	// for now, assume the url is a news url and load it....
	nsINntpUrl		*nntpUrl = nsnull;
	nsINetService	*pNetService = nsnull;
	nsITransport	*transport = nsnull;
	nsNNTPProtocol	*nntpProtocol = nsnull;
	nsresult rv = NS_OK;

	// make sure we have a netlib service around...
	rv = NS_NewINetService(&pNetService, nsnull); 

	if (NS_SUCCEEDED(rv) && pNetService)
	{

		rv = nsComponentManager::CreateInstance(kNntpUrlCID, nsnull, nsINntpUrl::GetIID(), (void **)
												&nntpUrl);

		if (NS_SUCCEEDED(rv) && nntpUrl)
		{
			char * urlSpec = urlString.ToNewCString();
			nntpUrl->SetSpec(urlSpec);
			if (urlSpec)
				delete [] urlSpec;
			
			const char * host;
			PRUint32 port = NEWS_PORT;
			
			if (aUrlListener) // register listener if there is one...
				nntpUrl->RegisterListener(aUrlListener);

			nntpUrl->GetHostPort(&port);
			nntpUrl->GetHost(&host);
			// okay now create a transport to run the url in...
			pNetService->CreateSocketTransport(&transport, port, host);
			if (NS_SUCCEEDED(rv) && transport)
			{
				// almost there...now create a nntp protocol instance to run the url in...
				nntpProtocol = new nsNNTPProtocol(nntpUrl, transport);
				if (nntpProtocol)
					nntpProtocol->LoadURL(nntpUrl, aConsumer);
			}

			if (aUrl)
				*aUrl = nntpUrl; // transfer ref count
			else
				NS_RELEASE(nntpUrl);
		} // if nntpUrl

		NS_RELEASE(pNetService);
	} // if pNetService
	
	return rv;
}



