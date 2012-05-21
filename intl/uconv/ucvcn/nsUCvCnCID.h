/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvCnCID_h___
#define nsUCvCnCID_h___

#include "nsISupports.h"

// Class ID for our GB2312ToUnicode charset converter// {379C2774-EC77-11d2-8AAC-00600811A836}
#define NS_GB2312TOUNICODE_CID \
  { 0x379c2774, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our ISO2022CNToUnicode charset converter
// {BA615199-1DFA-11d3-B3BF-00805F8A6670}
#define NS_ISO2022CNTOUNICODE_CID \
  { 0xba615199, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}
 
// Class ID for our HZToUnicode charset converter
// {BA61519A-1DFA-11d3-B3BF-00805F8A6670}
#define NS_HZTOUNICODE_CID \
  { 0xba61519a, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our GBKToUnicode charset converter
// {BA61519E-1DFA-11d3-B3BF-00805F8A6670}
#define NS_GBKTOUNICODE_CID \
  { 0xba61519e, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGB2312 charset converter
// {379C2777-EC77-11d2-8AAC-00600811A836}
#define NS_UNICODETOGB2312_CID \
  { 0x379c2777, 0xec77, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

// Class ID for our UnicodeToGBK charset converter
// {BA61519B-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOGBK_CID \
  { 0xba61519b, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToISO2022CN charset converter
// {BA61519C-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOISO2022CN_CID \
  { 0xba61519c, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToHZ charset converter
// {BA61519D-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOHZ_CID \
  { 0xba61519d, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGB18030 charset converter
// {A59DA932-4091-11d5-A145-005004832142}
#define NS_UNICODETOGB18030_CID \
  { 0xa59da932, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

// Class ID for our GBKToUnicode charset converter
// {A59DA935-4091-11d5-A145-005004832142}
#define NS_GB18030TOUNICODE_CID \
  { 0xa59da935, 0x4091, 0x11d5, { 0xa1, 0x45, 0x0, 0x50, 0x4, 0x83, 0x21, 0x42 } }

#endif /* nsUCvCnCID_h___ */
