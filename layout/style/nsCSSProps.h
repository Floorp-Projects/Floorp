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
 *   Mats Palmgren <matspal@gmail.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>, Collabora Ltd.
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

/*
 * methods for dealing with CSS properties and tables of the keyword
 * values they accept
 */

#ifndef nsCSSProps_h___
#define nsCSSProps_h___

#include "nsString.h"
#include "nsChangeHint.h"
#include "nsCSSProperty.h"
#include "nsStyleStruct.h"
#include "nsCSSKeywords.h"

// Flags for the kFlagsTable bitfield (flags_ in nsCSSPropList.h)

// A property that is a *-ltr-source or *-rtl-source property for one of
// the directional pseudo-shorthand properties.
#define CSS_PROPERTY_DIRECTIONAL_SOURCE           (1<<0)

#define CSS_PROPERTY_VALUE_LIST_USES_COMMAS       (1<<1) /* otherwise spaces */

#define CSS_PROPERTY_APPLIES_TO_FIRST_LETTER      (1<<2)
#define CSS_PROPERTY_APPLIES_TO_FIRST_LINE        (1<<3)
#define CSS_PROPERTY_APPLIES_TO_FIRST_LETTER_AND_FIRST_LINE \
  (CSS_PROPERTY_APPLIES_TO_FIRST_LETTER | CSS_PROPERTY_APPLIES_TO_FIRST_LINE)

// Note that 'background-color' is ignored differently from the other
// properties that have this set, but that's just special-cased.
#define CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED (1<<4)

// A property that needs to have image loads started when a URL value
// for the property is used for an element.  This is supported only
// for a few possible value formats: image directly in the value; list
// of images; and with CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0, image in slot
// 0 of an array, or list of such arrays.
#define CSS_PROPERTY_START_IMAGE_LOADS            (1<<5)

// Should be set only for properties with START_IMAGE_LOADS.  Indicates
// that the property has an array value with a URL/image value at index
// 0 in the array, rather than the URL/image being in the value or value
// list.
#define CSS_PROPERTY_IMAGE_IS_IN_ARRAY_0          (1<<6)

// This is a property for which the computed value should generally be
// reported as the computed value of a property of a different name.  In
// particular, the directional box properties (margin-left-value, etc.)
// should be reported as being margin-left, etc.  Call
// nsCSSProps::OtherNameFor to get the other property.
#define CSS_PROPERTY_REPORT_OTHER_NAME            (1<<7)

// This property allows calc() between lengths and percentages and
// stores such calc() expressions in its style structs (typically in an
// nsStyleCoord, although this is not the case for 'background-position'
// and 'background-size').
#define CSS_PROPERTY_STORES_CALC                  (1<<8)

// Define what mechanism the CSS parser uses for parsing the property.
// See CSSParserImpl::ParseProperty(nsCSSProperty).  Don't use 0 so that
// we can verify that every property sets one of the values.
//
// CSS_PROPERTY_PARSE_FUNCTION must be used for shorthand properties,
// since it's the only mechanism that allows appending values for
// separate properties.  Longhand properties that require custom parsing
// functions should prefer using CSS_PROPERTY_PARSE_VALUE (or
// CSS_PROPERTY_PARSE_VALUE_LIST) and
// CSS_PROPERTY_VALUE_PARSER_FUNCTION, though a number of existing
// longhand properties use CSS_PROPERTY_PARSE_FUNCTION instead.
#define CSS_PROPERTY_PARSE_PROPERTY_MASK          (7<<9)
#define CSS_PROPERTY_PARSE_INACCESSIBLE           (1<<9)
#define CSS_PROPERTY_PARSE_FUNCTION               (2<<9)
#define CSS_PROPERTY_PARSE_VALUE                  (3<<9)
#define CSS_PROPERTY_PARSE_VALUE_LIST             (4<<9)

// See CSSParserImpl::ParseSingleValueProperty and comment above
// CSS_PROPERTY_PARSE_FUNCTION (which is different).
#define CSS_PROPERTY_VALUE_PARSER_FUNCTION        (1<<12)
MOZ_STATIC_ASSERT((CSS_PROPERTY_PARSE_PROPERTY_MASK &
                   CSS_PROPERTY_VALUE_PARSER_FUNCTION) == 0,
                  "didn't leave enough room for the parse property constants");

