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

#ifndef __nsMsgAccountDataSource_h
#define __nsMsgAccountDataSource_h

#include "nscore.h"
#include "nsError.h"
#include "nsIID.h"

/* {b0284374-f82a-11d2-8aaa-006008948010} */
#define NS_MSGACCOUNTDATASOURCE_CID \
  {0xb0284374, 0xf82a, 0x11d2, \
    {0x8a, 0xaa, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}


NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgAccountDataSource(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif
