/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsImapIncomingServer_h
#define __nsImapIncomingServer_h

#include "nsIImapIncomingServer.h"
#include "nscore.h"

#define NS_IMAPINCOMINGSERVER_CID				  \
{ /* 8D3675E0-ED46-11d2-8077-006008128C4E */      \
 0x8d3675e0, 0xed46, 0x11d2,	                  \
 {0x80, 0x77, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

NS_BEGIN_EXTERN_C

nsresult NS_NewImapIncomingServer(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif
