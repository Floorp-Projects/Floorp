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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// This header file just contains prototypes for the factory methods
// for "builtin" data sources that are included in walletlib.dll. Each of
// these data sources is exposed to the external world via its CID in
// ../include/nsIWalletService.h.

#ifndef nsBuiltinDataSources_h__
#define nsBuiltinDataSources_h__

//#include "nsError.h"

//class nsIWalletDataSource;
//class nsIWalletDataBase;

nsresult NS_NewWalletService(nsIWalletServiceDataSource** result);

#endif // nsBuiltinDataSources_h__
