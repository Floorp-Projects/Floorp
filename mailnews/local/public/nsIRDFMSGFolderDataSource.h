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

#ifndef nsIRDFMSGFolderDataSource_h__
#define nsIRDFMSGFolderDataSource_h__

#include "nsIRDFDataSource.h"


// {915EA540-AFD6-11d2-956E-00805F8AC615}
#define NS_IRDFMSGFOLDERDATASOURCE_IID \
{ 0x915ea540, 0xafd6, 0x11d2, { 0x95, 0x6e, 0x0, 0x80, 0x5f, 0x8a, 0xc6, 0x15 } }


class nsIRDFMSGFolderDataSource : public nsIRDFDataSource {
public:
};

extern NS_EXPORT nsresult NS_NewRDFMSGFolderDataSource(nsIRDFDataSource** result);

#endif

