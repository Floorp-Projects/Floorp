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

#ifndef __nsComposeAppCore_h
#define __nsComposeAppCore_h

#include "nsCom.h"
#include "nsIDOMComposeAppCore.h"

#define NS_COMPOSEAPPCORE_CID \
{ /* 4E932BB0-CAA8-11d2-A6F2-0060B0EB39B5 */      \
  0x4e932bb0, 0xcaa8, 0x11d2,											\
    {0xa6, 0xf2, 0x0, 0x60, 0xb0, 0xeb, 0x39, 0xb5}}

NS_BEGIN_EXTERN_C

nsresult NS_NewComposeAppCore(const nsIID &aIID, void **);

NS_END_EXTERN_C

#endif //__nsComposeAppCore_h
