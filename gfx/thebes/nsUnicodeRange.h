/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"

class nsIAtom;

// The following constants define unicode subranges
// values below kRangeNum must be continuous so that we can map to 
// lang group directly.
// all ranges we care about should be defined under 32, that allows 
// us to store range using bits of a PRUint32

// frequently used range definitions
const PRUint8   kRangeCyrillic =    0;
const PRUint8   kRangeGreek    =    1;
const PRUint8   kRangeTurkish  =    2;
const PRUint8   kRangeHebrew   =    3;
const PRUint8   kRangeArabic   =    4;
const PRUint8   kRangeBaltic   =    5;
const PRUint8   kRangeThai     =    6;
const PRUint8   kRangeKorean   =    7;
const PRUint8   kRangeJapanese =    8;
const PRUint8   kRangeSChinese =    9;
const PRUint8   kRangeTChinese =   10;
const PRUint8   kRangeDevanagari = 11;
const PRUint8   kRangeTamil    =   12;
const PRUint8   kRangeArmenian =   13;
const PRUint8   kRangeBengali  =   14;
const PRUint8   kRangeCanadian =   15;
const PRUint8   kRangeEthiopic =   16;
const PRUint8   kRangeGeorgian =   17;
const PRUint8   kRangeGujarati =   18;
const PRUint8   kRangeGurmukhi =   19;
const PRUint8   kRangeKhmer    =   20;
const PRUint8   kRangeMalayalam =  21;
const PRUint8   kRangeOriya     =  22;
const PRUint8   kRangeTelugu    =  23;
const PRUint8   kRangeKannada   =  24;
const PRUint8   kRangeSinhala   =  25;
const PRUint8   kRangeTibetan   =  26;

const PRUint8   kRangeSpecificItemNum = 27;

//range/rangeSet grow to this place 27-29

const PRUint8   kRangeSetStart  =  30;   // range set definition starts from here
const PRUint8   kRangeSetLatin  =  30;
const PRUint8   kRangeSetCJK    =  31;
const PRUint8   kRangeSetEnd    =  31;   // range set definition ends here, this
                                         // and smaller ranges are used as bit
                                         // mask, don't increase this value.

// less frequently used range definition
const PRUint8   kRangeSurrogate            = 32;
const PRUint8   kRangePrivate              = 33;
const PRUint8   kRangeMisc                 = 34;
const PRUint8   kRangeUnassigned           = 35;
const PRUint8   kRangeSyriac               = 36;
const PRUint8   kRangeThaana               = 37;
const PRUint8   kRangeLao                  = 38;
const PRUint8   kRangeMyanmar              = 39;
const PRUint8   kRangeCherokee             = 40;
const PRUint8   kRangeOghamRunic           = 41;
const PRUint8   kRangeMongolian            = 42;
const PRUint8   kRangeMathOperators        = 43;
const PRUint8   kRangeMiscTechnical        = 44;
const PRUint8   kRangeControlOpticalEnclose = 45;
const PRUint8   kRangeBoxBlockGeometrics   = 46;
const PRUint8   kRangeMiscSymbols          = 47;
const PRUint8   kRangeDingbats             = 48;
const PRUint8   kRangeBraillePattern       = 49;
const PRUint8   kRangeYi                   = 50;
const PRUint8   kRangeCombiningDiacriticalMarks = 51;
const PRUint8   kRangeSpecials             = 52;

// aggregate ranges for non-BMP codepoints (u+2xxxx are all CJK)
const PRUint8   kRangeSMP                  = 53;  // u+1xxxx
const PRUint8   kRangeHigherPlanes         = 54;  // u+3xxxx and above

const PRUint8   kRangeTableBase   = 128;    //values over 127 are reserved for internal use only
const PRUint8   kRangeTertiaryTable  = 145; // leave room for 16 subtable 
                                            // indices (kRangeTableBase + 1 ..
                                            // kRangeTableBase + 16)



PRUint32 FindCharUnicodeRange(PRUint32 ch);
nsIAtom* LangGroupFromUnicodeRange(PRUint8 unicodeRange);
