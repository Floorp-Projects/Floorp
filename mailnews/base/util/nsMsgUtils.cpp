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
#include "nsCOMPtr.h"
#include "nsString.h"

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


NS_IMPL_ISUPPORTS(nsMessageFromMsgHdrEnumerator, nsIEnumerator::GetIID())

nsMessageFromMsgHdrEnumerator::nsMessageFromMsgHdrEnumerator(nsIEnumerator *srcEnumerator,
															 nsIMsgFolder *folder)
{
    NS_INIT_REFCNT();

	mSrcEnumerator = dont_QueryInterface(srcEnumerator);
	mFolder = dont_QueryInterface(folder);

}

nsMessageFromMsgHdrEnumerator::~nsMessageFromMsgHdrEnumerator()
{
	//member variables are nsCOMPtr's
}

NS_IMETHODIMP nsMessageFromMsgHdrEnumerator::First(void)
{
	return mSrcEnumerator->First();

}

NS_IMETHODIMP nsMessageFromMsgHdrEnumerator::Next(void)
{
	return mSrcEnumerator->Next();
}

NS_IMETHODIMP nsMessageFromMsgHdrEnumerator::CurrentItem(nsISupports **aItem)
{
	nsCOMPtr<nsISupports> currentItem;
	nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
	nsCOMPtr<nsIMessage> message;
	nsresult rv;

	rv = mSrcEnumerator->CurrentItem(getter_AddRefs(currentItem));
	if(NS_SUCCEEDED(rv))
	{
		msgDBHdr = do_QueryInterface(currentItem, &rv);
	}

	if(NS_SUCCEEDED(rv))
	{
		rv = mFolder->CreateMessageFromMsgDBHdr(msgDBHdr, getter_AddRefs(message));
	}

	if(NS_SUCCEEDED(rv))
	{
		*aItem = message;
		NS_ADDREF(*aItem);
	}

	return rv;
}

NS_IMETHODIMP nsMessageFromMsgHdrEnumerator::IsDone(void)
{
	return mSrcEnumerator->IsDone();
}

nsresult NS_NewMessageFromMsgHdrEnumerator(nsIEnumerator *srcEnumerator,
										   nsIMsgFolder *folder,	
										   nsMessageFromMsgHdrEnumerator **messageEnumerator)
{
	if(!messageEnumerator)
		return NS_ERROR_NULL_POINTER;

	*messageEnumerator =	new nsMessageFromMsgHdrEnumerator(srcEnumerator, folder);

	if(!messageEnumerator)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF(*messageEnumerator);
	return NS_OK;

}

// Where should this live? It's a utility used to convert a string priority, e.g., "High, Low, Normal" to an enum.
// Perhaps we should have an interface that groups together all these utilities...
nsresult NS_MsgGetPriorityFromString(const char *priority, nsMsgPriority *outPriority)
{
	if (!outPriority)
		return NS_ERROR_NULL_POINTER;

	nsMsgPriority retPriority = nsMsgPriorityNormal;

	if (PL_strcasestr(priority, "Normal") != NULL)
		retPriority = nsMsgPriorityNormal;
	else if (PL_strcasestr(priority, "Lowest") != NULL)
		retPriority = nsMsgPriorityLowest;
	else if (PL_strcasestr(priority, "Highest") != NULL)
		retPriority = nsMsgPriorityHighest;
	else if (PL_strcasestr(priority, "High") != NULL || 
			 PL_strcasestr(priority, "Urgent") != NULL)
		retPriority = nsMsgPriorityHigh;
	else if (PL_strcasestr(priority, "Low") != NULL ||
			 PL_strcasestr(priority, "Non-urgent") != NULL)
		retPriority = nsMsgPriorityLow;
	else if (PL_strcasestr(priority, "1") != NULL)
		retPriority = nsMsgPriorityHighest;
	else if (PL_strcasestr(priority, "2") != NULL)
		retPriority = nsMsgPriorityHigh;
	else if (PL_strcasestr(priority, "3") != NULL)
		retPriority = nsMsgPriorityNormal;
	else if (PL_strcasestr(priority, "4") != NULL)
		retPriority = nsMsgPriorityLow;
	else if (PL_strcasestr(priority, "5") != NULL)
	    retPriority = nsMsgPriorityLowest;
	else
		retPriority = nsMsgPriorityNormal;
	*outPriority = retPriority;
	return NS_OK;
		//return nsMsgNoPriority;
}


nsresult NS_MsgGetUntranslatedPriorityName (nsMsgPriority p, nsString2 *outName)
{
	if (!outName)
		return NS_ERROR_NULL_POINTER;
	switch (p)
	{
	case nsMsgPriorityNotSet:
	case nsMsgPriorityNone:
		*outName = "None";
		break;
	case nsMsgPriorityLowest:
		*outName = "Lowest";
		break;
	case nsMsgPriorityLow:
		*outName = "Low";
		break;
	case nsMsgPriorityNormal:
		*outName = "Normal";
		break;
	case nsMsgPriorityHigh:
		*outName = "High";
		break;
	case nsMsgPriorityHighest:
		*outName = "Highest";
		break;
	default:
		NS_ASSERTION(PR_FALSE, "invalid priority value");
	}
	return NS_OK;
}

