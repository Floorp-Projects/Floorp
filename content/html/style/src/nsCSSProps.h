/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsCSSProps_h___
#define nsCSSProps_h___

#include "nslayout.h"
#include "nsCSSPropIDs.h"

class NS_LAYOUT nsCSSProps {
public:
  // Given a null terminated string of 7 bit characters, return the
  // tag id (see nsCSSPropIDs.h) for the tag or -1 if the tag is not
  // known. The lookup function uses a perfect hash.
  static PRInt32 LookupName(const char* str);

    
  // Given a CSS Property ID and an Property Value Index
  // Return back a const char* representation of the 
  // value. Return back nsnull if no value is found
  static const char* LookupProperty(PRInt32 aProp, PRInt32 aIndex);

  struct NameTableEntry {
    const char* name;
    PRInt32 id;
  };

   // A table whose index is the tag id (from nsCSSPropIDs)
  static const NameTableEntry kNameTable[];

  static const PRInt32  kHintTable[];

  // Keyword/Enum value tables
  static const PRInt32 kAzimuthKTable[];
  static const PRInt32 kBackgroundAttachmentKTable[];
  static const PRInt32 kBackgroundColorKTable[];
  static const PRInt32 kBackgroundRepeatKTable[];
  static const PRInt32 kBorderCollapseKTable[];
  static const PRInt32 kBorderColorKTable[];
  static const PRInt32 kBorderStyleKTable[];
  static const PRInt32 kBorderWidthKTable[];
  static const PRInt32 kCaptionSideKTable[];
  static const PRInt32 kClearKTable[];
  static const PRInt32 kContentKTable[];
  static const PRInt32 kCursorKTable[];
  static const PRInt32 kDirectionKTable[];
  static const PRInt32 kDisplayKTable[];
  static const PRInt32 kElevationKTable[];
  static const PRInt32 kEmptyCellsKTable[];
  static const PRInt32 kFloatKTable[];
  static const PRInt32 kFontKTable[];
  static const PRInt32 kFontSizeKTable[];
  static const PRInt32 kFontStretchKTable[];
  static const PRInt32 kFontStyleKTable[];
  static const PRInt32 kFontVariantKTable[];
  static const PRInt32 kFontWeightKTable[];
  static const PRInt32 kListStylePositionKTable[];
  static const PRInt32 kListStyleKTable[];
  static const PRInt32 kOutlineColorKTable[];
  static const PRInt32 kOverflowKTable[];
  static const PRInt32 kPageBreakKTable[];
  static const PRInt32 kPageBreakInsideKTable[];
  static const PRInt32 kPageMarksKTable[];
  static const PRInt32 kPageSizeKTable[];
  static const PRInt32 kPitchKTable[];
  static const PRInt32 kPlayDuringKTable[];
  static const PRInt32 kPositionKTable[];
  static const PRInt32 kSpeakKTable[];
  static const PRInt32 kSpeakHeaderKTable[];
  static const PRInt32 kSpeakNumeralKTable[];
  static const PRInt32 kSpeakPunctuationKTable[];
  static const PRInt32 kSpeechRateKTable[];
  static const PRInt32 kTableLayoutKTable[];
  static const PRInt32 kTextAlignKTable[];
  static const PRInt32 kTextDecorationKTable[];
  static const PRInt32 kTextTransformKTable[];
  static const PRInt32 kUnicodeBidiKTable[];
  static const PRInt32 kVerticalAlignKTable[];
  static const PRInt32 kVisibilityKTable[];
  static const PRInt32 kVolumeKTable[];
  static const PRInt32 kWhitespaceKTable[];
};

#endif /* nsCSSProps_h___ */
