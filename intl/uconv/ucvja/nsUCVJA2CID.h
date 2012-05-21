/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCVJA2CID_h___
#define nsUCVJA2CID_h___

#include "nsISupports.h"

// Class ID for our EUCJPToUnicode charset converter
// {3F6FE6A1-AC0A-11d2-B3AE-00805F8A6670}
#define NS_EUCJPTOUNICODE_CID \
  {0x3f6fe6a1, 0xac0a, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our ISO2022JPToUnicode charset converter
// {3F6FE6A2-AC0A-11d2-B3AE-00805F8A6670}
#define NS_ISO2022JPTOUNICODE_CID \
  {0x3f6fe6a2, 0xac0a, 0x11d2, {0xb3, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

// Class ID for our UnicodeToEUCJP charset converter
// {45C23A20-D71C-11d2-8AAC-00600811A836}
#define NS_UNICODETOEUCJP_CID \
  {0x45c23a20, 0xd71c, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

// Class ID for our UnicodeToISO2022JP charset converter
// {4F76E100-D71C-11d2-8AAC-00600811A836}
#define NS_UNICODETOISO2022JP_CID \
  {0x4f76e100, 0xd71c, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}} 

// Class ID for our UnicodeToJISx0201 charset converter
// {BA615191-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOJISX0201_CID \
  {0xba615191, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}} 

#endif /* nsUCVJA2CID_h___ */
