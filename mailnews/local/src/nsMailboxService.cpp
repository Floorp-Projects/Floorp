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

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsINetService.h"
#include "nsMailboxService.h"

#include "nsMailboxUrl.h"
#include "nsMailboxProtocol.h"

nsMailboxService::nsMailboxService()
{
    NS_INIT_REFCNT();
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsMailboxService, nsIMailboxService::IID());

nsresult nsMailboxService::ParseMailbox(const nsFilePath& aMailboxPath, nsIStreamListener *aMailboxParser, nsIURL ** aURL)
{
	nsMailboxUrl * mailboxUrl = nsnull;
	nsIMailboxUrl * url = nsnull;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	mailboxUrl = new nsMailboxUrl(nsnull, nsnull);
	if (mailboxUrl)
	{
		rv = mailboxUrl->QueryInterface(nsIMailboxUrl::IID(), (void **) &url);
		if (NS_SUCCEEDED(rv) && url)
		{
			// okay now generate the url string
			char * urlSpec = PR_smprintf("mailbox://%s", (const char *) aMailboxPath);
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
			url->SetMailboxParser(aMailboxParser);

			nsMailboxProtocol * protocol = new nsMailboxProtocol(url);
			if (protocol)
				protocol->LoadURL(url);

			if (aURL)
				*aURL = url;
			else
				NS_IF_RELEASE(url); // otherwise release our ref count on it...
		}
	}

	NS_UNLOCK_INSTANCE();

	return rv;
}

nsresult nsMailboxService::DisplayMessage(const nsFilePath& aMailboxPath, PRUint32 aStartByte, PRUint32 aEndByte, 
										  nsISupports * aDisplayConsumer, nsIURL ** aURL)
{
	return NS_OK;
}
