/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __MNPREFID_H_
#define __MNPREFID_H_

#ifndef MOZ_COMMUNICATOR_IIDS

/* d3852820-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(IID_MailNewsInterface, 
0xd3852820, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* cd252be0-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(IID_OfflineInterface, 
0xcd252be0, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* c53af140-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(CLSID_MailNewsPrefs,
0xc53af140, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* a61b8da0-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(CLSID_OfflinePrefs,
0xa61b8da0, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

#else /* MOZ_COMMUNICATOR_IIDS */  

// {E8D6B4F0-8B58-11d0-9B63-00805F8ADDDE}
DEFINE_GUID(IID_MailNewsInterface, 
0xe8d6b4f0, 0x8b58, 0x11d0, 0x9b, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0xde);

// {DDF4AB60-8B84-11d0-9B63-00805F8ADDDE}
DEFINE_GUID(IID_OfflineInterface, 
0xddf4ab60, 0x8b84, 0x11d0, 0x9b, 0x63, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0xde);

// {CC3E2871-43CA-11d0-B6D8-00805F8ADDDE}
DEFINE_GUID(CLSID_MailNewsPrefs,
0xcc3e2871, 0x43ca, 0x11d0, 0xb6, 0xd8, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0xde);

// {CC3E2872-43CA-11d0-B6D8-00805F8ADDDE}
DEFINE_GUID(CLSID_OfflinePrefs,
0xcc3e2872, 0x43ca, 0x11d0, 0xb6, 0xd8, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0xde);

#endif /* MOZ_COMMUNICATOR_IIDS */

#endif /* __MNPREFID_H_ */

