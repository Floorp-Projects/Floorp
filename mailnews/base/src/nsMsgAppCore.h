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

#ifndef __nsMsgAppCore_h
#define __nsMsgAppCore_h

#include "nsCom.h"
#include "nsIDOMMsgAppCore.h"

#define NS_MSGAPPCORE_CID \
{ /* 3f181950-c14d-11d2-b7f2-00805f05ffa5 */      \
  0x3f181950, 0xc14d, 0x11d2,											\
    {0xb7, 0xf2, 0x0, 0x80, 0x5f, 0x05, 0xff, 0xa5}}

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgAppCore(const nsIID &aIID, void **);

NS_END_EXTERN_C

#endif
