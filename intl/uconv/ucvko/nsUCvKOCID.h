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

#ifndef nsUCvKOCID_h___
#define nsUCvKOCID_h___

#include "nsISupports.h"

// Class ID for our EUCKRToUnicode charset converter
// {379C2775-EC77-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kEUCKRToUnicodeCID,
  0x379c2775, 0xec77, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_EUCKRTOUNICODE_CID \
  { 0x379c2775, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our ISO2022KRToUnicode charset converter
// {BA61519f-1DFA-11d3-B3BF-00805F8A6670}
NS_DECLARE_ID(kISO2022KRToUnicodeCID,
  0xba61519f, 0x1dfa, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_ISO2022KRTOUNICODE_CID \
  { 0xba61519f, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToEUCKR charset converter
// {379C2778-EC77-11d2-8AAC-00600811A836}
NS_DECLARE_ID(kUnicodeToEUCKRCID, 
  0x379c2778, 0xec77, 0x11d2, 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36);
#define NS_UNICODETOEUCKR_CID \
  { 0x379c2778, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToISO2022KR charset converter
// {BA6151A0-1DFA-11d3-B3BF-00805F8A6670}
NS_DECLARE_ID(kUnicodeToISO2022KRCID, 
  0xba6151a0, 0x1dfa, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOISO2022KR_CID \
  { 0xba6151a0, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToKSC5601 charset converter
// {BA615194-1DFA-11d3-B3BF-00805F8A6670}
NS_DECLARE_ID(kUnicodeToKSC5601CID, 
  0xba615194, 0x1dfa, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOKSC5601_CID \
  { 0xba615194, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToCP949 charset converter
NS_DECLARE_ID(kUnicodeToCP949CID, 
// {9416BFBE-1F93-11d3-B3BF-00805F8A6670}
  0x9416bfbe, 0x1f93, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOCP949_CID \
  { 0x9416bfbe, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our CP949ToUnicode charset converter
NS_DECLARE_ID(kCP949ToUnicodeCID,
// {9416BFBF-1F93-11d3-B3BF-00805F8A6670}
  0x9416bfbf, 0x1f93, 0x11d3, 0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_CP949TOUNICODE_CID \
  { 0x9416bfbf, 0x1f93, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToX11Johab charset converter
NS_DECLARE_ID(kUnicodeToX11JohabCID, 
// {21DD6A01-413C-11d3-B3C3-00805F8A6670}
  0x21dd6a01, 0x413c, 0x11d3, 0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70);
#define NS_UNICODETOX11JOHAB_CID \
  { 0x21dd6a01, 0x413c, 0x11d3, {0xb3, 0xc3, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

#endif /* nsUCvKOCID_h___ */
