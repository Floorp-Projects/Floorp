/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvTWCID_h___
#define nsUCvTWCID_h___

#include "nsISupports.h"

// Class ID for our BIG5ToUnicode charset converter
// {EFC323E1-EC62-11d2-8AAC-00600811A836}
#define NS_BIG5TOUNICODE_CID \
  { 0xefc323e1, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToBIG5 charset converter
// {EFC323E2-EC62-11d2-8AAC-00600811A836}
#define NS_UNICODETOBIG5_CID \
  { 0xefc323e2, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our BIG5HKSCSToUnicode charset converter
// {BA6151BB-EC62-11d2-8AAC-00600811A836}
#define NS_BIG5HKSCSTOUNICODE_CID \
  { 0xba6151bb, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToBIG5HKSCS charset converter
// {BA6151BC-EC62-11d2-8AAC-00600811A836}
#define NS_UNICODETOBIG5HKSCS_CID \
  { 0xba6151bc, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToHKSCS charset converter
// {A59DA931-4091-11d5-A145-005004832142}
#define NS_UNICODETOHKSCS_CID \
  { 0xa59da931, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }


#endif /* nsUCvTWCID_h___ */
