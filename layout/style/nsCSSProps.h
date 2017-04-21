/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * methods for dealing with CSS properties and tables of the keyword
 * values they accept
 */

#ifndef nsCSSProps_h___
#define nsCSSProps_h___

#include <limits>
#include <type_traits>
#include "nsStringFwd.h"
#include "nsCSSPropertyID.h"
#include "nsStyleStructFwd.h"
#include "nsCSSKeywords.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/UseCounter.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/Preferences.h"
#include "nsXULAppAPI.h"

// Length of the "--" prefix on custom names (such as custom property names,
// and, in the future, custom media query names).
#define CSS_CUSTOM_NAME_PREFIX_LENGTH 2

// Flags for ParseVariant method
#define VARIANT_KEYWORD         0x000001  // K
#define VARIANT_LENGTH          0x000002  // L
#define VARIANT_PERCENT         0x000004  // P
#define VARIANT_COLOR           0x000008  // C eCSSUnit_*Color, eCSSUnit_Ident (e.g.  "red")
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
#define VARIANT_INHERIT         0x020000  // H eCSSUnit_Initial, eCSSUnit_Inherit, eCSSUnit_Unset
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
#define VARIANT_NONNEGATIVE_DIMENSION 0x10000000 // Only lengths greater than or equal to 0.0
// Keyword used iff gfx.font_rendering.opentype_svg.enabled is true:
#define VARIANT_OPENTYPE_SVG_KEYWORD 0x20000000
#define VARIANT_ABSOLUTE_DIMENSION 0x40000000 // B Only lengths with absolute length unit

// Variants that can consume more than one token
#define VARIANT_MULTIPLE_TOKENS \
  (VARIANT_COLOR |            /* rgb(...), hsl(...), etc. */                  \
   VARIANT_COUNTER |          /* counter(...), counters(...) */               \
   VARIANT_ATTR |             /* attr(...) */                                 \
   VARIANT_GRADIENT |         /* linear-gradient(...), etc. */                \
   VARIANT_TIMING_FUNCTION |  /* cubic-bezier(...), steps(...) */             \
   VARIANT_IMAGE_RECT |       /* -moz-image-rect(...) */                      \
   VARIANT_CALC |             /* calc(...) */                                 \
   VARIANT_ELEMENT)           /* -moz-element(...) */

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
#define VARIANT_LB   (VARIANT_LENGTH | VARIANT_ABSOLUTE_DIMENSION)
#define VARIANT_LBCALC (VARIANT_LB | VARIANT_CALC)
#define VARIANT_LCALC  (VARIANT_LENGTH | VARIANT_CALC)
#define VARIANT_LPCALC (VARIANT_LCALC | VARIANT_PERCENT)
#define VARIANT_LNCALC (VARIANT_LCALC | VARIANT_NUMBER)
#define VARIANT_LPNCALC (VARIANT_LNCALC | VARIANT_PERCENT)
#define VARIANT_IMAGE   (VARIANT_URL | VARIANT_NONE | VARIANT_GRADIENT | \
                        VARIANT_IMAGE_RECT | VARIANT_ELEMENT)

// Flags for the kFlagsTable bitfield (flags_ in nsCSSPropList.h)

#define CSS_PROPERTY_VALUE_LIST_USES_COMMAS       (1<<1) /* otherwise spaces */

// Define what mechanism the CSS parser uses for parsing the property.
// See CSSParserImpl::ParseProperty(nsCSSPropertyID).  Don't use 0 so that
// we can verify that every property sets one of the values.
#define CSS_PROPERTY_PARSE_PROPERTY_MASK          (7<<9)
#define CSS_PROPERTY_PARSE_INACCESSIBLE           (1<<9)
#define CSS_PROPERTY_PARSE_FUNCTION               (2<<9)

