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

#include "nsMsgUtils.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIServiceManager.h"

nsresult GetMessageServiceProgIDForURI(const char *uri, nsString &progID)
{

	nsresult rv = NS_OK;
	//Find protocol
	nsString uriStr = uri;
	PRInt32 pos = uriStr.Find(':');
	if(pos == -1)
		return NS_ERROR_FAILURE;
	nsString protocol;
	uriStr.Left(protocol, pos);

	//Build message service progid
	progID = "component://netscape/messenger/messageservice;type=";
	progID += protocol;

	return rv;
}

nsresult GetMessageServiceFromURI(const char *uri, nsIMsgMessageService **messageService)
{

	nsString progID;
	nsresult rv;

	rv = GetMessageServiceProgIDForURI(uri, progID);
	nsAutoCString progIDStr(progID);

	if(NS_SUCCEEDED(rv))
	{
		rv = nsServiceManager::GetService((const char*)progIDStr, nsIMsgMessageService::GetIID(),
		           (nsISupports**)messageService, nsnull);
	}

	return rv;
}


nsresult ReleaseMessageServiceFromURI(const char *uri, nsIMsgMessageService *messageService)
{
	nsString progID;
	nsresult rv;

	rv = GetMessageServiceProgIDForURI(uri, progID);
	if(NS_SUCCEEDED(rv))
	{
		nsAutoCString progIDStr(progID);
		rv = nsServiceManager::ReleaseService((const char*)progIDStr, messageService);
	}
	return rv;
}
