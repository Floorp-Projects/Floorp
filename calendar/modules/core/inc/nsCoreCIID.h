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

#ifndef nsCoreCIID_h__
#define nsCoreCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nscalexport.h"

// d4797370-4cc8-11d2-924a-00805f8a7ab6
#define NS_LAYER_CID   \
{ 0xd4797370, 0x4cc8, 0x11d2, \
  {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// dabe52d0-4ccb-11d2-924a-00805f8a7ab6
#define NS_LAYER_COLLECTION_CID   \
{ 0xdabe52d0, 0x4ccb, 0x11d2, \
  {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// 6858eab0-4cd8-11d2-924a-00805f8a7ab6
#define NS_CALENDAR_USER_CID   \
{ 0x6858eab0, 0x4cd8, 0x11d2, \
  {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// fd439e90-4e67-11d2-924a-00805f8a7ab6
#define NS_CALENDAR_MODEL_CID   \
{ 0xfd439e90, 0x4e67, 0x11d2, \
  {0x92, 0x4a, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

//ea7225c0-6313-11d2-b564-0060088a4b1d
#define NS_CAL_NET_FETCH_COLLECTOR_CID   \
  { 0xea7225c0, 0x6313, 0x11d2,    \
  { 0xb5, 0x64, 0x00, 0x60, 0x08, 0x8a, 0x4b, 0x1d } }

#endif
