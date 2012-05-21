/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCVJACID_h___
#define nsUCVJACID_h___

#include "nsISupports.h"

// Class ID for our SJIS2Unicode charset converter
// {0E6892C1-A9AD-11d2-B3AE-00805F8A6670}
#define NS_SJISTOUNICODE_CID \
  {0xe6892c1, 0xa9ad, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our UnicodeToSJIS charset converter
// {E28AB250-D66D-11d2-8AAC-00600811A836}
#define NS_UNICODETOSJIS_CID \
  {0xe28ab250, 0xd66d, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

#endif /* nsUCVJACID_h___ */
