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

#ifndef _NSMSGUTILS_H
#define _NSMSGUTILS_H

#include "nsIMsgMessageService.h"
#include "nsString.h"
#include "nsIEnumerator.h"
#include "nsIMsgFolder.h"
#include "msgCore.h"
#include "nsCOMPtr.h"

//These are utility functions that can used throughout the mailnews code

//Utilities for getting a message service.
NS_MSG_BASE nsresult GetMessageServiceProgIDForURI(const char *uri, nsString &progID);
//Use ReleaseMessageServiceFromURI to release the service.
NS_MSG_BASE nsresult GetMessageServiceFromURI(const char *uri, nsIMsgMessageService **messageService);
NS_MSG_BASE nsresult ReleaseMessageServiceFromURI(const char *uri, nsIMsgMessageService *messageService);


//An enumerator for converting nsIMsgHdrs to nsIMessages.
class NS_MSG_BASE nsMessageFromMsgHdrEnumerator: public nsISimpleEnumerator
{
protected:
	nsCOMPtr<nsISimpleEnumerator> mSrcEnumerator;
	nsCOMPtr<nsIMsgFolder> mFolder;

public:
	NS_DECL_ISUPPORTS
	nsMessageFromMsgHdrEnumerator(nsISimpleEnumerator *srcEnumerator, nsIMsgFolder *folder);
	nsMessageFromMsgHdrEnumerator(){} //Default constructor that does nothing so nsComPtr will work.
	virtual ~nsMessageFromMsgHdrEnumerator();

  NS_DECL_NSISIMPLEENUMERATOR
};

NS_MSG_BASE nsresult NS_NewMessageFromMsgHdrEnumerator(nsISimpleEnumerator *srcEnumerator,
										   nsIMsgFolder *folder,	
										   nsMessageFromMsgHdrEnumerator **messageEnumerator);

NS_MSG_BASE nsresult NS_MsgGetPriorityFromString(const char *priority, nsMsgPriority *outPriority);

NS_MSG_BASE nsresult NS_MsgGetUntranslatedPriorityName (nsMsgPriority p, nsString *outName);

NS_MSG_BASE nsresult NS_MsgHashIfNecessary(nsCAutoString &name);

#endif

