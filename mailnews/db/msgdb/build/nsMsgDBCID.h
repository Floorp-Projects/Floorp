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

#ifndef nsMsgDBCID_h__
#define nsMsgDBCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

//	a86c86ae-e97f-11d2-a506-0060b0fc04b7
#define NS_MAILDB_CID                      \
{ 0xa86c86ae, 0xe97f, 0xa506,                  \
    { 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// 36414aa0-e980-11d2-a506-0060b0fc04b7
#define NS_NEWSDB_CID                      \
{ 0x36414aa0, 0xe980, 0x11d2,                  \
    { 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// 9e4b07ee-e980-11d2-a506-0060b0fc04b7
#define NS_IMAPDB_CID                      \
{ 0x9e4b07ee, 0xe980, 0x11d2,                  \
    { 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

// b0908e06-dc06-11d2-8a46-0060b0fc04d2
#define NS_MAILBOXMESSAGERESOURCE_CID               \
{ 0xb0908e06,0xdc06, 0x11d2,											 \
	{0x8a, 0x46, 0x00, 0x60, 0xb0, 0xfc, 0x4, 0xd2}	 \
}

// sspitzer:  need to use uuidgen to create this. 
// this is a tempory cut-and-paste-and-alter job

// b0908e06-dc06-11d2-8a46-00600bcf04d2
#define NS_NEWSMESSAGERESOURCE_CID               \
{ 0xb0908e06, 0xdc06, 0x11d2,											 \
	{0x8a, 0x46, 0x00, 0x60, 0x0b, 0xcf, 0x4, 0xd2}	 \
}


#endif
