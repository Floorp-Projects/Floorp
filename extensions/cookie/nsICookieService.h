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

#ifndef nsICookieService_h__
#define nsICookieService_h__

#include "nsISupports.h"

// {AB397772-12D3-11d3-8AD1-00105A1B8860}
#define NS_ICOOKIESERVICE_IID \
{ 0xab397772, 0x12d3, 0x11d3, { 0x8a, 0xd1, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } };


// {AB397774-12D3-11d3-8AD1-00105A1B8860}
#define NS_COOKIESERVICE_CID \
{ 0xab397774, 0x12d3, 0x11d3, { 0x8a, 0xd1, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } };


class nsICookieService : public nsISupports {
public:
    
  NS_IMETHOD Init() = 0;
};


/* ProgID prefixes for Cookie DLL registration. */
#define NS_COOKIESERVICE_PROGID                           "component:||netscape|cookie"


#endif /* nsICookieService_h__ */
