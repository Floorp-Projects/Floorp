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
#include "nsIMsgFolder.h"
#include "nsImapMessage.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsImapUtils.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

nsImapMessage::nsImapMessage(void)
{

//  NS_INIT_REFCNT(); done by superclass
}

nsImapMessage::~nsImapMessage(void)
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsImapMessage, nsMessage, nsIDBMessage)

NS_IMETHODIMP nsImapMessage::GetMsgFolder(nsIMsgFolder **folder)
{
	nsresult rv;
  rv = nsMessage::GetMsgFolder(folder);
  if (NS_FAILED(rv))
		rv = GetFolderFromURI(folder);
	return rv;
}

NS_IMETHODIMP nsImapMessage::GetMessageType(PRUint32 *aMessageType)
{
	if(!aMessageType)
		return NS_ERROR_NULL_POINTER;

	*aMessageType = nsIMessage::MailMessage;
	return NS_OK;
}

nsresult nsImapMessage::GetFolderFromURI(nsIMsgFolder **folder)
{
	nsresult rv;
	nsXPIDLCString uri;
	nsIRDFResource *resource;
	if(NS_SUCCEEDED( rv = QueryInterface(NS_GET_IID(nsIRDFResource), (void**)&resource)))
	{
		resource->GetValue( getter_Copies(uri) );
		nsCAutoString messageFolderURIStr;
		nsMsgKey key;
		nsParseImapMessageURI(uri, messageFolderURIStr, &key, nsnull);
		nsCAutoString folderOnly, folderURIStr;

		if (messageFolderURIStr.Find(kImapRootURI) != ((PRInt32)-1))
		{
			messageFolderURIStr.Right(folderOnly, messageFolderURIStr.Length() -nsCRT::strlen(kImapRootURI));
			folderURIStr = kImapRootURI;
			folderURIStr+= folderOnly;
			nsIRDFResource *folderResource;
			const char *folderURI = folderURIStr.GetBuffer();

			NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
			if (NS_SUCCEEDED(rv))   // always check this before proceeding 
			{
				rv = rdfService->GetResource(folderURI, &folderResource);
				if(NS_SUCCEEDED(rv))
				{
					rv = NS_SUCCEEDED(folderResource->QueryInterface(NS_GET_IID(nsIMsgFolder), (void**)folder));
					NS_RELEASE(folderResource);
				}
			}
		}

		NS_RELEASE(resource);
	}
	return rv;
}
