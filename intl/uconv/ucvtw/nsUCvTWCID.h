/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsUCvTWCID_h___
#define nsUCvTWCID_h___

#include "nsISupports.h"

// Class ID for our BIG5ToUnicode charset converter
// {EFC323E1-EC62-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kBIG5ToUnicodeCID,
  0xefc323e1, 0xec62, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_BIG5TOUNICODE_CID \
  { 0xefc323e1, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToBIG5 charset converter
// {EFC323E2-EC62-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToBIG5CID, 
  0xefc323e2, 0xec62, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_UNICODETOBIG5_CID \
  { 0xefc323e2, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}


// Class ID for our UnicodeToBIG5NoAscii charset converter
// {BA615195-1DFA-11d3-B3BF-00805F8A6670}
NS_DECLARE_ID(kUnicodeToBIG5NoAsciiCID, 
0xba615195, 0x1dfa, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOBIG5NOASCII_CID \
  { 0xba615195, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our CP950ToUnicode charset converter
NS_DECLARE_ID(kCP950ToUnicodeCID,
// {9416BFBA-1F93-11d3-B3BF-00805F8A6670}
0x9416bfba, 0x1f93, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_CP950TOUNICODE_CID \
  { 0x9416bfba, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}


// Class ID for our UnicodeToCP950 charset converter
NS_DECLARE_ID(kUnicodeToCP950CID, 
// {9416BFBB-1F93-11d3-B3BF-00805F8A6670}
0x9416bfbb, 0x1f93, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOCP950_CID \
 { 0x9416bfbb, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}


// Class ID for our BIG5HKSCSToUnicode charset converter
// {BA6151BB-EC62-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kBIG5HKSCSToUnicodeCID,
  0xba6151bb, 0xec62, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_BIG5HKSCSTOUNICODE_CID \
  { 0xba6151bb, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToBIG5HKSCS charset converter
// {BA6151BC-EC62-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToBIG5HKSCSCID,
  0xba6151bc, 0xec62, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_UNICODETOBIG5HKSCS_CID \
  { 0xba6151bc, 0xec62, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#endif /* nsUCvTWCID_h___ */
