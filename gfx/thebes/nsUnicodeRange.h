/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

class nsIAtom;

// The following constants define unicode subranges
// values below kRangeNum must be continuous so that we can map to 
// lang group directly.
// all ranges we care about should be defined under 32, that allows 
// us to store range using bits of a uint32_t

// frequently used range definitions
const uint8_t   kRangeCyrillic =    0;
const uint8_t   kRangeGreek    =    1;
const uint8_t   kRangeHebrew   =    2;
const uint8_t   kRangeArabic   =    3;
const uint8_t   kRangeThai     =    4;
const uint8_t   kRangeKorean   =    5;
const uint8_t   kRangeJapanese =    6;
const uint8_t   kRangeSChinese =    7;
const uint8_t   kRangeTChinese =    8;
const uint8_t   kRangeDevanagari =  9;
const uint8_t   kRangeTamil    =   10;
const uint8_t   kRangeArmenian =   11;
const uint8_t   kRangeBengali  =   12;
const uint8_t   kRangeCanadian =   13;
const uint8_t   kRangeEthiopic =   14;
const uint8_t   kRangeGeorgian =   15;
const uint8_t   kRangeGujarati =   16;
const uint8_t   kRangeGurmukhi =   17;
const uint8_t   kRangeKhmer    =   18;
const uint8_t   kRangeMalayalam =  19;
const uint8_t   kRangeOriya     =  20;
const uint8_t   kRangeTelugu    =  21;
const uint8_t   kRangeKannada   =  22;
const uint8_t   kRangeSinhala   =  23;
const uint8_t   kRangeTibetan   =  24;

const uint8_t   kRangeSpecificItemNum = 25;

//range/rangeSet grow to this place 25-29

const uint8_t   kRangeSetStart  =  30;   // range set definition starts from here
const uint8_t   kRangeSetLatin  =  30;
const uint8_t   kRangeSetCJK    =  31;
const uint8_t   kRangeSetEnd    =  31;   // range set definition ends here, this
                                         // and smaller ranges are used as bit
                                         // mask, don't increase this value.

// less frequently used range definition
const uint8_t   kRangeSurrogate            = 32;
const uint8_t   kRangePrivate              = 33;
const uint8_t   kRangeMisc                 = 34;
const uint8_t   kRangeUnassigned           = 35;
const uint8_t   kRangeSyriac               = 36;
const uint8_t   kRangeThaana               = 37;
const uint8_t   kRangeLao                  = 38;
const uint8_t   kRangeMyanmar              = 39;
const uint8_t   kRangeCherokee             = 40;
const uint8_t   kRangeOghamRunic           = 41;
const uint8_t   kRangeMongolian            = 42;
const uint8_t   kRangeMathOperators        = 43;
const uint8_t   kRangeMiscTechnical        = 44;
const uint8_t   kRangeControlOpticalEnclose = 45;
const uint8_t   kRangeBoxBlockGeometrics   = 46;
const uint8_t   kRangeMiscSymbols          = 47;
const uint8_t   kRangeDingbats             = 48;
const uint8_t   kRangeBraillePattern       = 49;
const uint8_t   kRangeYi                   = 50;
const uint8_t   kRangeCombiningDiacriticalMarks = 51;
const uint8_t   kRangeSpecials             = 52;

// aggregate ranges for non-BMP codepoints (u+2xxxx are all CJK)
const uint8_t   kRangeSMP                  = 53;  // u+1xxxx
const uint8_t   kRangeHigherPlanes         = 54;  // u+3xxxx and above

const uint8_t   kRangeTableBase   = 128;    //values over 127 are reserved for internal use only
const uint8_t   kRangeTertiaryTable  = 145; // leave room for 16 subtable 
                                            // indices (kRangeTableBase + 1 ..
                                            // kRangeTableBase + 16)



uint32_t FindCharUnicodeRange(uint32_t ch);
nsIAtom* LangGroupFromUnicodeRange(uint8_t unicodeRange);