// See CSSParserImpl::ParseSingleValueProperty and comment above
// CSS_PROPERTY_PARSE_FUNCTION (which is different).
#define CSS_PROPERTY_VALUE_PARSER_FUNCTION        (1<<12)
static_assert((CSS_PROPERTY_PARSE_PROPERTY_MASK &
               CSS_PROPERTY_VALUE_PARSER_FUNCTION) == 0,
              "didn't leave enough room for the parse property constants");

// There's a free bit here.

// This property's getComputedStyle implementation requires layout to be
// flushed.
#define CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH     (1<<20)

// The following two flags along with the pref defines where the this
// property can be used:
// * If none of the two flags is presented, the pref completely controls
//   the availability of this property. And in that case, if it has no
//   pref, this property is usable everywhere.
// * If any of the flags is set, this property is always enabled in the
//   specific contexts regardless of the value of the pref. If there is
//   no pref for this property at all in this case, it is an internal-
//   only property, which cannot be used anywhere else, and should be
//   wrapped in "#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL".
// Note that, these flags have no effect on the use of aliases of this
// property.
// Furthermore, for the purposes of animation (including triggering
// transitions) these flags are ignored. That is, if the property is disabled
// by a pref, we will *not* run animations or transitions on it even in
// UA sheets or chrome.
#define CSS_PROPERTY_ENABLED_MASK                 (3<<22)
#define CSS_PROPERTY_ENABLED_IN_UA_SHEETS         (1<<22)
#define CSS_PROPERTY_ENABLED_IN_CHROME            (1<<23)
#define CSS_PROPERTY_ENABLED_IN_UA_SHEETS_AND_CHROME \
  (CSS_PROPERTY_ENABLED_IN_UA_SHEETS | CSS_PROPERTY_ENABLED_IN_CHROME)

// This property can be animated on the compositor.
#define CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR    (1<<27)

// This property is an internal property that is not represented
// in the DOM.  Properties with this flag must be defined in an #ifndef
// CSS_PROP_LIST_EXCLUDE_INTERNAL section of nsCSSPropList.h.
#define CSS_PROPERTY_INTERNAL                     (1<<28)

/**
 * Types of animatable values.
 */
enum nsStyleAnimType {
  // requires a custom implementation in
  // StyleAnimationValue::ExtractComputedValue
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

  // float values
  eStyleAnimType_float,

  // nscolor values
  eStyleAnimType_Color,

  // StyleComplexColor values
  eStyleAnimType_ComplexColor,

  // nsStyleSVGPaint values
  eStyleAnimType_PaintServer,

  // RefPtr<nsCSSShadowArray> values
  eStyleAnimType_Shadow,

  // discrete values
  eStyleAnimType_Discrete,

  // property not animatable
  eStyleAnimType_None
};

class nsCSSProps {
public:
  typedef mozilla::CSSEnabledState EnabledState;

  struct KTableEntry
  {
    // KTableEntry objects can be initialized either with an int16_t value
    // or a value of an enumeration type that can fit within an int16_t.

    constexpr KTableEntry(nsCSSKeyword aKeyword, int16_t aValue)
      : mKeyword(aKeyword)
      , mValue(aValue)
    {
    }

    template<typename T,
             typename = typename std::enable_if<std::is_enum<T>::value>::type>
    constexpr KTableEntry(nsCSSKeyword aKeyword, T aValue)
      : mKeyword(aKeyword)
      , mValue(static_cast<int16_t>(aValue))
    {
      static_assert(mozilla::EnumTypeFitsWithin<T, int16_t>::value,
                    "aValue must be an enum that fits within mValue");
    }

    bool IsSentinel() const
    {
      return mKeyword == eCSSKeyword_UNKNOWN && mValue == -1;
    }