#define CSS_PROPERTY_VALUE_RESTRICTION_MASK       (3<<13)
// The parser (in particular, CSSParserImpl::ParseSingleValueProperty)
// should enforce that the value of this property must be 0 or larger.
#define CSS_PROPERTY_VALUE_NONNEGATIVE            (1<<13)
// The parser (in particular, CSSParserImpl::ParseSingleValueProperty)
// should enforce that the value of this property must be 1 or larger.
#define CSS_PROPERTY_VALUE_AT_LEAST_ONE           (2<<13)

// NOTE: next free bit is (1<<15)

/**
 * Types of animatable values.
 */
enum nsStyleAnimType {
  // requires a custom implementation in
  // nsStyleAnimation::ExtractComputedValue
  eStyleAnimType_Custom,

  // nsStyleCoord with animatable values
  eStyleAnimType_Coord,

  // same as Coord, except for one side of an nsStyleSides
  // listed in the same order as the NS_STYLE_* constants
  eStyleAnimType_Sides_Top,
  eStyleAnimType_Sides_Right,
  eStyleAnimType_Sides_Bottom,
  eStyleAnimType_Sides_Left,

  // similar, but for the *pair* of coord members of an nsStyleCorners
  // for the relevant corner
  eStyleAnimType_Corner_TopLeft,
  eStyleAnimType_Corner_TopRight,
  eStyleAnimType_Corner_BottomRight,
  eStyleAnimType_Corner_BottomLeft,

  // nscoord values
  eStyleAnimType_nscoord,

  // enumerated values (stored in a PRUint8)
  // In order for a property to use this unit, _all_ of its enumerated values
  // must be listed in its keyword table, so that any enumerated value can be
  // converted into a string via a nsCSSValue of type eCSSUnit_Enumerated.
  eStyleAnimType_EnumU8,

  // float values
  eStyleAnimType_float,

  // nscolor values
  eStyleAnimType_Color,

  // nsStyleSVGPaint values
  eStyleAnimType_PaintServer,

  // nsRefPtr<nsCSSShadowArray> values
  eStyleAnimType_Shadow,

  // property not animatable
  eStyleAnimType_None
};

class nsCSSProps {
public:
  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Given a property string, return the enum value
  static nsCSSProperty LookupProperty(const nsAString& aProperty);
  static nsCSSProperty LookupProperty(const nsACString& aProperty);

  static inline bool IsShorthand(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                 "out of range");
    return (aProperty >= eCSSProperty_COUNT_no_shorthands);
  }

  // Same but for @font-face descriptors
  static nsCSSFontDesc LookupFontDesc(const nsAString& aProperty);
  static nsCSSFontDesc LookupFontDesc(const nsACString& aProperty);

  // Given a property enum, get the string value
  static const nsAFlatCString& GetStringValue(nsCSSProperty aProperty);
  static const nsAFlatCString& GetStringValue(nsCSSFontDesc aFontDesc);

  // Get the property to report the computed value of aProperty as being
  // the computed value of.  aProperty must have the
  // CSS_PROPERTY_REPORT_OTHER_NAME bit set.
  static nsCSSProperty OtherNameFor(nsCSSProperty aProperty);

  // Given a CSS Property and a Property Enum Value
  // Return back a const nsString& representation of the 
  // value. Return back nullstr if no value is found
  static const nsAFlatCString& LookupPropertyValue(nsCSSProperty aProperty, PRInt32 aValue);

  // Get a color name for a predefined color value like buttonhighlight or activeborder
  // Sets the aStr param to the name of the propertyID
  static bool GetColorName(PRInt32 aPropID, nsCString &aStr);

  // Find |aKeyword| in |aTable|, if found set |aValue| to its corresponding value.
  // If not found, return false and do not set |aValue|.
  static bool FindKeyword(nsCSSKeyword aKeyword, const PRInt32 aTable[], PRInt32& aValue);
  // Return the first keyword in |aTable| that has the corresponding value |aValue|.
  // Return |eCSSKeyword_UNKNOWN| if not found.
  static nsCSSKeyword ValueToKeywordEnum(PRInt32 aValue, const PRInt32 aTable[]);
  // Ditto but as a string, return "" when not found.
  static const nsAFlatCString& ValueToKeyword(PRInt32 aValue, const PRInt32 aTable[]);

  static const nsStyleStructID kSIDTable[eCSSProperty_COUNT_no_shorthands];
  static const PRInt32* const  kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];
  static const nsStyleAnimType kAnimTypeTable[eCSSProperty_COUNT_no_shorthands];
  static const ptrdiff_t
    kStyleStructOffsetTable[eCSSProperty_COUNT_no_shorthands];

