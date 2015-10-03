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
#include "nsCSSProperty.h"
#include "nsStyleStructFwd.h"
#include "nsCSSKeywords.h"
#include "mozilla/UseCounter.h"

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
#define VARIANT_POSITIVE_DIMENSION 0x10000000 // Only lengths greater than 0.0
#define VARIANT_NONNEGATIVE_DIMENSION 0x20000000 // Only lengths greater than or equal to 0.0
// Keyword used iff gfx.font_rendering.opentype_svg.enabled is true:
#define VARIANT_OPENTYPE_SVG_KEYWORD 0x40000000

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

// This property is a logical property (such as padding-inline-start).
#define CSS_PROPERTY_LOGICAL                      (1<<0)

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

// This is a logical property that represents some value associated with
// a logical axis rather than a logical box side, and thus has two
// corresponding physical properties it could set rather than four.  For
// example, the block-size logical property has this flag set, as it
// represents the size in either the block or inline axis dimensions, and
// has two corresponding physical properties, width and height.  Must not
// be used in conjunction with CSS_PROPERTY_LOGICAL_END_EDGE.
#define CSS_PROPERTY_LOGICAL_AXIS                 (1<<7)

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

// Does this property support the hashless hex color quirk in quirks mode?
#define CSS_PROPERTY_HASHLESS_COLOR_QUIRK         (1<<15)

// Does this property support the unitless length quirk in quirks mode?
#define CSS_PROPERTY_UNITLESS_LENGTH_QUIRK        (1<<16)

// Is this property (which must be a shorthand) really an alias?
#define CSS_PROPERTY_IS_ALIAS                     (1<<17)

// Does the property apply to ::-moz-placeholder?
#define CSS_PROPERTY_APPLIES_TO_PLACEHOLDER       (1<<18)

// This property is allowed in an @page rule.
#define CSS_PROPERTY_APPLIES_TO_PAGE_RULE         (1<<19)

// This property's getComputedStyle implementation requires layout to be
// flushed.
#define CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH     (1<<20)

// This property requires a stacking context.
#define CSS_PROPERTY_CREATES_STACKING_CONTEXT     (1<<21)

// This property is always enabled in UA sheets.  This is meant to be used
// together with a pref that enables the property for non-UA sheets.
// Note that if such a property has an alias, then any use of that alias
// in an UA sheet will still be ignored unless the pref is enabled.
// In other words, this bit has no effect on the use of aliases.
#define CSS_PROPERTY_ALWAYS_ENABLED_IN_UA_SHEETS  (1<<22)

// XXX (1<<23) will shortly be reused in bug 1069192.

// This property's unitless values are pixels.
#define CSS_PROPERTY_NUMBERS_ARE_PIXELS           (1<<24)

// This property is a logical property for one of the two block axis
// sides (such as margin-block-start or margin-block-end).  Must only be
// set if CSS_PROPERTY_LOGICAL is set.  When not set, the logical
// property is for one of the two inline axis sides (such as
// margin-inline-start or margin-inline-end).
#define CSS_PROPERTY_LOGICAL_BLOCK_AXIS           (1<<25)

