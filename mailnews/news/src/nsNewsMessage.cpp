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
 */


#include "msgCore.h"    // precompiled header...
#include "nsIMsgHdr.h"
#include "nsIMsgFolder.h"
#include "nsNewsMessage.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsNewsUtils.h"

static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

nsNewsMessage::nsNewsMessage(void)
{

//  NS_INIT_REFCNT(); done by superclass
}

nsNewsMessage::~nsNewsMessage(void)
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsNewsMessage, nsMessage, nsIDBMessage)

NS_IMETHODIMP nsNewsMessage::GetMsgFolder(nsIMsgFolder **folder)
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

//Note this is the same as the function in LocalMessage except for news uri specific stuff.
//I'm not convinced that parsing the url is going to be different so I'm setting this up as a
//separate function.  If it turns out this isn't the case we could write a utility function that
//both message classes can use.
nsresult nsNewsMessage::GetFolderFromURI(nsIMsgFolder **folder)
{
	nsresult rv;
	nsXPIDLCString uri;
	nsCOMPtr <nsIRDFResource> resource;
	if(NS_SUCCEEDED( rv = QueryInterface(nsIRDFResource::GetIID(), getter_AddRefs(resource))))
	{
		resource->GetValue( getter_Copies(uri) );
		nsCAutoString messageFolderURIStr;
		nsMsgKey key;
		nsParseNewsMessageURI(uri, messageFolderURIStr, &key);
		nsCAutoString folderOnly;
		nsCAutoString folderURIStr;

		if (messageFolderURIStr.Find(kNewsMessageRootURI) != ((PRInt32)-1))			{
			messageFolderURIStr.Right(folderOnly, messageFolderURIStr.Length() - kNewsMessageRootURILen);
			folderURIStr = kNewsRootURI;
			folderURIStr+= folderOnly;
			nsCOMPtr <nsIRDFResource> folderResource;

			NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
			if (NS_SUCCEEDED(rv))   // always check this before proceeding 
			{
				rv = rdfService->GetResource(folderURIStr, getter_AddRefs(folderResource));
				if(NS_SUCCEEDED(rv))
				{
					rv = NS_SUCCEEDED(folderResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)folder));
				}
			}
		}
	}
	return rv;
}