    nsCSSKeyword mKeyword;
    int16_t mValue;
  };

  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Looks up the property with name aProperty and returns its corresponding
  // nsCSSPropertyID value.  If aProperty is the name of a custom property,
  // then eCSSPropertyExtra_variable will be returned.
  static nsCSSPropertyID LookupProperty(const nsAString& aProperty,
                                      EnabledState aEnabled);
  // As above, but looked up using a property's IDL name.
  // eCSSPropertyExtra_variable won't be returned from these methods.
  static nsCSSPropertyID LookupPropertyByIDLName(
      const nsAString& aPropertyIDLName,
      EnabledState aEnabled);
  static nsCSSPropertyID LookupPropertyByIDLName(
      const nsACString& aPropertyIDLName,
      EnabledState aEnabled);

  // Returns whether aProperty is a custom property name, i.e. begins with
  // "--".  This assumes that the CSS Variables pref has been enabled.
  static bool IsCustomPropertyName(const nsAString& aProperty);

  static inline bool IsShorthand(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return (aProperty >= eCSSProperty_COUNT_no_shorthands);
  }

  // Same but for @font-face descriptors
  static nsCSSFontDesc LookupFontDesc(const nsAString& aProperty);

  // Given a property enum, get the string value
  static const nsCString& GetStringValue(nsCSSPropertyID aProperty);
  static const nsCString& GetStringValue(nsCSSFontDesc aFontDesc);
  static const nsCString& GetStringValue(nsCSSCounterDesc aCounterDesc);

  // Given a CSS Property and a Property Enum Value
  // Return back a const nsString& representation of the
  // value. Return back nullstr if no value is found
  static const nsCString& LookupPropertyValue(nsCSSPropertyID aProperty, int32_t aValue);

  // Get a color name for a predefined color value like buttonhighlight or activeborder
  // Sets the aStr param to the name of the propertyID
  static bool GetColorName(int32_t aPropID, nsCString &aStr);

  // Returns the index of |aKeyword| in |aTable|, if it exists there;
  // otherwise, returns -1.
  // NOTE: Generally, clients should call FindKeyword() instead of this method.
  static int32_t FindIndexOfKeyword(nsCSSKeyword aKeyword,
                                    const KTableEntry aTable[]);

  // Find |aKeyword| in |aTable|, if found set |aValue| to its corresponding value.
  // If not found, return false and do not set |aValue|.
  static bool FindKeyword(nsCSSKeyword aKeyword, const KTableEntry aTable[],
                          int32_t& aValue);
  // Return the first keyword in |aTable| that has the corresponding value |aValue|.
  // Return |eCSSKeyword_UNKNOWN| if not found.
  static nsCSSKeyword ValueToKeywordEnum(int32_t aValue,
                                         const KTableEntry aTable[]);
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  static nsCSSKeyword ValueToKeywordEnum(T aValue,
                                         const KTableEntry aTable[])
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int16_t>::value,
                  "aValue must be an enum that fits within KTableEntry::mValue");
    return ValueToKeywordEnum(static_cast<int16_t>(aValue), aTable);
  }
  // Ditto but as a string, return "" when not found.
  static const nsCString& ValueToKeyword(int32_t aValue,
                                         const KTableEntry aTable[]);
  template<typename T,
           typename = typename std::enable_if<std::is_enum<T>::value>::type>
  static const nsCString& ValueToKeyword(T aValue, const KTableEntry aTable[])
  {
    static_assert(mozilla::EnumTypeFitsWithin<T, int16_t>::value,
                  "aValue must be an enum that fits within KTableEntry::mValue");
    return ValueToKeyword(static_cast<int16_t>(aValue), aTable);
  }

  static const KTableEntry* const kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];
  static const nsStyleAnimType kAnimTypeTable[eCSSProperty_COUNT_no_shorthands];

private:
  static const uint32_t        kFlagsTable[eCSSProperty_COUNT];