// This property is a logical property for the "end" edge of the
// axis determined by the presence or absence of
// CSS_PROPERTY_LOGICAL_BLOCK_AXIS (such as margin-block-end or
// margin-inline-end).  Must only be set if CSS_PROPERTY_LOGICAL is set.
// When not set, the logical property is for the "start" edge (such as
// margin-block-start or margin-inline-start).
#define CSS_PROPERTY_LOGICAL_END_EDGE             (1<<26)

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
  typedef int16_t KTableValue;

  static void AddRefTable(void);
  static void ReleaseTable(void);

  enum EnabledState {
    // The default EnabledState: only enable what's enabled for all content,
    // given the current values of preferences.
    eEnabledForAllContent = 0,
    // Enable a property in UA sheets.
    eEnabledInUASheets    = 0x01,
    // Special value to unconditionally enable a property. This implies all the
    // bits above, but is strictly more than just their OR-ed union.
    // This just skips any test so a property will be enabled even if it would
    // have been disabled with all the bits above set.
    eIgnoreEnabledState   = 0xff
  };

  // Looks up the property with name aProperty and returns its corresponding
  // nsCSSProperty value.  If aProperty is the name of a custom property,
  // then eCSSPropertyExtra_variable will be returned.
  static nsCSSProperty LookupProperty(const nsAString& aProperty,
                                      EnabledState aEnabled);
  static nsCSSProperty LookupProperty(const nsACString& aProperty,
                                      EnabledState aEnabled);
  // Returns whether aProperty is a custom property name, i.e. begins with
  // "--".  This assumes that the CSS Variables pref has been enabled.
  static bool IsCustomPropertyName(const nsAString& aProperty);
  static bool IsCustomPropertyName(const nsACString& aProperty);

  static inline bool IsShorthand(nsCSSProperty aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return (aProperty >= eCSSProperty_COUNT_no_shorthands);
  }

  // Must be given a longhand property.
  static bool IsInherited(nsCSSProperty aProperty);

  // Same but for @font-face descriptors
  static nsCSSFontDesc LookupFontDesc(const nsAString& aProperty);
  static nsCSSFontDesc LookupFontDesc(const nsACString& aProperty);

  // For @counter-style descriptors
  static nsCSSCounterDesc LookupCounterDesc(const nsAString& aProperty);
  static nsCSSCounterDesc LookupCounterDesc(const nsACString& aProperty);

  // For predefined counter styles which need to be lower-cased during parse
  static bool IsPredefinedCounterStyle(const nsAString& aStyle);
  static bool IsPredefinedCounterStyle(const nsACString& aStyle);

  // Given a property enum, get the string value
  static const nsAFlatCString& GetStringValue(nsCSSProperty aProperty);
  static const nsAFlatCString& GetStringValue(nsCSSFontDesc aFontDesc);
  static const nsAFlatCString& GetStringValue(nsCSSCounterDesc aCounterDesc);

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
  static int32_t FindIndexOfKeyword(nsCSSKeyword aKeyword,
                                    const KTableValue aTable[]);

  // Find |aKeyword| in |aTable|, if found set |aValue| to its corresponding value.
  // If not found, return false and do not set |aValue|.
  static bool FindKeyword(nsCSSKeyword aKeyword, const KTableValue aTable[],
                          int32_t& aValue);
  // Return the first keyword in |aTable| that has the corresponding value |aValue|.
  // Return |eCSSKeyword_UNKNOWN| if not found.
  static nsCSSKeyword ValueToKeywordEnum(int32_t aValue, 
                                         const KTableValue aTable[]);
  // Ditto but as a string, return "" when not found.
  static const nsAFlatCString& ValueToKeyword(int32_t aValue,
                                              const KTableValue aTable[]);

  static const nsStyleStructID kSIDTable[eCSSProperty_COUNT_no_shorthands];
  static const KTableValue* const kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];
  static const nsStyleAnimType kAnimTypeTable[eCSSProperty_COUNT_no_shorthands];
  static const ptrdiff_t
    kStyleStructOffsetTable[eCSSProperty_COUNT_no_shorthands];

private:
  static const uint32_t        kFlagsTable[eCSSProperty_COUNT];

public:
  static inline bool PropHasFlags(nsCSSProperty aProperty, uint32_t aFlags)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    MOZ_ASSERT(!(aFlags & CSS_PROPERTY_PARSE_PROPERTY_MASK),
               "The CSS_PROPERTY_PARSE_* values are not bitflags; don't pass "
               "them to PropHasFlags.  You probably want PropertyParseType "
               "instead.");
    return (nsCSSProps::kFlagsTable[aProperty] & aFlags) == aFlags;
  }

  static inline uint32_t PropertyParseType(nsCSSProperty aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_PARSE_PROPERTY_MASK;
  }

  static inline uint32_t ValueRestrictions(nsCSSProperty aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return nsCSSProps::kFlagsTable[aProperty] &
           CSS_PROPERTY_VALUE_RESTRICTION_MASK;
  }

private:
  // Lives in nsCSSParser.cpp for the macros it depends on.
  static const uint32_t kParserVariantTable[eCSSProperty_COUNT_no_shorthands];

