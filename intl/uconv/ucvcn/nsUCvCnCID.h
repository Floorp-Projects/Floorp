/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUCvCnCID_h___
#define nsUCvCnCID_h___

#include "nsISupports.h"

// Class ID for our HZToUnicode charset converter
// {BA61519A-1DFA-11d3-B3BF-00805F8A6670}
#define NS_HZTOUNICODE_CID \
  { 0xba61519a, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our GBKToUnicode charset converter
// {BA61519E-1DFA-11d3-B3BF-00805F8A6670}
#define NS_GBKTOUNICODE_CID \
  { 0xba61519e, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

// Class ID for our UnicodeToGBK charset converter
// {BA61519B-1DFA-11d3-B3BF-00805F8A6670}
#define NS_UNICODETOGBK_CID \
  { 0xba61519b, 0x1dfa, 0x11d3, {0xb3, 0xbf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70}}

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
