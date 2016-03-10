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
#define VARIANT_ABSOLUTE_DIMENSION 0x80000000 // B Only lengths with absolute length unit

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
#define CSS_PROPERTY_ENABLED_MASK                 (3<<22)
#define CSS_PROPERTY_ENABLED_IN_UA_SHEETS         (1<<22)
#define CSS_PROPERTY_ENABLED_IN_CHROME            (1<<23)
#define CSS_PROPERTY_ENABLED_IN_UA_SHEETS_AND_CHROME \
  (CSS_PROPERTY_ENABLED_IN_UA_SHEETS | CSS_PROPERTY_ENABLED_IN_CHROME)

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

// This property is a logical property which always maps to the same physical
// property, and its values have some custom processing when being mapped to
// the physical property's values.  Must not be used in conjunction with
// CSS_PROPERTY_LOGICAL_{AXIS,BLOCK_AXIS,END_EDGE}.
#define CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING (1<<27)

// This property can be animated on the compositor.
#define CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR    (1<<28)

// This property is an internal property that is not represented
// in the DOM.  Properties with this flag must be defined in an #ifndef
// CSS_PROP_LIST_EXCLUDE_INTERNAL section of nsCSSPropList.h.
#define CSS_PROPERTY_INTERNAL                     (1<<29)

// This property has values that can establish a containing block for
// fixed positioned and absolutely positioned elements.
// This should be set for any properties that can cause an element to be
// such a containing block, as implemented in
// nsStyleDisplay::IsFixedPosContainingBlock.
#define CSS_PROPERTY_FIXPOS_CB                    (1<<30)

// This property has values that can establish a containing block for
// absolutely positioned elements.
// This should be set for any properties that can cause an element to be
// such a containing block, as implemented in
// nsStyleDisplay::IsAbsPosContainingBlock.
// It does not need to be set for properties that also have
// CSS_PROPERTY_FIXPOS_CB set.
#define CSS_PROPERTY_ABSPOS_CB                    (1u<<31)

// NOTE: Before adding any new CSS_PROPERTY_* flags here, we'll need to
// upgrade kFlagsTable to 64-bits -- see bug 1231384.

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

  // RefPtr<nsCSSShadowArray> values
  eStyleAnimType_Shadow,

  // property not animatable
  eStyleAnimType_None
};

class nsCSSProps {
public:
  struct KTableEntry {
    nsCSSKeyword mKeyword;
    int16_t mValue;
  };

  static void AddRefTable(void);
  static void ReleaseTable(void);

  enum EnabledState {
    // The default EnabledState: only enable what's enabled for all content,
    // given the current values of preferences.
    eEnabledForAllContent = 0,
    // Enable a property in UA sheets.
    eEnabledInUASheets    = 0x01,
    // Enable a property in chrome code.
    eEnabledInChrome      = 0x02,
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
  // As above, but looked up using a property's IDL name.
  // eCSSPropertyExtra_variable won't be returned from these methods.
  static nsCSSProperty LookupPropertyByIDLName(
      const nsAString& aPropertyIDLName,
      EnabledState aEnabled);
  static nsCSSProperty LookupPropertyByIDLName(
      const nsACString& aPropertyIDLName,
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
                                    const KTableEntry aTable[]);

  // Find |aKeyword| in |aTable|, if found set |aValue| to its corresponding value.
  // If not found, return false and do not set |aValue|.
  static bool FindKeyword(nsCSSKeyword aKeyword, const KTableEntry aTable[],
                          int32_t& aValue);
  // Return the first keyword in |aTable| that has the corresponding value |aValue|.
  // Return |eCSSKeyword_UNKNOWN| if not found.
  static nsCSSKeyword ValueToKeywordEnum(int32_t aValue, 
                                         const KTableEntry aTable[]);
  // Ditto but as a string, return "" when not found.
  static const nsAFlatCString& ValueToKeyword(int32_t aValue,
                                              const KTableEntry aTable[]);

