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

#ifndef __BRPREFID_H_
#define __BRPREFID_H_

#ifndef MOZ_COMMUNICATOR_IIDS

/* 9dac1eb0-f7db-11d0-b099-00805f8a1db7 */
DEFINE_GUID(IID_IIntlFont,
0x9dac1eb0, 0xf7db, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* e4e3ba40-f7dc-11d0-b099-00805f8a1db7 */
DEFINE_GUID(IID_IBrowserPrefs, 
0xe4e3ba40, 0xf7dc, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* fee0f8a0-f7dc-11d0-b099-00805f8a1db7 */
DEFINE_GUID(IID_IEnumHelpers, 
0xfee0f8a0, 0xf7dc, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* 0bd1d720-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(CLSID_AppearancePrefs, 
0x0bd1d720, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* 172f4c20-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(CLSID_BrowserPrefs, 
0x172f4c20, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

/* 2221ca00-f7dd-11d0-b099-00805f8a1db7 */
DEFINE_GUID(CLSID_AdvancedPrefs, 
0x2221ca00, 0xf7dd, 0x11d0, 0xb0, 0x99, 0x00, 0x80, 0x5f, 0x8a, 0x1d, 0xb7);

#else /* MOZ_COMMUNICATOR_IIDS */

// {C98D0190-7D81-11d0-BF8D-00A02468FAB6}
DEFINE_GUID(IID_IIntlFont, 
0xc98d0190, 0x7d81, 0x11d0, 0xbf, 0x8d, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6);

// {37B601C0-8AC8-11d0-83AF-00805F8A274D}
DEFINE_GUID(IID_IBrowserPrefs, 
0x37b601c0, 0x8ac8, 0x11d0, 0x83, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x27, 0x4d);

// {913A4A20-8EBF-11d0-BFAB-00A02468FAB6}
DEFINE_GUID(IID_IEnumHelpers, 
0x913a4a20, 0x8ebf, 0x11d0, 0xbf, 0xab, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6);

// {7865A9A1-33A8-11d0-BED9-00A02468FAB6}
DEFINE_GUID(CLSID_AppearancePrefs, 
0x7865a9a1, 0x33a8, 0x11d0, 0xbe, 0xd9, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6);

// {543EC0D0-6AB7-11d0-BF56-00A02468FAB6}
DEFINE_GUID(CLSID_BrowserPrefs, 
0x543ec0d0, 0x6ab7, 0x11d0, 0xbf, 0x56, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6);

// {543EC0D1-6AB7-11d0-BF56-00A02468FAB6}
DEFINE_GUID(CLSID_AdvancedPrefs, 
0x543ec0d1, 0x6ab7, 0x11d0, 0xbf, 0x56, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6);

#endif /* MOZ_COMMUNICATOR_IIDS */

#endif /* __BRPREFID_H_ */

