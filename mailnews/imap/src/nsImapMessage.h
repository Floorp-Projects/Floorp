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

/********************************************************************************************************
 
   Interface for representing Local Mail folders.
 
*********************************************************************************************************/

#ifndef nsImapMessage_h__
#define nsImapMessage_h__

#include "nsMessage.h" /* include the interface we are going to support */


// 225310c0-f69e-11d2-8d6d-00805f8a6617
#define NS_IMAPMESSAGERESOURCE_CID \
	{0x225310c0, 0xf69e, 0x11d2, \
	{0x8d, 0x6d, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17}}

class nsImapMessage : public nsMessage
{
public:
	nsImapMessage(void);
	virtual ~nsImapMessage(void);

	NS_DECL_ISUPPORTS_INHERITED
	NS_IMETHOD GetMsgFolder(nsIMsgFolder **folder);

protected:
	nsresult GetFolderFromURI(nsIMsgFolder **folder);

};

#endif //nsImapMessage_h__