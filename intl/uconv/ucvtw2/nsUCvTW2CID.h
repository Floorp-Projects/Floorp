/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvTW2CID_h___
#define nsUCvTW2CID_h___

#include "nsISupports.h"

// Class ID for our EUCTWToUnicode charset converter
// {379C2771-EC77-11d2-8AAC-00600811A836}
#define NS_EUCTWTOUNICODE_CID \
  { 0x379c2771, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToEUCTW charset converter
// {379C2776-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCTW_CID \
  { 0x379c2776, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#endif /* nsUCvTW2CID_h___ */
