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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

  rv = nsMessage::GetMsgFolder(folder);
  if (NS_FAILED(rv))
		rv = GetFolderFromURI(folder);
	return rv;
}

NS_IMETHODIMP nsLocalMessage::GetMessageType(PRUint32 *aMessageType)
{
	if(!aMessageType)
		return NS_ERROR_NULL_POINTER;

	*aMessageType = nsIMessage::MailMessage;
	return NS_OK;
}

nsresult nsLocalMessage::GetFolderFromURI(nsIMsgFolder **folder)
{
	nsresult rv;
	nsXPIDLCString uri;
	nsCOMPtr<nsIRDFResource> resource;
	if(NS_SUCCEEDED( rv = QueryInterface(NS_GET_IID(nsIRDFResource), getter_AddRefs(resource))))
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
					rv = NS_SUCCEEDED(folderResource->QueryInterface(NS_GET_IID(nsIMsgFolder), (void**)folder));
				}
			}
		}

	}
	return rv;
}

