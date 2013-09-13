/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

// Flags for ParseVariant method
#define VARIANT_KEYWORD         0x000001  // K
#define VARIANT_LENGTH          0x000002  // L
#define VARIANT_PERCENT         0x000004  // P
#define VARIANT_COLOR           0x000008  // C eCSSUnit_Color, eCSSUnit_Ident (e.g.  "red")
#define VARIANT_URL             0x000010  // U
#define VARIANT_NUMBER          0x000020  // N
#define VARIANT_INTEGER         0x000040  // I
#define VARIANT_ANGLE           0x000080  // G
#define VARIANT_FREQUENCY       0x000100  // F
#define VARIANT_TIME            0x000200  // T
#define VARIANT_STRING          0x000400  // S
#define VARIANT_COUNTER         0x000800  //
#define VARIANT_ATTR            0x001000  //
#define VARIANT_IDENTIFIER      0x002000  // D
#define VARIANT_IDENTIFIER_NO_INHERIT 0x004000 // like above, but excluding
// 'inherit' and 'initial'
#define VARIANT_AUTO            0x010000  // A
#define VARIANT_INHERIT         0x020000  // H eCSSUnit_Initial, eCSSUnit_Inherit
#define VARIANT_NONE            0x040000  // O
#define VARIANT_NORMAL          0x080000  // M
#define VARIANT_SYSFONT         0x100000  // eCSSUnit_System_Font
#define VARIANT_GRADIENT        0x200000  // eCSSUnit_Gradient
#define VARIANT_TIMING_FUNCTION 0x400000  // cubic-bezier() and steps()
#define VARIANT_ALL             0x800000  //
#define VARIANT_IMAGE_RECT    0x01000000  // eCSSUnit_Function
// This is an extra bit that says that a VARIANT_ANGLE allows unitless zero:
#define VARIANT_ZERO_ANGLE    0x02000000  // unitless zero for angles
#define VARIANT_CALC          0x04000000  // eCSSUnit_Calc
#define VARIANT_ELEMENT       0x08000000  // eCSSUnit_Element
#define VARIANT_POSITIVE_DIMENSION 0x10000000 // Only lengths greater than 0.0
#define VARIANT_NONNEGATIVE_DIMENSION 0x20000000 // Only lengths greater than or equal to 0.0

// Common combinations of variants
#define VARIANT_AL   (VARIANT_AUTO | VARIANT_LENGTH)
#define VARIANT_LP   (VARIANT_LENGTH | VARIANT_PERCENT)
#define VARIANT_LN   (VARIANT_LENGTH | VARIANT_NUMBER)
#define VARIANT_AH   (VARIANT_AUTO | VARIANT_INHERIT)
#define VARIANT_AHLP (VARIANT_AH | VARIANT_LP)
#define VARIANT_AHI  (VARIANT_AH | VARIANT_INTEGER)
#define VARIANT_AHK  (VARIANT_AH | VARIANT_KEYWORD)
#define VARIANT_AHKLP (VARIANT_AHLP | VARIANT_KEYWORD)
#define VARIANT_AHL  (VARIANT_AH | VARIANT_LENGTH)
#define VARIANT_AHKL (VARIANT_AHK | VARIANT_LENGTH)
#define VARIANT_HK   (VARIANT_INHERIT | VARIANT_KEYWORD)
#define VARIANT_HKF  (VARIANT_HK | VARIANT_FREQUENCY)
#define VARIANT_HKI  (VARIANT_HK | VARIANT_INTEGER)
#define VARIANT_HKL  (VARIANT_HK | VARIANT_LENGTH)
#define VARIANT_HKLP (VARIANT_HK | VARIANT_LP)
#define VARIANT_HKLPO (VARIANT_HKLP | VARIANT_NONE)
#define VARIANT_HL   (VARIANT_INHERIT | VARIANT_LENGTH)
#define VARIANT_HI   (VARIANT_INHERIT | VARIANT_INTEGER)
#define VARIANT_HLP  (VARIANT_HL | VARIANT_PERCENT)
#define VARIANT_HLPN (VARIANT_HLP | VARIANT_NUMBER)
#define VARIANT_HLPO (VARIANT_HLP | VARIANT_NONE)
#define VARIANT_HTP  (VARIANT_INHERIT | VARIANT_TIME | VARIANT_PERCENT)
#define VARIANT_HMK  (VARIANT_HK | VARIANT_NORMAL)
#define VARIANT_HC   (VARIANT_INHERIT | VARIANT_COLOR)
#define VARIANT_HCK  (VARIANT_HK | VARIANT_COLOR)
#define VARIANT_HUK  (VARIANT_HK | VARIANT_URL)
#define VARIANT_HUO  (VARIANT_INHERIT | VARIANT_URL | VARIANT_NONE)
#define VARIANT_AHUO (VARIANT_AUTO | VARIANT_HUO)
#define VARIANT_HPN  (VARIANT_INHERIT | VARIANT_PERCENT | VARIANT_NUMBER)
#define VARIANT_PN   (VARIANT_PERCENT | VARIANT_NUMBER)
#define VARIANT_ALPN (VARIANT_AL | VARIANT_PN)
#define VARIANT_HN   (VARIANT_INHERIT | VARIANT_NUMBER)
#define VARIANT_HON  (VARIANT_HN | VARIANT_NONE)
#define VARIANT_HOS  (VARIANT_INHERIT | VARIANT_NONE | VARIANT_STRING)
#define VARIANT_LPN  (VARIANT_LP | VARIANT_NUMBER)
#define VARIANT_UK   (VARIANT_URL | VARIANT_KEYWORD)
#define VARIANT_UO   (VARIANT_URL | VARIANT_NONE)
#define VARIANT_ANGLE_OR_ZERO (VARIANT_ANGLE | VARIANT_ZERO_ANGLE)
#define VARIANT_LCALC  (VARIANT_LENGTH | VARIANT_CALC)
#define VARIANT_LPCALC (VARIANT_LCALC | VARIANT_PERCENT)
#define VARIANT_LNCALC (VARIANT_LCALC | VARIANT_NUMBER)
#define VARIANT_LPNCALC (VARIANT_LNCALC | VARIANT_PERCENT)
#define VARIANT_IMAGE   (VARIANT_URL | VARIANT_NONE | VARIANT_GRADIENT | \
                        VARIANT_IMAGE_RECT | VARIANT_ELEMENT)

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
static_assert((CSS_PROPERTY_PARSE_PROPERTY_MASK &
               CSS_PROPERTY_VALUE_PARSER_FUNCTION) == 0,
              "didn't leave enough room for the parse property constants");