public:
  static inline uint32_t ParserVariant(nsCSSProperty aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
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
    MOZ_ASSERT(eCSSProperty_COUNT_no_shorthands <= aProperty &&
               aProperty < eCSSProperty_COUNT,
               "out of range");
    return nsCSSProps::kSubpropertyTable[aProperty -
                                         eCSSProperty_COUNT_no_shorthands];
  }

  // Returns an eCSSProperty_UNKNOWN-terminated array of the shorthand
  // properties containing |aProperty|, sorted from those that contain
  // the most properties to those that contain the least.
  static const nsCSSProperty * ShorthandsContaining(nsCSSProperty aProperty) {
    MOZ_ASSERT(gShorthandsContainingPool, "uninitialized");
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
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
    MOZ_ASSERT(0 <= aSID && aSID < nsStyleStructID_Length,
               "out of range");
    return gPropertyCountInStruct[aSID];
  }
  /**
   * Return an index for aProperty that is unique within its SID and in
   * the range 0 <= index < PropertyCountInStruct(aSID).
   */
  static size_t PropertyIndexInStruct(nsCSSProperty aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
               "out of range");
    return gPropertyIndexInStruct[aProperty];
  }

private:
  // A table for logical property groups.  Indexes are
  // nsCSSPropertyLogicalGroup values.
  static const nsCSSProperty* const
    kLogicalGroupTable[eCSSPropertyLogicalGroup_COUNT];

public:
  /**
   * Returns an array of longhand physical properties which can be set by
   * the argument, which must be a logical longhand property.  The returned
   * array is terminated by an eCSSProperty_UNKNOWN value.  For example,
   * given eCSSProperty_margin_block_start, returns an array of the four
   * properties eCSSProperty_margin_top, eCSSProperty_margin_right,
   * eCSSProperty_margin_bottom and eCSSProperty_margin_left, followed
   * by the sentinel.
   *
   * When called with a property that has the CSS_PROPERTY_LOGICAL_AXIS
   * flag, the returned array will have two values preceding the sentinel;
   * otherwise it will have four.
   *
   * (Note that the running time of this function is proportional to the
   * number of logical longhand properties that exist.  If we start
   * getting too many of these properties, we should make kLogicalGroupTable
   * be a simple array of eCSSProperty_COUNT length.)
   */
  static const nsCSSProperty* LogicalGroup(nsCSSProperty aProperty);

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
  static const char* PropertyIDLName(nsCSSProperty aProperty)
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
  static int32_t PropertyIDLNameSortPosition(nsCSSProperty aProperty)
  {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return kIDLNameSortPositionTable[aProperty];
  }

  static bool IsEnabled(nsCSSProperty aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_with_aliases,
               "out of range");
    return gPropertyEnabled[aProperty];
  }

  // A table for the use counter associated with each CSS property.  If a
  // property does not have a use counter defined in UseCounters.conf, then
  // its associated entry is |eUseCounter_UNKNOWN|.
  static const mozilla::UseCounter gPropertyUseCounter[eCSSProperty_COUNT_no_shorthands];

public:

  static mozilla::UseCounter UseCounterFor(nsCSSProperty aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
               "out of range");
    return gPropertyUseCounter[aProperty];
  }

  static bool IsEnabled(nsCSSProperty aProperty, EnabledState aEnabled)
  {
    if (IsEnabled(aProperty)) {
      return true;
    }
    if (aEnabled == eIgnoreEnabledState) {
      return true;
    }
    if ((aEnabled & eEnabledInUASheets) &&
        PropHasFlags(aProperty, CSS_PROPERTY_ALWAYS_ENABLED_IN_UA_SHEETS))
    {
      return true;
    }
    return false;
  }

public:

