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

// 225310c0-f69e-11d2-8d6d-00805f8a6617
#define NS_IMAPMESSAGERESOURCE_CID \
	{0x225310c0, 0xf69e, 0x11d2, \
	{0x8d, 0x6d, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0x17}}

#endif
