/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvKOCID_h___
#define nsUCvKOCID_h___

#include "nsISupports.h"

// Class ID for our EUCKRToUnicode charset converter
// {379C2775-EC77-11d2-8AAC-00600811A836}
#define NS_EUCKRTOUNICODE_CID \
  { 0x379c2775, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our ISO2022KRToUnicode charset converter
// {BA61519f-1DFA-11d3-B3BF-00805F8A6670}
#define NS_ISO2022KRTOUNICODE_CID \
  { 0xba61519f, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToEUCKR charset converter
// {379C2778-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCKR_CID \
  { 0x379c2778, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToISO2022KR charset converter
// {BA6151A0-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOISO2022KR_CID \
  { 0xba6151a0, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

#endif /* nsUCvKOCID_h___ */