#define CSS_PROPERTY_VALUE_RESTRICTION_MASK       (3<<13)
// The parser (in particular, CSSParserImpl::ParseSingleValueProperty)
// should enforce that the value of this property must be 0 or larger.
#define CSS_PROPERTY_VALUE_NONNEGATIVE            (1<<13)
// The parser (in particular, CSSParserImpl::ParseSingleValueProperty)
// should enforce that the value of this property must be 1 or larger.
#define CSS_PROPERTY_VALUE_AT_LEAST_ONE           (2<<13)

// Does this property suppor the hashless hex color quirk in quirks mode?
#define CSS_PROPERTY_HASHLESS_COLOR_QUIRK         (1<<15)

// Does this property suppor the unitless length quirk in quirks mode?
#define CSS_PROPERTY_UNITLESS_LENGTH_QUIRK        (1<<16)

// Is this property (which must be a shorthand) really an alias?
#define CSS_PROPERTY_IS_ALIAS                     (1<<17)

// Does the property apply to ::-moz-placeholder?
#define CSS_PROPERTY_APPLIES_TO_PLACEHOLDER       (1<<18)

// This property is allowed in an @page rule.
#define CSS_PROPERTY_APPLIES_TO_PAGE_RULE         (1<<19)

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

  // enumerated values (stored in a uint8_t)
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
  enum EnabledState {
    eEnabled,
    eAny
  };
  static nsCSSProperty LookupProperty(const nsAString& aProperty,
                                      EnabledState aEnabled);
  static nsCSSProperty LookupProperty(const nsACString& aProperty,
                                      EnabledState aEnabled);

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
  static const nsAFlatCString& LookupPropertyValue(nsCSSProperty aProperty, int32_t aValue);

  // Get a color name for a predefined color value like buttonhighlight or activeborder
  // Sets the aStr param to the name of the propertyID
  static bool GetColorName(int32_t aPropID, nsCString &aStr);

  // Returns the index of |aKeyword| in |aTable|, if it exists there;
  // otherwise, returns -1.
  // NOTE: Generally, clients should call FindKeyword() instead of this method.
  static int32_t FindIndexOfKeyword(nsCSSKeyword aKeyword, const int32_t aTable[]);

  // Find |aKeyword| in |aTable|, if found set |aValue| to its corresponding value.
  // If not found, return false and do not set |aValue|.
  static bool FindKeyword(nsCSSKeyword aKeyword, const int32_t aTable[], int32_t& aValue);
  // Return the first keyword in |aTable| that has the corresponding value |aValue|.
  // Return |eCSSKeyword_UNKNOWN| if not found.
  static nsCSSKeyword ValueToKeywordEnum(int32_t aValue, const int32_t aTable[]);
  // Ditto but as a string, return "" when not found.
  static const nsAFlatCString& ValueToKeyword(int32_t aValue, const int32_t aTable[]);

  static const nsStyleStructID kSIDTable[eCSSProperty_COUNT_no_shorthands];
  static const int32_t* const  kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];
  static const nsStyleAnimType kAnimTypeTable[eCSSProperty_COUNT_no_shorthands];
  static const ptrdiff_t
    kStyleStructOffsetTable[eCSSProperty_COUNT_no_shorthands];

