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

#ifndef nsMsgLocalCID_h__
#define nsMsgLocalCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define  NS_LOCALMAILFOLDERRESOURCE_CID              \
{ /* e490d22c-cd67-11d2-8cca-0060b0fc14a3 */         \
	0xe490d22c,										 \
    0xcd67,                                          \
    0x11d2,                                          \
    {0x8c, 0xca, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_POP3INCOMINGSERVER_CID									\
{ /* D2876E51-E62C-11d2-B7FC-00805F05FFA5 */			\
 0xd2876e51, 0xe62c, 0x11d2,											\
 {0xb7, 0xfc, 0x0, 0x80, 0x5f, 0x5, 0xff, 0xa5 }}

#endif // nsMsgLocalCID_h__
