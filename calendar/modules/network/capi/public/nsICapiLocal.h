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

#ifndef nsICapiLocal_h___
#define nsICapiLocal_h___

#include "nsISupports.h"
#include "nsString.h"

#include "capi.h"

//0bd252a0-41fc-11d2-924a-00805f8a7ab6
#define NS_ICAPI_LOCAL_IID   \
{ 0x0bd252a0, 0x41fc, 0x11d2,    \
{ 0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICapiLocal : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

NS_IMETHOD_(CAPIStatus) CAPI_LogonCurl( 
    const char* psCurl,         /* i: Calendar store (and ":extra" information )  */
    const char* psPassword,     /* i: password for sUser  */
    long lFlags,                /* i: bit flags (none at this time; set to 0)  */
    CAPISession* pSession) = 0;     /* o: the session  */
  

};

#endif /* nsICapiLocal_h___ */