// Storing the enabledstate_ value in an nsCSSProperty variable is a small hack
// to avoid needing a separate variable declaration for its real type
// (nsCSSProps::EnabledState), which would then require using a block and
// therefore a pair of macros by consumers for the start and end of the loop.
#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(it_, prop_, enabledstate_)       \
  for (const nsCSSProperty *it_ = nsCSSProps::SubpropertyEntryFor(prop_),     \
                            es_ = (nsCSSProperty) (enabledstate_);            \
       *it_ != eCSSProperty_UNKNOWN; ++it_)                                   \
    if (nsCSSProps::IsEnabled(*it_, (nsCSSProps::EnabledState) es_))

  // Keyword/Enum value tables
  static const KTableValue kAnimationDirectionKTable[];
  static const KTableValue kAnimationFillModeKTable[];
  static const KTableValue kAnimationIterationCountKTable[];
  static const KTableValue kAnimationPlayStateKTable[];
  static const KTableValue kAnimationTimingFunctionKTable[];
  static const KTableValue kAppearanceKTable[];
  static const KTableValue kAzimuthKTable[];
  static const KTableValue kBackfaceVisibilityKTable[];
  static const KTableValue kTransformStyleKTable[];
  static const KTableValue kBackgroundAttachmentKTable[];
  static const KTableValue kBackgroundOriginKTable[];
  static const KTableValue kBackgroundPositionKTable[];
  static const KTableValue kBackgroundRepeatKTable[];
  static const KTableValue kBackgroundRepeatPartKTable[];
  static const KTableValue kBackgroundSizeKTable[];
  static const KTableValue kBlendModeKTable[];
  static const KTableValue kBorderCollapseKTable[];
  static const KTableValue kBorderColorKTable[];
  static const KTableValue kBorderImageRepeatKTable[];
  static const KTableValue kBorderImageSliceKTable[];
  static const KTableValue kBorderStyleKTable[];
  static const KTableValue kBorderWidthKTable[];
  static const KTableValue kBoxAlignKTable[];
  static const KTableValue kBoxDecorationBreakKTable[];
  static const KTableValue kBoxDirectionKTable[];
  static const KTableValue kBoxOrientKTable[];
  static const KTableValue kBoxPackKTable[];
  static const KTableValue kClipShapeSizingKTable[];
  static const KTableValue kCounterRangeKTable[];
  static const KTableValue kCounterSpeakAsKTable[];
  static const KTableValue kCounterSymbolsSystemKTable[];
  static const KTableValue kCounterSystemKTable[];
  static const KTableValue kDominantBaselineKTable[];
  static const KTableValue kShapeRadiusKTable[];
  static const KTableValue kFillRuleKTable[];
  static const KTableValue kFilterFunctionKTable[];
  static const KTableValue kImageRenderingKTable[];
  static const KTableValue kShapeRenderingKTable[];
  static const KTableValue kStrokeLinecapKTable[];
  static const KTableValue kStrokeLinejoinKTable[];
  static const KTableValue kStrokeContextValueKTable[];
  static const KTableValue kVectorEffectKTable[];
  static const KTableValue kTextAnchorKTable[];
  static const KTableValue kTextRenderingKTable[];
  static const KTableValue kColorInterpolationKTable[];
  static const KTableValue kColumnFillKTable[];
  static const KTableValue kBoxPropSourceKTable[];
  static const KTableValue kBoxShadowTypeKTable[];
  static const KTableValue kBoxSizingKTable[];
  static const KTableValue kCaptionSideKTable[];
  static const KTableValue kClearKTable[];
  static const KTableValue kColorKTable[];
  static const KTableValue kContentKTable[];
  static const KTableValue kControlCharacterVisibilityKTable[];
  static const KTableValue kCursorKTable[];
  static const KTableValue kDirectionKTable[];
  // Not const because we modify its entries when various 
  // "layout.css.*.enabled" prefs changes:
  static KTableValue kDisplayKTable[];
  static const KTableValue kElevationKTable[];
  static const KTableValue kEmptyCellsKTable[];
  static const KTableValue kAlignContentKTable[];
  static const KTableValue kAlignItemsKTable[];
  static const KTableValue kAlignSelfKTable[];
  static const KTableValue kFlexDirectionKTable[];
  static const KTableValue kFlexWrapKTable[];
  static const KTableValue kJustifyContentKTable[];
  static const KTableValue kFloatKTable[];
  static const KTableValue kFloatEdgeKTable[];
  static const KTableValue kFontKTable[];
  static const KTableValue kFontKerningKTable[];
  static const KTableValue kFontSizeKTable[];
  static const KTableValue kFontSmoothingKTable[];
  static const KTableValue kFontStretchKTable[];
  static const KTableValue kFontStyleKTable[];
  static const KTableValue kFontSynthesisKTable[];
  static const KTableValue kFontVariantKTable[];
  static const KTableValue kFontVariantAlternatesKTable[];
  static const KTableValue kFontVariantAlternatesFuncsKTable[];
  static const KTableValue kFontVariantCapsKTable[];
  static const KTableValue kFontVariantEastAsianKTable[];
  static const KTableValue kFontVariantLigaturesKTable[];
  static const KTableValue kFontVariantNumericKTable[];
  static const KTableValue kFontVariantPositionKTable[];
  static const KTableValue kFontWeightKTable[];
  static const KTableValue kGridAutoFlowKTable[];
  static const KTableValue kGridTrackBreadthKTable[];
  static const KTableValue kHyphensKTable[];
  static const KTableValue kImageOrientationKTable[];
  static const KTableValue kImageOrientationFlipKTable[];
  static const KTableValue kIsolationKTable[];
  static const KTableValue kIMEModeKTable[];
  static const KTableValue kLineHeightKTable[];
  static const KTableValue kListStylePositionKTable[];
  static const KTableValue kListStyleKTable[];
  static const KTableValue kMaskTypeKTable[];
  static const KTableValue kMathVariantKTable[];
  static const KTableValue kMathDisplayKTable[];
  static const KTableValue kContainKTable[];
  static const KTableValue kContextOpacityKTable[];
  static const KTableValue kContextPatternKTable[];
  static const KTableValue kObjectFitKTable[];
  static const KTableValue kOrientKTable[];
  static const KTableValue kOutlineStyleKTable[];
  static const KTableValue kOutlineColorKTable[];
  static const KTableValue kOverflowKTable[];
  static const KTableValue kOverflowSubKTable[];
  static const KTableValue kOverflowClipBoxKTable[];
  static const KTableValue kPageBreakKTable[];
  static const KTableValue kPageBreakInsideKTable[];
  static const KTableValue kPageMarksKTable[];
  static const KTableValue kPageSizeKTable[];
  static const KTableValue kPitchKTable[];
  static const KTableValue kPointerEventsKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.sticky.enabled" changes:
  static KTableValue kPositionKTable[];
  static const KTableValue kRadialGradientShapeKTable[];
  static const KTableValue kRadialGradientSizeKTable[];
  static const KTableValue kRadialGradientLegacySizeKTable[];
  static const KTableValue kResizeKTable[];
  static const KTableValue kRubyAlignKTable[];
  static const KTableValue kRubyPositionKTable[];
  static const KTableValue kScrollBehaviorKTable[];
  static const KTableValue kScrollSnapTypeKTable[];
  static const KTableValue kSpeakKTable[];
  static const KTableValue kSpeakHeaderKTable[];
  static const KTableValue kSpeakNumeralKTable[];
  static const KTableValue kSpeakPunctuationKTable[];
  static const KTableValue kSpeechRateKTable[];
  static const KTableValue kStackSizingKTable[];
  static const KTableValue kTableLayoutKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.text-align-true-value.enabled" changes:
  static KTableValue kTextAlignKTable[];
  static KTableValue kTextAlignLastKTable[];
  static const KTableValue kTextCombineUprightKTable[];
  static const KTableValue kTextDecorationLineKTable[];
  static const KTableValue kTextDecorationStyleKTable[];
  static const KTableValue kTextOrientationKTable[];
  static const KTableValue kTextOverflowKTable[];
  static const KTableValue kTextTransformKTable[];
  static const KTableValue kTouchActionKTable[];
  static const KTableValue kTransformBoxKTable[];
  static const KTableValue kTransitionTimingFunctionKTable[];
  static const KTableValue kUnicodeBidiKTable[];
  static const KTableValue kUserFocusKTable[];
  static const KTableValue kUserInputKTable[];
  static const KTableValue kUserModifyKTable[];
  static const KTableValue kUserSelectKTable[];
  static const KTableValue kVerticalAlignKTable[];
  static const KTableValue kVisibilityKTable[];
  static const KTableValue kVolumeKTable[];
  static const KTableValue kWhitespaceKTable[];
  static const KTableValue kWidthKTable[]; // also min-width, max-width
  static const KTableValue kWindowDraggingKTable[];
  static const KTableValue kWindowShadowKTable[];
  static const KTableValue kWordBreakKTable[];
  static const KTableValue kWordWrapKTable[];
  static const KTableValue kWritingModeKTable[];
};

inline nsCSSProps::EnabledState operator|(nsCSSProps::EnabledState a,
                                          nsCSSProps::EnabledState b)
{
  return nsCSSProps::EnabledState(int(a) | int(b));
}

inline nsCSSProps::EnabledState operator&(nsCSSProps::EnabledState a,
                                          nsCSSProps::EnabledState b)
{
  return nsCSSProps::EnabledState(int(a) & int(b));
}

inline nsCSSProps::EnabledState& operator|=(nsCSSProps::EnabledState& a,
                                            nsCSSProps::EnabledState b)
{
  return a = a | b;
}

inline nsCSSProps::EnabledState& operator&=(nsCSSProps::EnabledState& a,
                                            nsCSSProps::EnabledState b)
{
  return a = a & b;
}

#endif /* nsCSSProps_h___ */