public:
  static inline bool PropHasFlags(nsCSSPropertyID aProperty, uint32_t aFlags)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    MOZ_ASSERT(!(aFlags & CSS_PROPERTY_PARSE_PROPERTY_MASK),
               "The CSS_PROPERTY_PARSE_* values are not bitflags; don't pass "
               "them to PropHasFlags.  You probably want PropertyParseType "
               "instead.");
    return (nsCSSProps::kFlagsTable[aProperty] & aFlags) == aFlags;
  }

  static inline uint32_t PropertyParseType(nsCSSPropertyID aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_PARSE_PROPERTY_MASK;
  }

private:
  static const uint32_t kParserVariantTable[eCSSProperty_COUNT_no_shorthands];

public:
  static inline uint32_t ParserVariant(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
               "out of range");
    return nsCSSProps::kParserVariantTable[aProperty];
  }

private:
  // A table for shorthand properties.  The appropriate index is the
  // property ID minus eCSSProperty_COUNT_no_shorthands.
  static const nsCSSPropertyID *const
    kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands];

public:
  static inline
  const nsCSSPropertyID * SubpropertyEntryFor(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(eCSSProperty_COUNT_no_shorthands <= aProperty &&
               aProperty < eCSSProperty_COUNT,
               "out of range");
    return nsCSSProps::kSubpropertyTable[aProperty -
                                         eCSSProperty_COUNT_no_shorthands];
  }

private:
  static bool gPropertyEnabled[eCSSProperty_COUNT_with_aliases];

private:
  // Defined in the generated nsCSSPropsGenerated.inc.
  static const char* const kIDLNameTable[eCSSProperty_COUNT];

public:
  /**
   * Returns the IDL name of the specified property, which must be a
   * longhand, logical or shorthand property.  The IDL name is the property
   * name with any hyphen-lowercase character pairs replaced by an
   * uppercase character:
   * https://drafts.csswg.org/cssom/#css-property-to-idl-attribute
   *
   * As a special case, the string "cssFloat" is returned for the float
   * property.  nullptr is returned for internal properties.
   */
  static const char* PropertyIDLName(nsCSSPropertyID aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return kIDLNameTable[aProperty];
  }

private:
  static const int32_t kIDLNameSortPositionTable[eCSSProperty_COUNT];

public:
  /**
   * Returns the position of the specified property in a list of all
   * properties sorted by their IDL name.
   */
  static int32_t PropertyIDLNameSortPosition(nsCSSPropertyID aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return kIDLNameSortPositionTable[aProperty];
  }

  static bool IsEnabled(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_with_aliases,
               "out of range");
    // In the child process, assert that we're not trying to parse stylesheets
    // before we've gotten all our prefs.
    MOZ_ASSERT_IF(!XRE_IsParentProcess(),
                  mozilla::Preferences::ArePrefsInitedInContentProcess());
    return gPropertyEnabled[aProperty];
  }

  // A table for the use counter associated with each CSS property.  If a
  // property does not have a use counter defined in UseCounters.conf, then
  // its associated entry is |eUseCounter_UNKNOWN|.
  static const mozilla::UseCounter gPropertyUseCounter[eCSSProperty_COUNT_no_shorthands];

public:

  static mozilla::UseCounter UseCounterFor(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
               "out of range");
    return gPropertyUseCounter[aProperty];
  }

  static bool IsEnabled(nsCSSPropertyID aProperty, EnabledState aEnabled)
  {
    if (IsEnabled(aProperty)) {
      return true;
    }
    if (aEnabled == EnabledState::eIgnoreEnabledState) {
      return true;
    }
    if ((aEnabled & EnabledState::eInUASheets) &&
        PropHasFlags(aProperty, CSS_PROPERTY_ENABLED_IN_UA_SHEETS))
    {
      return true;
    }
    if ((aEnabled & EnabledState::eInChrome) &&
        PropHasFlags(aProperty, CSS_PROPERTY_ENABLED_IN_CHROME))
    {
      return true;
    }
    return false;
  }

public:

  // Return an array of possible list style types, and the length of
  // the array.
  static const char* const* GetListStyleTypes(int32_t *aLength);