private:
  static const uint32_t        kFlagsTable[eCSSProperty_COUNT];

public:
  static inline bool PropHasFlags(nsCSSProperty aProperty, uint32_t aFlags)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    MOZ_ASSERT(!(aFlags & CSS_PROPERTY_PARSE_PROPERTY_MASK),
               "The CSS_PROPERTY_PARSE_* values are not bitflags; don't pass "
               "them to PropHasFlags.  You probably want PropertyParseType "
               "instead.");
    return (nsCSSProps::kFlagsTable[aProperty] & aFlags) == aFlags;
  }

  static inline uint32_t PropertyParseType(nsCSSProperty aProperty)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_PARSE_PROPERTY_MASK;
  }

  static inline uint32_t ValueRestrictions(nsCSSProperty aProperty)
  {
    NS_ABORT_IF_FALSE(0 <= aProperty && aProperty < eCSSProperty_COUNT,
                      "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_VALUE_RESTRICTION_MASK;
  }

private:
  // Lives in nsCSSParser.cpp for the macros it depends on.
  static const uint32_t kParserVariantTable[eCSSProperty_COUNT_no_shorthands];

public:
  static inline uint32_t ParserVariant(nsCSSProperty aProperty) {
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

private:
  static bool gPropertyEnabled[eCSSProperty_COUNT_with_aliases];

public:

  static bool IsEnabled(nsCSSProperty aProperty) {
    NS_ABORT_IF_FALSE(0 <= aProperty &&
                      aProperty < eCSSProperty_COUNT_with_aliases,
                      "out of range");
    return gPropertyEnabled[aProperty];
  }

public:

#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(iter_, prop_)                    \
  for (const nsCSSProperty* iter_ = nsCSSProps::SubpropertyEntryFor(prop_);   \
       *iter_ != eCSSProperty_UNKNOWN; ++iter_) \
    if (nsCSSProps::IsEnabled(*iter_))

  // Keyword/Enum value tables
  static const int32_t kAnimationDirectionKTable[];
  static const int32_t kAnimationFillModeKTable[];
  static const int32_t kAnimationIterationCountKTable[];
  static const int32_t kAnimationPlayStateKTable[];
  static const int32_t kAnimationTimingFunctionKTable[];
  static const int32_t kAppearanceKTable[];
  static const int32_t kAzimuthKTable[];
  static const int32_t kBackfaceVisibilityKTable[];
  static const int32_t kTransformStyleKTable[];
  static const int32_t kBackgroundAttachmentKTable[];
  static const int32_t kBackgroundInlinePolicyKTable[];
  static const int32_t kBackgroundOriginKTable[];
  static const int32_t kBackgroundPositionKTable[];
  static const int32_t kBackgroundRepeatKTable[];
  static const int32_t kBackgroundRepeatPartKTable[];
  static const int32_t kBackgroundSizeKTable[];
  static const int32_t kBlendModeKTable[];
  static const int32_t kBorderCollapseKTable[];
  static const int32_t kBorderColorKTable[];
  static const int32_t kBorderImageRepeatKTable[];
  static const int32_t kBorderImageSliceKTable[];
  static const int32_t kBorderStyleKTable[];
  static const int32_t kBorderWidthKTable[];
  static const int32_t kBoxAlignKTable[];
  static const int32_t kBoxDirectionKTable[];
  static const int32_t kBoxOrientKTable[];
  static const int32_t kBoxPackKTable[];
  static const int32_t kDominantBaselineKTable[];
  static const int32_t kFillRuleKTable[];
  static const int32_t kFilterFunctionKTable[];
  static const int32_t kImageRenderingKTable[];
  static const int32_t kShapeRenderingKTable[];
  static const int32_t kStrokeLinecapKTable[];
  static const int32_t kStrokeLinejoinKTable[];
  static const int32_t kStrokeObjectValueKTable[];
  static const int32_t kVectorEffectKTable[];
  static const int32_t kTextAnchorKTable[];
  static const int32_t kTextRenderingKTable[];
  static const int32_t kColorInterpolationKTable[];
  static const int32_t kColumnFillKTable[];
  static const int32_t kBoxPropSourceKTable[];
  static const int32_t kBoxShadowTypeKTable[];
  static const int32_t kBoxSizingKTable[];
  static const int32_t kCaptionSideKTable[];
  static const int32_t kClearKTable[];
  static const int32_t kColorKTable[];
  static const int32_t kContentKTable[];
  static const int32_t kCursorKTable[];
  static const int32_t kDirectionKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.flexbox.enabled" changes:
  static int32_t kDisplayKTable[];
  static const int32_t kElevationKTable[];
  static const int32_t kEmptyCellsKTable[];
  static const int32_t kAlignItemsKTable[];
  static const int32_t kAlignSelfKTable[];
  static const int32_t kFlexDirectionKTable[];
  static const int32_t kJustifyContentKTable[];
  static const int32_t kFloatKTable[];
  static const int32_t kFloatEdgeKTable[];
  static const int32_t kFontKTable[];
  static const int32_t kFontKerningKTable[];
  static const int32_t kFontSizeKTable[];
  static const int32_t kFontSmoothingKTable[];
  static const int32_t kFontStretchKTable[];
  static const int32_t kFontStyleKTable[];
  static const int32_t kFontSynthesisKTable[];
  static const int32_t kFontVariantKTable[];
  static const int32_t kFontVariantAlternatesKTable[];
  static const int32_t kFontVariantAlternatesFuncsKTable[];
  static const int32_t kFontVariantCapsKTable[];
  static const int32_t kFontVariantEastAsianKTable[];
  static const int32_t kFontVariantLigaturesKTable[];
  static const int32_t kFontVariantNumericKTable[];
  static const int32_t kFontVariantPositionKTable[];
  static const int32_t kFontWeightKTable[];
  static const int32_t kImageOrientationKTable[];
  static const int32_t kImageOrientationFlipKTable[];
  static const int32_t kIMEModeKTable[];
  static const int32_t kLineHeightKTable[];
  static const int32_t kListStylePositionKTable[];
  static const int32_t kListStyleKTable[];
  static const int32_t kMaskTypeKTable[];
  static const int32_t kObjectOpacityKTable[];
  static const int32_t kObjectPatternKTable[];
  static const int32_t kOrientKTable[];
  static const int32_t kOutlineStyleKTable[];
  static const int32_t kOutlineColorKTable[];
  static const int32_t kOverflowKTable[];
  static const int32_t kOverflowSubKTable[];
  static const int32_t kPageBreakKTable[];
  static const int32_t kPageBreakInsideKTable[];
  static const int32_t kPageMarksKTable[];
  static const int32_t kPageSizeKTable[];
  static const int32_t kPitchKTable[];
  static const int32_t kPointerEventsKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.sticky.enabled" changes:
  static int32_t kPositionKTable[];
  static const int32_t kRadialGradientShapeKTable[];
  static const int32_t kRadialGradientSizeKTable[];
  static const int32_t kRadialGradientLegacySizeKTable[];
  static const int32_t kResizeKTable[];
  static const int32_t kSpeakKTable[];
  static const int32_t kSpeakHeaderKTable[];
  static const int32_t kSpeakNumeralKTable[];
  static const int32_t kSpeakPunctuationKTable[];
  static const int32_t kSpeechRateKTable[];
  static const int32_t kStackSizingKTable[];
  static const int32_t kTableLayoutKTable[];
  static const int32_t kTextAlignKTable[];
  static const int32_t kTextAlignLastKTable[];
  static const int32_t kTextCombineHorizontalKTable[];
  static const int32_t kTextDecorationLineKTable[];
  static const int32_t kTextDecorationStyleKTable[];
  static const int32_t kTextOrientationKTable[];
  static const int32_t kTextOverflowKTable[];
  static const int32_t kTextTransformKTable[];
  static const int32_t kTransitionTimingFunctionKTable[];
  static const int32_t kUnicodeBidiKTable[];
  static const int32_t kUserFocusKTable[];
  static const int32_t kUserInputKTable[];
  static const int32_t kUserModifyKTable[];
  static const int32_t kUserSelectKTable[];
  static const int32_t kVerticalAlignKTable[];
  static const int32_t kVisibilityKTable[];
  static const int32_t kVolumeKTable[];
  static const int32_t kWhitespaceKTable[];
  static const int32_t kWidthKTable[]; // also min-width, max-width
  static const int32_t kWindowShadowKTable[];
  static const int32_t kWordBreakKTable[];
  static const int32_t kWordWrapKTable[];
  static const int32_t kWritingModeKTable[];
  static const int32_t kHyphensKTable[];
};

#endif /* nsCSSProps_h___ */
