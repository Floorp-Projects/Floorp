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

#ifndef nsCalUtilCIID_h__
#define nsCalUtilCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"

//a3b8bdb0-1cca-11d2-9246-00805f8a7ab6
#define NS_DATETIME_CID \
{ 0xa3b8bdb0, 0x1cca, 0x11d2, \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

//80cb25a0-223d-11d2-9246-00805f8a7ab6
#define NS_DURATION_CID \
{ 0x80cb25a0, 0x223d, 0x11d2, \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

//cbf2f570-7335-11d2-b57e-0060088a4b1d
#define NS_MCC_CMD_CID \
{ 0xcbf2f570, 0x7335, 0x11d2, \
{ 0xb5, 0x7e, 0x00, 0x60, 0x08, 0x8a, 0x4b, 0x1d } }

#endif
