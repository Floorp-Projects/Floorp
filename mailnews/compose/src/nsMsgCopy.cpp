/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgCopy.h"

//#include "nsCopyMessageStreamListener.h"
#include "nsICopyMessageListener.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

/***
nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	nsMsgCompPrefs pCompPrefs;
	char* result = NULL;
	PRBool useBcc, mailBccSelf, newsBccSelf = PR_FALSE;

	if (newsBcc)
  {
    if (NS_SUCCEEDED(rv) && prefs)
    {
  		prefs->GetBoolPref("news.use_default_cc", &useBcc);
    }
****/

/*************

nsresult
CopyFileToFolder()
{
  NS_IMETHODIMP
nsMsgAppCore::CopyMessages(nsIDOMXULElement *srcFolderElement, nsIDOMXULElement *dstFolderElement,
						   nsIDOMNodeList *nodeList, PRBool isMove)
	nsresult rv;

	if(!srcFolderElement || !dstFolderElement || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsIRDFResource *srcResource, *dstResource;
	nsICopyMessageListener *dstFolder;
	nsIMsgFolder *srcFolder;
	nsISupportsArray *resourceArray;

	if(NS_FAILED(rv = dstFolderElement->GetResource(&dstResource)))
		return rv;

	if(NS_FAILED(rv = dstResource->QueryInterface(nsICopyMessageListener::GetIID(), (void**)&dstFolder)))
		return rv;

	if(NS_FAILED(rv = srcFolderElement->GetResource(&srcResource)))
		return rv;

	if(NS_FAILED(rv = srcResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)&srcFolder)))
		return rv;

	if(NS_FAILED(rv =ConvertDOMListToResourceArray(nodeList, &resourceArray)))
		return rv;

	//Call the mailbox service to copy first message.  In the future we should call CopyMessages.
	//And even more in the future we need to distinguish between the different types of URI's, i.e.
	//local, imap, and news, and call the appropriate copy function.

	PRUint32 cnt;
  rv = resourceArray->Count(&cnt);
  if (NS_SUCCEEDED(rv) && cnt > 0)
	{
		nsIRDFResource * firstMessage = (nsIRDFResource*)resourceArray->ElementAt(0);
		char *uri;
		firstMessage->GetValue(&uri);
		nsCopyMessageStreamListener* copyStreamListener = new nsCopyMessageStreamListener(srcFolder, dstFolder, nsnull);

		nsIMsgMessageService * messageService = nsnull;
		rv = GetMessageServiceFromURI(uri, &messageService);

		if (NS_SUCCEEDED(rv) && messageService)
		{
			nsIURL * url = nsnull;
			messageService->CopyMessage(uri, copyStreamListener, isMove, nsnull, &url);
			ReleaseMessageServiceFromURI(uri, messageService);
		}

	}

	NS_RELEASE(srcResource);
	NS_RELEASE(srcFolder);
	NS_RELEASE(dstResource);
	NS_RELEASE(dstFolder);
	NS_RELEASE(resourceArray);
	return rv;
}

FindFolderWithFlag
nsIMsgHeader

identity
server (nsIMsgIncommingServer)
folder (nsIMsgFolder)
GetFolderWithFlags() - nsMsgFolderFlags.h (public)

GetMessages() on folder

nsIMessages returned
get URI, key, header (nsIMsgHdr - )
get message key

nsMsg

get POP service - extract from Berkely mail folder

nsIPop3Service 


smtptest.cpp - setup as a url listener

********/