private:
  static const PRUint32        kFlagsTable[eCSSProperty_COUNT];

public:
  static inline bool PropHasFlags(nsCSSProperty aProperty, PRUint32 aFlags)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    return (nsCSSProps::kFlagsTable[aProperty] & aFlags) == aFlags;
  }

  static inline PRUint32 PropertyParseType(nsCSSProperty aProperty)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_PARSE_PROPERTY_MASK;
  }

  static inline PRUint32 ValueRestrictions(nsCSSProperty aProperty)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_VALUE_RESTRICTION_MASK;
  }

private:
  // Lives in nsCSSParser.cpp for the macros it depends on.
  static const PRUint32 kParserVariantTable[eCSSProperty_COUNT_no_shorthands];

public:
  static inline PRUint32 ParserVariant(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(0 <= aProperty &&
                      aProperty < eCSSProperty_COUNT_no_shorthands,
                      "out of range");
    return nsCSSProps::kParserVariantTable[aProperty];
  }

private:
  // A table for shorthand properties.  The appropriate index is the
  // property ID minus eCSSProperty_COUNT_no_shorthands.
  static const nsCSSProperty *const
    kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands];

public:
  static inline
  const nsCSSProperty * SubpropertyEntryFor(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(eCSSProperty_COUNT_no_shorthands <= aProperty &&
                      aProperty < eCSSProperty_COUNT,
                      "out of range");
    return nsCSSProps::kSubpropertyTable[aProperty -
                                         eCSSProperty_COUNT_no_shorthands];
  }

  // Returns an eCSSProperty_UNKNOWN-terminated array of the shorthand
  // properties containing |aProperty|, sorted from those that contain
  // the most properties to those that contain the least.
  static const nsCSSProperty * ShorthandsContaining(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(gShorthandsContainingPool, "uninitialized");
    NS_ABORT_IF_FALSE(0 <= aProperty &&
                      aProperty < eCSSProperty_COUNT_no_shorthands,
                      "out of range");
    return gShorthandsContainingTable[aProperty];
  }
private:
  // gShorthandsContainingTable is an array of the return values for
  // ShorthandsContaining (arrays of nsCSSProperty terminated by
  // eCSSProperty_UNKNOWN) pointing into memory in
  // gShorthandsContainingPool (which contains all of those arrays in a
  // single allocation, and is the one pointer that should be |free|d).
  static nsCSSProperty *gShorthandsContainingTable[eCSSProperty_COUNT_no_shorthands];
  static nsCSSProperty* gShorthandsContainingPool;
  static bool BuildShorthandsContainingTable();

private:
  static const size_t gPropertyCountInStruct[nsStyleStructID_Length];
  static const size_t gPropertyIndexInStruct[eCSSProperty_COUNT_no_shorthands];
public:
  /**
   * Return the number of properties that must be cascaded when
   * nsRuleNode builds the nsStyle* for aSID.
   */
  static size_t PropertyCountInStruct(nsStyleStructID aSID) {
    NS_ABORT_IF_FALSE(0 <= aSID && aSID < nsStyleStructID_Length,
                      "out of range");
    return gPropertyCountInStruct[aSID];
  }
  /**
   * Return an index for aProperty that is unique within its SID and in
   * the range 0 <= index < PropertyCountInStruct(aSID).
   */
  static size_t PropertyIndexInStruct(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(0 <= aProperty &&
                         aProperty < eCSSProperty_COUNT_no_shorthands,
                      "out of range");
    return gPropertyIndexInStruct[aProperty];
  }

public:

