/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsCSSProps_h___
#define nsCSSProps_h___

#include "nsString.h"
#include "nsChangeHint.h"
#include "nsCSSProperty.h"
#include "nsStyleStruct.h"

class nsCSSProps {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a property string, return the enum value
  static nsCSSProperty LookupProperty(const nsAString& aProperty);
  static nsCSSProperty LookupProperty(const nsACString& aProperty);

  static inline PRBool IsShorthand(nsCSSProperty aProperty) {
    NS_ASSERTION(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                 "out of range");
    return (aProperty >= eCSSProperty_COUNT_no_shorthands);
  }

  // Given a property enum, get the string value
  static const nsAFlatCString& GetStringValue(nsCSSProperty aProperty);

  // Given a CSS Property and a Property Enum Value
  // Return back a const nsString& representation of the 
  // value. Return back nullstr if no value is found
  static const nsAFlatCString& LookupPropertyValue(nsCSSProperty aProperty, PRInt32 aValue);

  // Get a color name for a predefined color value like buttonhighlight or activeborder
  // Sets the aStr param to the name of the propertyID
  static PRBool GetColorName(PRInt32 aPropID, nsCString &aStr);

  static PRInt32 SearchKeywordTableInt(PRInt32 aValue, const PRInt32 aTable[]);
  static const nsAFlatCString& SearchKeywordTable(PRInt32 aValue, const PRInt32 aTable[]);

  static const nsCSSType       kTypeTable[eCSSProperty_COUNT_no_shorthands];
  static const nsStyleStructID kSIDTable[eCSSProperty_COUNT_no_shorthands];
  static const PRInt32* const  kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];

  // A table for shorthand properties.  The appropriate index is the
  // property ID minus eCSSProperty_COUNT_no_shorthands.
private:
  static const nsCSSProperty *const
    kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands];

public:
  static inline
  const nsCSSProperty *const SubpropertyEntryFor(nsCSSProperty aProperty) {
    NS_ASSERTION(eCSSProperty_COUNT_no_shorthands <= aProperty &&
                 aProperty < eCSSProperty_COUNT,
                 "out of range");
    return nsCSSProps::kSubpropertyTable[aProperty -
                                         eCSSProperty_COUNT_no_shorthands];
  }

#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(iter_, prop_)                    \
  for (const nsCSSProperty* iter_ = nsCSSProps::SubpropertyEntryFor(prop_);   \
       *iter_ != eCSSProperty_UNKNOWN; ++iter_)

  // Keyword/Enum value tables
  static const PRInt32 kAppearanceKTable[];
  static const PRInt32 kAzimuthKTable[];
  static const PRInt32 kBackgroundAttachmentKTable[];
  static const PRInt32 kBackgroundClipKTable[];
  static const PRInt32 kBackgroundColorKTable[];
  static const PRInt32 kBackgroundInlinePolicyKTable[];
  static const PRInt32 kBackgroundOriginKTable[];
  static const PRInt32 kBackgroundRepeatKTable[];
  static const PRInt32 kBackgroundXPositionKTable[];
  static const PRInt32 kBackgroundYPositionKTable[];
  static const PRInt32 kBorderCollapseKTable[];
  static const PRInt32 kBorderColorKTable[];
  static const PRInt32 kBorderStyleKTable[];
  static const PRInt32 kBorderWidthKTable[];
  static const PRInt32 kBoxAlignKTable[];
  static const PRInt32 kBoxDirectionKTable[];
  static const PRInt32 kBoxOrientKTable[];
  static const PRInt32 kBoxPackKTable[];
#ifdef MOZ_SVG
  static const PRInt32 kDominantBaselineKTable[];
  static const PRInt32 kFillRuleKTable[];
  static const PRInt32 kPointerEventsKTable[];
  static const PRInt32 kShapeRenderingKTable[];
  static const PRInt32 kStrokeLinecapKTable[];
  static const PRInt32 kStrokeLinejoinKTable[];
  static const PRInt32 kTextAnchorKTable[];
  static const PRInt32 kTextRenderingKTable[];
#endif
  static const PRInt32 kBoxPropSourceKTable[];
  static const PRInt32 kBoxSizingKTable[];
  static const PRInt32 kCaptionSideKTable[];
  static const PRInt32 kClearKTable[];
  static const PRInt32 kColorKTable[];
  static const PRInt32 kContentKTable[];
  static const PRInt32 kCursorKTable[];
  static const PRInt32 kDirectionKTable[];
  static const PRInt32 kDisplayKTable[];
  static const PRInt32 kElevationKTable[];
  static const PRInt32 kEmptyCellsKTable[];
  static const PRInt32 kFloatKTable[];
  static const PRInt32 kFloatEdgeKTable[];
  static const PRInt32 kFontKTable[];
  static const PRInt32 kFontSizeKTable[];
  static const PRInt32 kFontStretchKTable[];
  static const PRInt32 kFontStyleKTable[];
  static const PRInt32 kFontVariantKTable[];
  static const PRInt32 kFontWeightKTable[];
  static const PRInt32 kKeyEquivalentKTable[];
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
  static const PRInt32 kUserFocusKTable[];
  static const PRInt32 kUserInputKTable[];
  static const PRInt32 kUserModifyKTable[];
  static const PRInt32 kUserSelectKTable[];
  static const PRInt32 kVerticalAlignKTable[];
  static const PRInt32 kVisibilityKTable[];
  static const PRInt32 kVolumeKTable[];
  static const PRInt32 kWhitespaceKTable[];
};

#endif /* nsCSSProps_h___ */
