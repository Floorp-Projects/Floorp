/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsIPICS_h__
#define nsIPICS_h__

#include "nsISupports.h"
#include "nsIPref.h"


// {DFB4E4D2-AFC3-11d2-8286-00805F2AB103}
#define NS_IPICS_IID \
{0xdfb4e4d2, 0xafc3, 0x11d2, {0x82, 0x86, 0x0, 0x80, 0x5f, 0x2a, 0xb1, 0x3}}


// {DFB4E4D3-AFC3-11d2-8286-00805F2AB103}
#define NS_PICS_CID \
{0xdfb4e4d3, 0xafc3, 0x11d2,{0x82, 0x86, 0x0, 0x80, 0x5f, 0x2a, 0xb1, 0x3}}


class nsIPICS : public nsISupports {
public:
    
    NS_IMETHOD ProcessPICSLabel(char *label) = 0;
	NS_IMETHOD Init() = 0;
};

/* ProgID prefixes for PICS DLL registration. */
#define NS_PICS_PROGID                           "component:||netscape|extensions|pics"


#endif /* nsIPICS_h__ */