#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(iter_, prop_)                    \
  for (const nsCSSProperty* iter_ = nsCSSProps::SubpropertyEntryFor(prop_);   \
       *iter_ != eCSSProperty_UNKNOWN; ++iter_)

  // Keyword/Enum value tables
  static const PRInt32 kAnimationDirectionKTable[];
  static const PRInt32 kAnimationFillModeKTable[];
  static const PRInt32 kAnimationIterationCountKTable[];
  static const PRInt32 kAnimationPlayStateKTable[];
  static const PRInt32 kAnimationTimingFunctionKTable[];
  static const PRInt32 kAppearanceKTable[];
  static const PRInt32 kAzimuthKTable[];
  static const PRInt32 kBackfaceVisibilityKTable[];
  static const PRInt32 kTransformStyleKTable[];
  static const PRInt32 kBackgroundAttachmentKTable[];
  static const PRInt32 kBackgroundInlinePolicyKTable[];
  static const PRInt32 kBackgroundOriginKTable[];
  static const PRInt32 kBackgroundPositionKTable[];
  static const PRInt32 kBackgroundRepeatKTable[];
  static const PRInt32 kBackgroundRepeatPartKTable[];
  static const PRInt32 kBackgroundSizeKTable[];
  static const PRInt32 kBorderCollapseKTable[];
  static const PRInt32 kBorderColorKTable[];
  static const PRInt32 kBorderImageRepeatKTable[];
  static const PRInt32 kBorderImageSliceKTable[];
  static const PRInt32 kBorderStyleKTable[];
  static const PRInt32 kBorderWidthKTable[];
  static const PRInt32 kBoxAlignKTable[];
  static const PRInt32 kBoxDirectionKTable[];
  static const PRInt32 kBoxOrientKTable[];
  static const PRInt32 kBoxPackKTable[];
  static const PRInt32 kDominantBaselineKTable[];
  static const PRInt32 kFillRuleKTable[];
  static const PRInt32 kImageRenderingKTable[];
  static const PRInt32 kShapeRenderingKTable[];
  static const PRInt32 kStrokeLinecapKTable[];
  static const PRInt32 kStrokeLinejoinKTable[];
  static const PRInt32 kTextAnchorKTable[];
  static const PRInt32 kTextRenderingKTable[];
  static const PRInt32 kColorInterpolationKTable[];
  static const PRInt32 kColumnFillKTable[];
  static const PRInt32 kBoxPropSourceKTable[];
  static const PRInt32 kBoxShadowTypeKTable[];
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
  static const PRInt32 kIMEModeKTable[];
  static const PRInt32 kLineHeightKTable[];
  static const PRInt32 kListStylePositionKTable[];
  static const PRInt32 kListStyleKTable[];
  static const PRInt32 kOrientKTable[];
  static const PRInt32 kOutlineStyleKTable[];
  static const PRInt32 kOutlineColorKTable[];
  static const PRInt32 kOverflowKTable[];
  static const PRInt32 kOverflowSubKTable[];
  static const PRInt32 kPageBreakKTable[];
  static const PRInt32 kPageBreakInsideKTable[];
  static const PRInt32 kPageMarksKTable[];
  static const PRInt32 kPageSizeKTable[];
  static const PRInt32 kPitchKTable[];
  static const PRInt32 kPointerEventsKTable[];
  static const PRInt32 kPositionKTable[];
  static const PRInt32 kRadialGradientShapeKTable[];
  static const PRInt32 kRadialGradientSizeKTable[];
  static const PRInt32 kResizeKTable[];
  static const PRInt32 kSpeakKTable[];
  static const PRInt32 kSpeakHeaderKTable[];
  static const PRInt32 kSpeakNumeralKTable[];
  static const PRInt32 kSpeakPunctuationKTable[];
  static const PRInt32 kSpeechRateKTable[];
  static const PRInt32 kStackSizingKTable[];
  static const PRInt32 kTableLayoutKTable[];
  static const PRInt32 kTextAlignKTable[];
  static const PRInt32 kTextAlignLastKTable[];
  static const PRInt32 kTextBlinkKTable[];
  static const PRInt32 kTextDecorationLineKTable[];
  static const PRInt32 kTextDecorationStyleKTable[];
  static const PRInt32 kTextOverflowKTable[];
  static const PRInt32 kTextTransformKTable[];
  static const PRInt32 kTransitionTimingFunctionKTable[];
  static const PRInt32 kUnicodeBidiKTable[];
  static const PRInt32 kUserFocusKTable[];
  static const PRInt32 kUserInputKTable[];
  static const PRInt32 kUserModifyKTable[];
  static const PRInt32 kUserSelectKTable[];
  static const PRInt32 kVerticalAlignKTable[];
  static const PRInt32 kVisibilityKTable[];
  static const PRInt32 kVolumeKTable[];
  static const PRInt32 kWhitespaceKTable[];
  static const PRInt32 kWidthKTable[]; // also min-width, max-width
  static const PRInt32 kWindowShadowKTable[];
  static const PRInt32 kWordwrapKTable[];
  static const PRInt32 kHyphensKTable[];
};

#endif /* nsCSSProps_h___ */