// Storing the enabledstate_ value in an nsCSSPropertyID variable is a small hack
// to avoid needing a separate variable declaration for its real type
// (CSSEnabledState), which would then require using a block and
// therefore a pair of macros by consumers for the start and end of the loop.
#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(it_, prop_, enabledstate_)   \
  for (const nsCSSPropertyID *it_ = nsCSSProps::SubpropertyEntryFor(prop_), \
                            es_ = (nsCSSPropertyID)((enabledstate_) |       \
                                                  CSSEnabledState(0));    \
       *it_ != eCSSProperty_UNKNOWN; ++it_)                               \
    if (nsCSSProps::IsEnabled(*it_, (mozilla::CSSEnabledState) es_))

  // Keyword/Enum value tables
  static const KTableEntry kAnimationDirectionKTable[];
  static const KTableEntry kAnimationFillModeKTable[];
  static const KTableEntry kAnimationIterationCountKTable[];
  static const KTableEntry kAnimationPlayStateKTable[];
  static const KTableEntry kAnimationTimingFunctionKTable[];
  static const KTableEntry kAppearanceKTable[];
  static const KTableEntry kAzimuthKTable[];
  static const KTableEntry kBackfaceVisibilityKTable[];
  static const KTableEntry kTransformStyleKTable[];
  static const KTableEntry kImageLayerAttachmentKTable[];
  static const KTableEntry kBackgroundOriginKTable[];
  static const KTableEntry kMaskOriginKTable[];
  static const KTableEntry kImageLayerPositionKTable[];
  static const KTableEntry kImageLayerRepeatKTable[];
  static const KTableEntry kImageLayerRepeatPartKTable[];
  static const KTableEntry kImageLayerSizeKTable[];
  static const KTableEntry kImageLayerCompositeKTable[];
  static const KTableEntry kImageLayerModeKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.background-clip.text" changes:
  static KTableEntry kBackgroundClipKTable[];
  static const KTableEntry kMaskClipKTable[];
  static const KTableEntry kBlendModeKTable[];
  static const KTableEntry kBorderCollapseKTable[];
  static const KTableEntry kBorderImageRepeatKTable[];
  static const KTableEntry kBorderImageSliceKTable[];
  static const KTableEntry kBorderStyleKTable[];
  static const KTableEntry kBorderWidthKTable[];
  static const KTableEntry kBoxAlignKTable[];
  static const KTableEntry kBoxDecorationBreakKTable[];
  static const KTableEntry kBoxDirectionKTable[];
  static const KTableEntry kBoxOrientKTable[];
  static const KTableEntry kBoxPackKTable[];
  static const KTableEntry kClipPathGeometryBoxKTable[];
  static const KTableEntry kCounterRangeKTable[];
  static const KTableEntry kCounterSpeakAsKTable[];
  static const KTableEntry kCounterSymbolsSystemKTable[];
  static const KTableEntry kCounterSystemKTable[];
  static const KTableEntry kDominantBaselineKTable[];
  static const KTableEntry kShapeRadiusKTable[];
  static const KTableEntry kFillRuleKTable[];
  static const KTableEntry kFilterFunctionKTable[];
  static const KTableEntry kImageRenderingKTable[];
  static const KTableEntry kShapeOutsideShapeBoxKTable[];
  static const KTableEntry kShapeRenderingKTable[];
  static const KTableEntry kStrokeLinecapKTable[];
  static const KTableEntry kStrokeLinejoinKTable[];
  static const KTableEntry kStrokeContextValueKTable[];
  static const KTableEntry kVectorEffectKTable[];
  static const KTableEntry kTextAnchorKTable[];
  static const KTableEntry kTextRenderingKTable[];
  static const KTableEntry kColorAdjustKTable[];
  static const KTableEntry kColorInterpolationKTable[];
  static const KTableEntry kColumnFillKTable[];
  static const KTableEntry kColumnSpanKTable[];
  static const KTableEntry kBoxPropSourceKTable[];
  static const KTableEntry kBoxShadowTypeKTable[];
  static const KTableEntry kBoxSizingKTable[];
  static const KTableEntry kCaptionSideKTable[];
  static const KTableEntry kClearKTable[];
  static const KTableEntry kColorKTable[];
  static const KTableEntry kContentKTable[];
  static const KTableEntry kControlCharacterVisibilityKTable[];
  static const KTableEntry kCursorKTable[];
  static const KTableEntry kDirectionKTable[];
  // Not const because we modify its entries when various
  // "layout.css.*.enabled" prefs changes:
  static KTableEntry kDisplayKTable[];
  static const KTableEntry kElevationKTable[];
  static const KTableEntry kEmptyCellsKTable[];
  // -- tables for parsing the {align,justify}-{content,items,self} properties --
  static const KTableEntry kAlignAllKeywords[];
  static const KTableEntry kAlignOverflowPosition[]; // <overflow-position>
  static const KTableEntry kAlignSelfPosition[];     // <self-position>
  static const KTableEntry kAlignLegacy[];           // 'legacy'
  static const KTableEntry kAlignLegacyPosition[];   // 'left/right/center'
  static const KTableEntry kAlignAutoNormalStretchBaseline[]; // 'auto/normal/stretch/baseline'
  static const KTableEntry kAlignNormalStretchBaseline[]; // 'normal/stretch/baseline'
  static const KTableEntry kAlignNormalBaseline[]; // 'normal/baseline'
  static const KTableEntry kAlignContentDistribution[]; // <content-distribution>
  static const KTableEntry kAlignContentPosition[]; // <content-position>
  // -- tables for auto-completion of the {align,justify}-{content,items,self} properties --
  static const KTableEntry kAutoCompletionAlignJustifySelf[];
  static const KTableEntry kAutoCompletionAlignItems[];
  static const KTableEntry kAutoCompletionAlignJustifyContent[];
  // ------------------------------------------------------------------
  static const KTableEntry kFlexDirectionKTable[];
  static const KTableEntry kFlexWrapKTable[];
  static const KTableEntry kFloatKTable[];
  static const KTableEntry kFloatEdgeKTable[];
  static const KTableEntry kFontDisplayKTable[];
  static const KTableEntry kFontKTable[];
  static const KTableEntry kFontKerningKTable[];
  static const KTableEntry kFontOpticalSizingKTable[];
  static const KTableEntry kFontSizeKTable[];
  static const KTableEntry kFontSmoothingKTable[];
  static const KTableEntry kFontStretchKTable[];
  static const KTableEntry kFontStyleKTable[];
  static const KTableEntry kFontSynthesisKTable[];
  static const KTableEntry kFontVariantKTable[];
  static const KTableEntry kFontVariantAlternatesKTable[];
  static const KTableEntry kFontVariantAlternatesFuncsKTable[];
  static const KTableEntry kFontVariantCapsKTable[];
  static const KTableEntry kFontVariantEastAsianKTable[];
  static const KTableEntry kFontVariantLigaturesKTable[];
  static const KTableEntry kFontVariantNumericKTable[];
  static const KTableEntry kFontVariantPositionKTable[];
  static const KTableEntry kFontWeightKTable[];
  static const KTableEntry kGridAutoFlowKTable[];
  static const KTableEntry kGridTrackBreadthKTable[];
  static const KTableEntry kHyphensKTable[];
  static const KTableEntry kImageOrientationKTable[];
  static const KTableEntry kImageOrientationFlipKTable[];
  static const KTableEntry kIsolationKTable[];
  static const KTableEntry kIMEModeKTable[];
  static const KTableEntry kLineHeightKTable[];
  static const KTableEntry kListStylePositionKTable[];
  static const KTableEntry kMaskTypeKTable[];
  static const KTableEntry kMathVariantKTable[];
  static const KTableEntry kMathDisplayKTable[];
  static const KTableEntry kContainKTable[];
  static const KTableEntry kContextOpacityKTable[];
  static const KTableEntry kContextPatternKTable[];
  static const KTableEntry kObjectFitKTable[];
  static const KTableEntry kOrientKTable[];
  static const KTableEntry kOutlineStyleKTable[];
  static const KTableEntry kOverflowKTable[];
  static const KTableEntry kOverflowSubKTable[];
  static const KTableEntry kOverflowClipBoxKTable[];
  static const KTableEntry kOverflowWrapKTable[];
  static const KTableEntry kPageBreakKTable[];
  static const KTableEntry kPageBreakInsideKTable[];
  static const KTableEntry kPageMarksKTable[];
  static const KTableEntry kPageSizeKTable[];
  static const KTableEntry kPitchKTable[];
  static const KTableEntry kPointerEventsKTable[];
  static const KTableEntry kPositionKTable[];
  static const KTableEntry kRadialGradientShapeKTable[];
  static const KTableEntry kRadialGradientSizeKTable[];
  static const KTableEntry kRadialGradientLegacySizeKTable[];
  static const KTableEntry kResizeKTable[];
  static const KTableEntry kRubyAlignKTable[];
  static const KTableEntry kRubyPositionKTable[];
  static const KTableEntry kScrollBehaviorKTable[];
  static const KTableEntry kOverscrollBehaviorKTable[];
  static const KTableEntry kScrollSnapTypeKTable[];
  static const KTableEntry kSpeakKTable[];
  static const KTableEntry kSpeakHeaderKTable[];
  static const KTableEntry kSpeakNumeralKTable[];
  static const KTableEntry kSpeakPunctuationKTable[];
  static const KTableEntry kSpeechRateKTable[];
  static const KTableEntry kStackSizingKTable[];
  static const KTableEntry kTableLayoutKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.text-align-unsafe-value.enabled" changes:
  static KTableEntry kTextAlignKTable[];
  static KTableEntry kTextAlignLastKTable[];
  static const KTableEntry kTextCombineUprightKTable[];
  static const KTableEntry kTextDecorationLineKTable[];
  static const KTableEntry kTextDecorationStyleKTable[];
  static const KTableEntry kTextEmphasisPositionKTable[];
  static const KTableEntry kTextEmphasisStyleFillKTable[];
  static const KTableEntry kTextEmphasisStyleShapeKTable[];
  static const KTableEntry kTextJustifyKTable[];
  static const KTableEntry kTextOrientationKTable[];
  static const KTableEntry kTextOverflowKTable[];
  static const KTableEntry kTextSizeAdjustKTable[];
  static const KTableEntry kTextTransformKTable[];
  static const KTableEntry kTouchActionKTable[];
  static const KTableEntry kTopLayerKTable[];
  static const KTableEntry kTransformBoxKTable[];
  static const KTableEntry kTransitionTimingFunctionKTable[];
  static const KTableEntry kUnicodeBidiKTable[];
  static const KTableEntry kUserFocusKTable[];
  static const KTableEntry kUserInputKTable[];
  static const KTableEntry kUserModifyKTable[];
  static const KTableEntry kUserSelectKTable[];
  static const KTableEntry kVerticalAlignKTable[];
  static const KTableEntry kVisibilityKTable[];
  static const KTableEntry kVolumeKTable[];
  static const KTableEntry kWhitespaceKTable[];
  static const KTableEntry kWidthKTable[]; // also min-width, max-width
  static const KTableEntry kFlexBasisKTable[];
  static const KTableEntry kWindowDraggingKTable[];
  static const KTableEntry kWindowShadowKTable[];
  static const KTableEntry kWordBreakKTable[];
  static const KTableEntry kWritingModeKTable[];
};

#endif /* nsCSSProps_h___ */
