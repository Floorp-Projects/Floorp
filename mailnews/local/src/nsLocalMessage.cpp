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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "msgCore.h"    // precompiled header...
#include "nsIMsgHdr.h"
#include "nsLocalMessage.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsLocalUtils.h"
#include "nsIMsgFolder.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

nsLocalMessage::nsLocalMessage(void)
{

//  NS_INIT_REFCNT(); done by superclass
}

nsLocalMessage::~nsLocalMessage(void)
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsLocalMessage, nsMessage, nsIDBMessage)

NS_IMETHODIMP nsLocalMessage::GetMsgFolder(nsIMsgFolder **folder)
{
	nsresult rv;
	if(mFolder)
	{
		*folder = mFolder;
		NS_ADDREF(mFolder);
		rv = NS_OK;
	}
	else
	{
		rv = GetFolderFromURI(folder);
	}
	return rv;


}

nsresult nsLocalMessage::GetFolderFromURI(nsIMsgFolder **folder)
{
	nsresult rv;
	nsXPIDLCString uri;
	nsCOMPtr<nsIRDFResource> resource;
	if(NS_SUCCEEDED( rv = QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(resource))))
	{
		resource->GetValue( getter_Copies(uri) );
		nsCAutoString messageFolderURIStr;
		nsMsgKey key;
		nsParseLocalMessageURI(uri, messageFolderURIStr, &key);
		nsCAutoString folderOnly, folderURIStr;

		if (messageFolderURIStr.Find(kMailboxMessageRootURI) != ((PRInt32)-1))
		{
			messageFolderURIStr.Right(folderOnly, messageFolderURIStr.Length() -nsCRT::strlen(kMailboxMessageRootURI));
			folderURIStr = kMailboxRootURI;
			folderURIStr+= folderOnly;
			nsCOMPtr<nsIRDFResource> folderResource;
			const char *folderURI = folderURIStr.GetBuffer();

			NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
			if (NS_SUCCEEDED(rv))   // always check this before proceeding 
			{
				rv = rdfService->GetResource(folderURI, getter_AddRefs(folderResource));
				if(NS_SUCCEEDED(rv))
				{
					rv = NS_SUCCEEDED(folderResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)folder));
				}
			}
		}

	}
	return rv;
}

