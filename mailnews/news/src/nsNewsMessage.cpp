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

NS_IMPL_ISUPPORTS_INHERITED(nsNewsMessage, nsMessage, nsIMessage)

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
	nsIRDFResource *resource;
	if(NS_SUCCEEDED( rv = QueryInterface(nsIRDFResource::GetIID(), (void**)&resource)))
	{
		resource->GetValue( getter_Copies(uri) );
		nsString messageFolderURIStr;
		nsMsgKey key;
		nsParseNewsMessageURI(uri, messageFolderURIStr, &key);
		nsString folderOnly, folderURIStr;

		if (messageFolderURIStr.Find(kNewsMessageRootURI) != ((PRInt32)-1))			{
			messageFolderURIStr.Right(folderOnly, messageFolderURIStr.Length() -nsCRT::strlen(kNewsMessageRootURI));
			folderURIStr = kNewsRootURI;
			folderURIStr+= folderOnly;
			nsIRDFResource *folderResource;
			char *folderURI = folderURIStr.ToNewCString();

			NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
			if (NS_SUCCEEDED(rv))   // always check this before proceeding 
			{
				rv = rdfService->GetResource(folderURI, &folderResource);
				if(NS_SUCCEEDED(rv))
				{
					rv = NS_SUCCEEDED(folderResource->QueryInterface(nsIMsgFolder::GetIID(), (void**)folder));
					NS_RELEASE(folderResource);
				}
			}


			delete[] folderURI;
		}

		NS_RELEASE(resource);
	}
	return rv;
}

