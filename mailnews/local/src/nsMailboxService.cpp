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
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgKeyArray.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

nsMailboxService::nsMailboxService()
{
    NS_INIT_REFCNT();
}

nsMailboxService::~nsMailboxService()
{}

NS_IMPL_THREADSAFE_ISUPPORTS(nsMailboxService, nsIMailboxService::GetIID());

nsresult nsMailboxService::ParseMailbox(const nsFileSpec& aMailboxPath, nsIStreamListener *aMailboxParser, 
										nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
	nsMailboxUrl * mailboxUrl = nsnull;
	nsIMailboxUrl * url = nsnull;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	mailboxUrl = new nsMailboxUrl(nsnull, nsnull);
	if (mailboxUrl)
	{
		rv = mailboxUrl->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &url);
		if (NS_SUCCEEDED(rv) && url)
		{
			// okay now generate the url string
			nsFilePath filePath(aMailboxPath); // convert to file url representation...
			char * urlSpec = PR_smprintf("mailbox://%s", (const char *) filePath);
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
			url->SetMailboxParser(aMailboxParser);
			if (aUrlListener)
				url->RegisterListener(aUrlListener);

			nsMailboxProtocol * protocol = new nsMailboxProtocol(url);
			if (protocol)
				rv = protocol->LoadURL(url, nsnull /* no consumers for this type of url */);

			if (aURL)
				*aURL = url;
			else
				NS_IF_RELEASE(url); // otherwise release our ref count on it...
		}
	}

	NS_UNLOCK_INSTANCE();

	return rv;
}


nsresult nsMailboxService::CopyMessage(const char * aSrcMailboxURI, nsIStreamListener * aMailboxCopyHandler, PRBool moveMessage,
						   nsIUrlListener * aUrlListener, nsIURL **aURL)
{
	nsMailboxUrl * mailboxUrl = nsnull;
	nsIMailboxUrl * url = nsnull;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	mailboxUrl = new nsMailboxUrl(nsnull, nsnull);
	if (mailboxUrl)
	{
		rv = mailboxUrl->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &url);
		if (NS_SUCCEEDED(rv) && url)
		{
			// okay now generate the url string
			char * urlSpec = nsnull;
			nsString folderURI;
			nsFileSpec folderPath ("");
			nsMsgKey msgKey;
			
			nsParseLocalMessageURI(aSrcMailboxURI, folderURI, &msgKey);
			char *rootURI = folderURI.ToNewCString();
			nsURI2Path(kMessageRootURI, rootURI, folderPath);

			nsFilePath filePath(folderPath); // convert to file url representation...
			urlSpec = PR_smprintf("mailboxMessage://%s?number=%d", (const char *) filePath, msgKey);
			
			// set the url type to be a copy...
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
			
			if (moveMessage)
				url->SetMailboxAction(nsMailboxActionMoveMessage);
			else
				url->SetMailboxAction(nsMailboxActionCopyMessage);

			url->SetMailboxCopyHandler(aMailboxCopyHandler);
			
			if (aUrlListener)
				url->RegisterListener(aUrlListener);

			// create a protocol instance to run the url..
			nsMailboxProtocol * protocol = new nsMailboxProtocol(url);
			if (protocol)
				rv = protocol->LoadURL(url, nsnull);

			if (aURL)
				*aURL = url;
			else
				NS_IF_RELEASE(url); // otherwise release our ref count on it...
		}
	}

	NS_UNLOCK_INSTANCE();

	return rv;
}

nsresult nsMailboxService::DisplayMessage(const char* aMessageURI, nsISupports * aDisplayConsumer, 
										  nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
	nsMailboxUrl * mailboxUrl = nsnull;
	nsIMailboxUrl * url = nsnull;
	nsresult rv = NS_OK;
	NS_LOCK_INSTANCE();

	mailboxUrl = new nsMailboxUrl(nsnull, nsnull);
	if (mailboxUrl)
	{
		rv = mailboxUrl->QueryInterface(nsIMailboxUrl::GetIID(), (void **) &url);
		if (NS_SUCCEEDED(rv) && url)
		{
			// okay now generate the url string
			char * urlSpec = nsnull;
			
			// decompose the uri into a full path and message id...
			nsString folderURI;
			nsFileSpec folderSpec;
			nsMsgKey msgIndex;

			nsParseLocalMessageURI(aMessageURI, folderURI, &msgIndex);
			nsURI2Path(kMessageRootURI, nsAutoCString(folderURI), folderSpec);

			nsFilePath filePath(folderSpec); // convert to file url representation...
			urlSpec = PR_smprintf("mailboxMessage://%s?number=%d", (const char *) filePath, msgIndex);
			
			url->SetSpec(urlSpec);
			PR_FREEIF(urlSpec);
			if (aUrlListener)
				url->RegisterListener(aUrlListener);

			// create a protocol instance to run the url..
			nsMailboxProtocol * protocol = new nsMailboxProtocol(url);
			if (protocol)
				rv = protocol->LoadURL(url, aDisplayConsumer);

			if (aURL)
				*aURL = url;
			else
				NS_IF_RELEASE(url); // otherwise release our ref count on it...
		}
	}

	NS_UNLOCK_INSTANCE();

	return rv;
}

nsresult nsMailboxService::DisplayMessageNumber(const nsFileSpec& aMailboxPath, PRUint32 aMessageNumber, nsISupports * aDisplayConsumer,
                                                nsIUrlListener * aUrlListener, nsIURL ** aURL)
{
	nsMsgKeyArray msgKeys;
	nsresult rv = NS_OK;
	// extract the message key for this message number and turn around and call the other displayMessage method on it...
	nsIMsgDatabase * mailDB = nsnull;
	nsIMsgDatabase *mailDBFactory;

	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		rv = mailDBFactory->Open((nsFileSpec&) aMailboxPath, PR_FALSE, (nsIMsgDatabase **) &mailDB, PR_FALSE);
		mailDBFactory->Release();
	}
//	rv = nsMailDatabase::Open((nsFileSpec&) aMailboxPath, PR_FALSE, &mailDb);

	if (NS_SUCCEEDED(rv) && mailDB)
	{
		// extract the message key array
		mailDB->ListAllKeys(msgKeys);
		if (aMessageNumber < msgKeys.GetSize()) 
		{
			nsMsgKey msgKey = msgKeys[aMessageNumber];
			// okay, we have the msgKey so let's get rid of our db state...
			mailDB->Close(PR_TRUE);
			mailDB = nsnull;
			char * uri = nsnull;

			nsBuildLocalMessageURI(aMailboxPath, msgKey, &uri);

			rv = DisplayMessage(uri, aDisplayConsumer, aUrlListener, aURL);
		}
		else
			rv = NS_ERROR_FAILURE;
	}

	if (mailDB) // in case we slipped through the cracks without releasing the db...
		mailDB->Close(PR_TRUE);

	return rv;
}