  static const nsStyleStructID kSIDTable[eCSSProperty_COUNT_no_shorthands];
  static const KTableEntry* const kKeywordTableTable[eCSSProperty_COUNT_no_shorthands];
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
   * flag, the returned array will have two values preceding the sentinel.
   * When called with a property that has the
   * CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING flag, the returned array
   * will have one value preceding the sentinel.
   * Otherwise it will have four values preceding the sentinel.
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
        PropHasFlags(aProperty, CSS_PROPERTY_ENABLED_IN_UA_SHEETS))
    {
      return true;
    }
    if ((aEnabled & eEnabledInChrome) &&
        PropHasFlags(aProperty, CSS_PROPERTY_ENABLED_IN_CHROME))
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
  static const KTableEntry kImageLayerOriginKTable[];
  static const KTableEntry kImageLayerPositionKTable[];
  static const KTableEntry kImageLayerRepeatKTable[];
  static const KTableEntry kImageLayerRepeatPartKTable[];
  static const KTableEntry kImageLayerSizeKTable[];
  static const KTableEntry kImageLayerCompositeKTable[];
  static const KTableEntry kImageLayerModeKTable[];
  static const KTableEntry kBlendModeKTable[];
  static const KTableEntry kBorderCollapseKTable[];
  static const KTableEntry kBorderColorKTable[];
  static const KTableEntry kBorderImageRepeatKTable[];
  static const KTableEntry kBorderImageSliceKTable[];
  static const KTableEntry kBorderStyleKTable[];
  static const KTableEntry kBorderWidthKTable[];
  static const KTableEntry kBoxAlignKTable[];
  static const KTableEntry kBoxDecorationBreakKTable[];
  static const KTableEntry kBoxDirectionKTable[];
  static const KTableEntry kBoxOrientKTable[];
  static const KTableEntry kBoxPackKTable[];
  static const KTableEntry kClipShapeSizingKTable[];
  static const KTableEntry kCounterRangeKTable[];
  static const KTableEntry kCounterSpeakAsKTable[];
  static const KTableEntry kCounterSymbolsSystemKTable[];
  static const KTableEntry kCounterSystemKTable[];
  static const KTableEntry kDominantBaselineKTable[];
  static const KTableEntry kShapeRadiusKTable[];
  static const KTableEntry kFillRuleKTable[];
  static const KTableEntry kFilterFunctionKTable[];
  static const KTableEntry kImageRenderingKTable[];
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
  static const KTableEntry kBoxPropSourceKTable[];
  static const KTableEntry kBoxShadowTypeKTable[];
  static const KTableEntry kBoxSizingKTable[];
  static const KTableEntry kCaptionSideKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.float-logical-values.enabled" changes:
  static KTableEntry kClearKTable[];
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
  static const KTableEntry kAlignAutoNormalStretchBaseline[]; // 'auto/normal/stretch/baseline/last-baseline'
  static const KTableEntry kAlignNormalStretchBaseline[]; // 'normal/stretch/baseline/last-baseline'
  static const KTableEntry kAlignNormalBaseline[]; // 'normal/baseline/last-baseline'
  static const KTableEntry kAlignContentDistribution[]; // <content-distribution>
  static const KTableEntry kAlignContentPosition[]; // <content-position>
  // -- tables for auto-completion of the {align,justify}-{content,items,self} properties --
  static const KTableEntry kAutoCompletionAlignJustifySelf[];
  static const KTableEntry kAutoCompletionAlignItems[];
  static const KTableEntry kAutoCompletionAlignJustifyContent[];
  // ------------------------------------------------------------------
  static const KTableEntry kFlexDirectionKTable[];
  static const KTableEntry kFlexWrapKTable[];
  // Not const because we modify its entries when the pref
  // "layout.css.float-logical-values.enabled" changes:
  static KTableEntry kFloatKTable[];
  static const KTableEntry kFloatEdgeKTable[];
  static const KTableEntry kFontDisplayKTable[];
  static const KTableEntry kFontKTable[];
  static const KTableEntry kFontKerningKTable[];
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
  static const KTableEntry kListStyleKTable[];
  static const KTableEntry kMaskTypeKTable[];
  static const KTableEntry kMathVariantKTable[];
  static const KTableEntry kMathDisplayKTable[];
  static const KTableEntry kContainKTable[];
  static const KTableEntry kContextOpacityKTable[];
  static const KTableEntry kContextPatternKTable[];
  static const KTableEntry kObjectFitKTable[];
  static const KTableEntry kOrientKTable[];
  static const KTableEntry kOutlineStyleKTable[];
  static const KTableEntry kOutlineColorKTable[];
  static const KTableEntry kOverflowKTable[];
  static const KTableEntry kOverflowSubKTable[];
  static const KTableEntry kOverflowClipBoxKTable[];
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
  static const KTableEntry kTextOrientationKTable[];
  static const KTableEntry kTextOverflowKTable[];
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
  static const KTableEntry kWindowDraggingKTable[];
  static const KTableEntry kWindowShadowKTable[];
  static const KTableEntry kWordBreakKTable[];
  static const KTableEntry kWordWrapKTable[];
  static const KTableEntry kWritingModeKTable[];
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
