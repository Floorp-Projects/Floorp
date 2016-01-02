/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * methods for dealing with CSS properties and tables of the keyword
 * values they accept
 */

#include "mozilla/ArrayUtils.h"

#include "nsCSSProps.h"
#include "nsCSSKeywords.h"
#include "nsLayoutUtils.h"
#include "nsStyleConsts.h"
#include "nsIWidget.h"
#include "nsThemeConstants.h"  // For system widget appearance types

#include "mozilla/dom/AnimationEffectReadOnlyBinding.h" // for PlaybackDirection
#include "mozilla/LookAndFeel.h" // for system colors

#include "nsString.h"
#include "nsStaticNameTable.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

typedef nsCSSProps::KTableEntry KTableEntry;

// MSVC before 2015 doesn't consider string literal as a constant
// expression, thus we are not able to do this check here.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
// By wrapping internal-only properties in this macro, we are not
// exposing them in the CSSOM. Since currently it is not necessary to
// allow accessing them in that way, it is easier and cheaper to just
// do this rather than exposing them conditionally.
#define CSS_PROP(name_, id_, method_, flags_, pref_, ...) \
  static_assert(!((flags_) & CSS_PROPERTY_ENABLED_MASK) || pref_[0], \
                "Internal-only property '" #name_ "' should be wrapped in " \
                "#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL");
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#define CSS_PROP_LIST_EXCLUDE_INTERNAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_EXCLUDE_INTERNAL
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#endif

#define CSS_PROP(name_, id_, method_, flags_, pref_, ...) \
  static_assert(!((flags_) & CSS_PROPERTY_ENABLED_IN_CHROME) || \
                ((flags_) & CSS_PROPERTY_ENABLED_IN_UA_SHEETS), \
                "Property '" #name_ "' is enabled in chrome, so it should " \
                "also be enabled in UA sheets");
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP

// required to make the symbol external, so that TestCSSPropertyLookup.cpp can link with it
extern const char* const kCSSRawProperties[];

// define an array of all CSS properties
const char* const kCSSRawProperties[eCSSProperty_COUNT_with_aliases] = {
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  #name_,
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) #name_,
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
#define CSS_PROP_ALIAS(aliasname_, id_, method_, pref_) #aliasname_,
#include "nsCSSPropAliasList.h"
#undef CSS_PROP_ALIAS
};

using namespace mozilla;

static int32_t gPropertyTableRefCount;
static nsStaticCaseInsensitiveNameTable* gPropertyTable;
static nsStaticCaseInsensitiveNameTable* gFontDescTable;
static nsStaticCaseInsensitiveNameTable* gCounterDescTable;
static nsStaticCaseInsensitiveNameTable* gPredefinedCounterStyleTable;
static nsDataHashtable<nsCStringHashKey,nsCSSProperty>* gPropertyIDLNameTable;

/* static */ nsCSSProperty *
  nsCSSProps::gShorthandsContainingTable[eCSSProperty_COUNT_no_shorthands];
/* static */ nsCSSProperty* nsCSSProps::gShorthandsContainingPool = nullptr;

static const char* const kCSSRawFontDescs[] = {
#define CSS_FONT_DESC(name_, method_) #name_,
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC
};

static const char* const kCSSRawCounterDescs[] = {
#define CSS_COUNTER_DESC(name_, method_) #name_,
#include "nsCSSCounterDescList.h"
#undef CSS_COUNTER_DESC
};

static const char* const kCSSRawPredefinedCounterStyles[] = {
  "none",
  // 6 Simple Predefined Counter Styles
  // 6.1 Numeric
  "decimal", "decimal-leading-zero", "arabic-indic", "armenian",
  "upper-armenian", "lower-armenian", "bengali", "cambodian", "khmer",
  "cjk-decimal", "devanagari", "georgian", "gujarati", "gurmukhi", "hebrew",
  "kannada", "lao", "malayalam", "mongolian", "myanmar", "oriya", "persian",
  "lower-roman", "upper-roman", "tamil", "telugu", "thai", "tibetan",
  // 6.2 Alphabetic
  "lower-alpha", "lower-latin", "upper-alpha", "upper-latin",
  "cjk-earthly-branch", "cjk-heavenly-stem", "lower-greek",
  "hiragana", "hiragana-iroha", "katakana", "katakana-iroha",
  // 6.3 Symbolic
  "disc", "circle", "square", "disclosure-open", "disclosure-closed",
  // 7 Complex Predefined Counter Styles
  // 7.1 Longhand East Asian Counter Styles
  // 7.1.1 Japanese
  "japanese-informal", "japanese-formal",
  // 7.1.2 Korean
  "korean-hangul-formal", "korean-hanja-informal", "korean-hanja-formal",
  // 7.1.3 Chinese
  "simp-chinese-informal", "simp-chinese-formal",
  "trad-chinese-informal", "trad-chinese-formal", "cjk-ideographic",
  // 7.2 Ethiopic Numeric Counter Style
  "ethiopic-numeric"
};

struct PropertyAndCount {
  nsCSSProperty property;
  uint32_t count;
};

static int
SortPropertyAndCount(const void* s1, const void* s2, void *closure)
{
  const PropertyAndCount *pc1 = static_cast<const PropertyAndCount*>(s1);
  const PropertyAndCount *pc2 = static_cast<const PropertyAndCount*>(s2);
  // Primary sort by count (lowest to highest)
  if (pc1->count != pc2->count)
    return pc1->count - pc2->count;
  // Secondary sort by property index (highest to lowest)
  return pc2->property - pc1->property;
}

// We need eCSSAliasCount so we can make gAliases nonzero size when there
// are no aliases.
enum {
  eCSSAliasCount = eCSSProperty_COUNT_with_aliases - eCSSProperty_COUNT
};

// The names are in kCSSRawProperties.
static nsCSSProperty gAliases[eCSSAliasCount != 0 ? eCSSAliasCount : 1] = {
#define CSS_PROP_ALIAS(aliasname_, propid_, aliasmethod_, pref_)  \
  eCSSProperty_##propid_ ,
#include "nsCSSPropAliasList.h"
#undef CSS_PROP_ALIAS
};

nsStaticCaseInsensitiveNameTable*
CreateStaticTable(const char* const aRawTable[], int32_t aLength)
{
  auto table = new nsStaticCaseInsensitiveNameTable(aRawTable, aLength);
#ifdef DEBUG
  // Partially verify the entries.
  for (int32_t index = 0; index < aLength; ++index) {
    nsAutoCString temp(aRawTable[index]);
    MOZ_ASSERT(-1 == temp.FindChar('_'),
               "underscore char in case insensitive name table");
  }
#endif
  return table;
}

void
nsCSSProps::AddRefTable(void)
{
  if (0 == gPropertyTableRefCount++) {
    MOZ_ASSERT(!gPropertyTable, "pre existing array!");
    MOZ_ASSERT(!gFontDescTable, "pre existing array!");
    MOZ_ASSERT(!gCounterDescTable, "pre existing array!");
    MOZ_ASSERT(!gPredefinedCounterStyleTable, "pre existing array!");
    MOZ_ASSERT(!gPropertyIDLNameTable, "pre existing array!");

    gPropertyTable = CreateStaticTable(
        kCSSRawProperties, eCSSProperty_COUNT_with_aliases);
    gFontDescTable = CreateStaticTable(kCSSRawFontDescs, eCSSFontDesc_COUNT);
    gCounterDescTable = CreateStaticTable(
        kCSSRawCounterDescs, eCSSCounterDesc_COUNT);
    gPredefinedCounterStyleTable = CreateStaticTable(
        kCSSRawPredefinedCounterStyles,
        ArrayLength(kCSSRawPredefinedCounterStyles));

    gPropertyIDLNameTable = new nsDataHashtable<nsCStringHashKey,nsCSSProperty>;
    for (nsCSSProperty p = nsCSSProperty(0);
         size_t(p) < ArrayLength(kIDLNameTable);
         p = nsCSSProperty(p + 1)) {
      if (kIDLNameTable[p]) {
        gPropertyIDLNameTable->Put(nsDependentCString(kIDLNameTable[p]), p);
      }
    }

    BuildShorthandsContainingTable();

    static bool prefObserversInited = false;
    if (!prefObserversInited) {
      prefObserversInited = true;
      
      #define OBSERVE_PROP(pref_, id_)                                        \
        if (pref_[0]) {                                                       \
          Preferences::AddBoolVarCache(&gPropertyEnabled[id_],                \
                                       pref_);                                \
        }

      #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                       kwtable_, stylestruct_, stylestructoffset_, animtype_) \
        OBSERVE_PROP(pref_, eCSSProperty_##id_)
      #define CSS_PROP_LIST_INCLUDE_LOGICAL
      #include "nsCSSPropList.h"
      #undef CSS_PROP_LIST_INCLUDE_LOGICAL
      #undef CSS_PROP

      #define  CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) \
        OBSERVE_PROP(pref_, eCSSProperty_##id_)
      #include "nsCSSPropList.h"
      #undef CSS_PROP_SHORTHAND

      #define CSS_PROP_ALIAS(aliasname_, propid_, aliasmethod_, pref_)    \
        OBSERVE_PROP(pref_, eCSSPropertyAlias_##aliasmethod_)
      #include "nsCSSPropAliasList.h"
      #undef CSS_PROP_ALIAS

      #undef OBSERVE_PROP
    }

#ifdef DEBUG
    {
      // Assert that if CSS_PROPERTY_ENABLED_IN_UA_SHEETS or
      // CSS_PROPERTY_ENABLED_IN_CHROME is used on a shorthand property
      // that all of its component longhands also have the flag.
      static uint32_t flagsToCheck[] = {
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
        CSS_PROPERTY_ENABLED_IN_CHROME
      };
      for (nsCSSProperty shorthand = eCSSProperty_COUNT_no_shorthands;
           shorthand < eCSSProperty_COUNT;
           shorthand = nsCSSProperty(shorthand + 1)) {
        for (size_t i = 0; i < ArrayLength(flagsToCheck); i++) {
          uint32_t flag = flagsToCheck[i];
          if (!nsCSSProps::PropHasFlags(shorthand, flag)) {
            continue;
          }
          for (const nsCSSProperty* p =
                 nsCSSProps::SubpropertyEntryFor(shorthand);
               *p != eCSSProperty_UNKNOWN;
               ++p) {
            MOZ_ASSERT(nsCSSProps::PropHasFlags(*p, flag),
                       "all subproperties of a property with a "
                       "CSS_PROPERTY_ENABLED_* flag must also have "
                       "the flag");
          }
        }
      }

      // Assert that CSS_PROPERTY_INTERNAL is used on properties in
      // #ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL sections of nsCSSPropList.h
      // and on no others.
      static nsCSSProperty nonInternalProperties[] = {
        #define CSS_PROP(name_, id_, ...)           eCSSProperty_##id_,
        #define CSS_PROP_SHORTHAND(name_, id_, ...) eCSSProperty_##id_,
        #define CSS_PROP_LIST_INCLUDE_LOGICAL
        #define CSS_PROP_LIST_EXCLUDE_INTERNAL
        #include "nsCSSPropList.h"
        #undef CSS_PROP_LIST_EXCLUDE_INTERNAL
        #undef CSS_PROP_LIST_INCLUDE_LOGICAL
        #undef CSS_PROP_SHORTHAND
        #undef CSS_PROP
      };
      MOZ_ASSERT(ArrayLength(nonInternalProperties) <= eCSSProperty_COUNT);

      bool found[eCSSProperty_COUNT];
      PodArrayZero(found);
      for (nsCSSProperty p : nonInternalProperties) {
        MOZ_ASSERT(!nsCSSProps::PropHasFlags(p, CSS_PROPERTY_INTERNAL),
                   "properties defined outside of #ifndef "
                   "CSS_PROP_LIST_EXCLUDE_INTERNAL sections must not have "
                   "the CSS_PROPERTY_INTERNAL flag");
        found[p] = true;
      }

      for (size_t i = 0; i < ArrayLength(found); ++i) {
        if (!found[i]) {
          auto p = static_cast<nsCSSProperty>(i);
          MOZ_ASSERT(nsCSSProps::PropHasFlags(p, CSS_PROPERTY_INTERNAL),
                     "properties defined in #ifndef "
                     "CSS_PROP_LIST_EXCLUDE_INTERNAL sections must have "
                     "the CSS_PROPERTY_INTERNAL flag");
        }
      }
    }
#endif
  }
}

#undef  DEBUG_SHORTHANDS_CONTAINING

bool
nsCSSProps::BuildShorthandsContainingTable()
{
  uint32_t occurrenceCounts[eCSSProperty_COUNT_no_shorthands];
  memset(occurrenceCounts, 0, sizeof(occurrenceCounts));
  PropertyAndCount subpropCounts[eCSSProperty_COUNT -
                                   eCSSProperty_COUNT_no_shorthands];
  for (nsCSSProperty shorthand = eCSSProperty_COUNT_no_shorthands;
       shorthand < eCSSProperty_COUNT;
       shorthand = nsCSSProperty(shorthand + 1)) {
#ifdef DEBUG_SHORTHANDS_CONTAINING
    printf("Considering shorthand property '%s'.\n",
           nsCSSProps::GetStringValue(shorthand).get());
#endif
    PropertyAndCount &subpropCountsEntry =
      subpropCounts[shorthand - eCSSProperty_COUNT_no_shorthands];
    subpropCountsEntry.property = shorthand;
    subpropCountsEntry.count = 0;
    if (nsCSSProps::PropHasFlags(shorthand, CSS_PROPERTY_IS_ALIAS)) {
      // Don't put shorthands that are acting as aliases in the
      // shorthands-containing lists.
      continue;
    }
    for (const nsCSSProperty* subprops = SubpropertyEntryFor(shorthand);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      MOZ_ASSERT(0 <= *subprops && *subprops < eCSSProperty_COUNT_no_shorthands,
                 "subproperty must be a longhand");
      ++occurrenceCounts[*subprops];
      ++subpropCountsEntry.count;
    }
  }

  uint32_t poolEntries = 0;
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    uint32_t count = occurrenceCounts[longhand];
    if (count > 0)
      // leave room for terminator
      poolEntries += count + 1;
  }

  gShorthandsContainingPool = new nsCSSProperty[poolEntries];
  if (!gShorthandsContainingPool)
    return false;

  // Initialize all entries to point to their null-terminator.
  {
    nsCSSProperty *poolCursor = gShorthandsContainingPool - 1;
    nsCSSProperty *lastTerminator =
      gShorthandsContainingPool + poolEntries - 1;
    for (nsCSSProperty longhand = nsCSSProperty(0);
         longhand < eCSSProperty_COUNT_no_shorthands;
         longhand = nsCSSProperty(longhand + 1)) {
      uint32_t count = occurrenceCounts[longhand];
      if (count > 0) {
        poolCursor += count + 1;
        gShorthandsContainingTable[longhand] = poolCursor;
        *poolCursor = eCSSProperty_UNKNOWN;
      } else {
        gShorthandsContainingTable[longhand] = lastTerminator;
      }
    }
    MOZ_ASSERT(poolCursor == lastTerminator, "miscalculation");
  }

  // Sort with lowest count at the start and highest at the end, and
  // within counts sort in reverse property index order.
  NS_QuickSort(&subpropCounts, ArrayLength(subpropCounts),
               sizeof(subpropCounts[0]), SortPropertyAndCount, nullptr);

  // Fill in all the entries in gShorthandsContainingTable
  for (const PropertyAndCount *shorthandAndCount = subpropCounts,
                           *shorthandAndCountEnd = ArrayEnd(subpropCounts);
       shorthandAndCount < shorthandAndCountEnd;
       ++shorthandAndCount) {
#ifdef DEBUG_SHORTHANDS_CONTAINING
    printf("Entering %u subprops for '%s'.\n",
           shorthandAndCount->count,
           nsCSSProps::GetStringValue(shorthandAndCount->property).get());
#endif
    if (nsCSSProps::PropHasFlags(shorthandAndCount->property,
                                 CSS_PROPERTY_IS_ALIAS)) {
      // Don't put shorthands that are acting as aliases in the
      // shorthands-containing lists.
      continue;
    }
    for (const nsCSSProperty* subprops =
           SubpropertyEntryFor(shorthandAndCount->property);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      *(--gShorthandsContainingTable[*subprops]) = shorthandAndCount->property;
    }
  }

#ifdef DEBUG_SHORTHANDS_CONTAINING
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    printf("Property %s is in %d shorthands.\n",
           nsCSSProps::GetStringValue(longhand).get(),
           occurrenceCounts[longhand]);
    for (const nsCSSProperty *shorthands = ShorthandsContaining(longhand);
         *shorthands != eCSSProperty_UNKNOWN;
         ++shorthands) {
      printf("  %s\n", nsCSSProps::GetStringValue(*shorthands).get());
    }
  }
#endif

#ifdef DEBUG
  // Verify that all values that should be are present.
  for (nsCSSProperty shorthand = eCSSProperty_COUNT_no_shorthands;
       shorthand < eCSSProperty_COUNT;
       shorthand = nsCSSProperty(shorthand + 1)) {
    if (nsCSSProps::PropHasFlags(shorthand, CSS_PROPERTY_IS_ALIAS)) {
      // Don't put shorthands that are acting as aliases in the
      // shorthands-containing lists.
      continue;
    }
    for (const nsCSSProperty* subprops = SubpropertyEntryFor(shorthand);
         *subprops != eCSSProperty_UNKNOWN;
         ++subprops) {
      uint32_t count = 0;
      for (const nsCSSProperty *shcont = ShorthandsContaining(*subprops);
           *shcont != eCSSProperty_UNKNOWN;
           ++shcont) {
        if (*shcont == shorthand)
          ++count;
      }
      MOZ_ASSERT(count == 1,
                 "subproperty of shorthand should have shorthand"
                 " in its ShorthandsContaining() table");
    }
  }

  // Verify that there are no extra values
  for (nsCSSProperty longhand = nsCSSProperty(0);
       longhand < eCSSProperty_COUNT_no_shorthands;
       longhand = nsCSSProperty(longhand + 1)) {
    for (const nsCSSProperty *shorthands = ShorthandsContaining(longhand);
         *shorthands != eCSSProperty_UNKNOWN;
         ++shorthands) {
      uint32_t count = 0;
      for (const nsCSSProperty* subprops = SubpropertyEntryFor(*shorthands);
           *subprops != eCSSProperty_UNKNOWN;
           ++subprops) {
        if (*subprops == longhand)
          ++count;
      }
      MOZ_ASSERT(count == 1,
                 "longhand should be in subproperty table of "
                 "property in its ShorthandsContaining() table");
    }
  }
#endif

  return true;
}

void
nsCSSProps::ReleaseTable(void)
{
  if (0 == --gPropertyTableRefCount) {
    delete gPropertyTable;
    gPropertyTable = nullptr;

    delete gFontDescTable;
    gFontDescTable = nullptr;

    delete gCounterDescTable;
    gCounterDescTable = nullptr;

    delete gPredefinedCounterStyleTable;
    gPredefinedCounterStyleTable = nullptr;

    delete gPropertyIDLNameTable;
    gPropertyIDLNameTable = nullptr;

    delete [] gShorthandsContainingPool;
    gShorthandsContainingPool = nullptr;
  }
}

/* static */ bool
nsCSSProps::IsInherited(nsCSSProperty aProperty)
{
  MOZ_ASSERT(!IsShorthand(aProperty));

  nsStyleStructID sid = kSIDTable[aProperty];
  return nsCachedStyleData::IsInherited(sid);
}

/* static */ bool
nsCSSProps::IsCustomPropertyName(const nsACString& aProperty)
{
  // Custom properties don't need to have a character after the "--" prefix.
  return aProperty.Length() >= CSS_CUSTOM_NAME_PREFIX_LENGTH &&
         StringBeginsWith(aProperty, NS_LITERAL_CSTRING("--"));
}

/* static */ bool
nsCSSProps::IsCustomPropertyName(const nsAString& aProperty)
{
  return aProperty.Length() >= CSS_CUSTOM_NAME_PREFIX_LENGTH &&
         StringBeginsWith(aProperty, NS_LITERAL_STRING("--"));
}

nsCSSProperty
nsCSSProps::LookupProperty(const nsACString& aProperty,
                           EnabledState aEnabled)
{
  MOZ_ASSERT(gPropertyTable, "no lookup table, needs addref");

  if (nsLayoutUtils::CSSVariablesEnabled() &&
      IsCustomPropertyName(aProperty)) {
    return eCSSPropertyExtra_variable;
  }

  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (MOZ_LIKELY(res < eCSSProperty_COUNT)) {
    if (res != eCSSProperty_UNKNOWN && !IsEnabled(res, aEnabled)) {
      res = eCSSProperty_UNKNOWN;
    }
    return res;
  }
  MOZ_ASSERT(eCSSAliasCount != 0,
             "'res' must be an alias at this point so we better have some!");
  // We intentionally don't support eEnabledInUASheets or eEnabledInChrome
  // for aliases yet because it's unlikely there will be a need for it.
  if (IsEnabled(res) || aEnabled == eIgnoreEnabledState) {
    res = gAliases[res - eCSSProperty_COUNT];
    MOZ_ASSERT(0 <= res && res < eCSSProperty_COUNT,
               "aliases must not point to other aliases");
    if (IsEnabled(res) || aEnabled == eIgnoreEnabledState) {
      return res;
    }
  }
  return eCSSProperty_UNKNOWN;
}

nsCSSProperty
nsCSSProps::LookupProperty(const nsAString& aProperty, EnabledState aEnabled)
{
  if (nsLayoutUtils::CSSVariablesEnabled() &&
      IsCustomPropertyName(aProperty)) {
    return eCSSPropertyExtra_variable;
  }

  // This is faster than converting and calling
  // LookupProperty(nsACString&).  The table will do its own
  // converting and avoid a PromiseFlatCString() call.
  MOZ_ASSERT(gPropertyTable, "no lookup table, needs addref");
  nsCSSProperty res = nsCSSProperty(gPropertyTable->Lookup(aProperty));
  if (MOZ_LIKELY(res < eCSSProperty_COUNT)) {
    if (res != eCSSProperty_UNKNOWN && !IsEnabled(res, aEnabled)) {
      res = eCSSProperty_UNKNOWN;
    }
    return res;
  }
  MOZ_ASSERT(eCSSAliasCount != 0,
             "'res' must be an alias at this point so we better have some!");
  // We intentionally don't support eEnabledInUASheets or eEnabledInChrome
  // for aliases yet because it's unlikely there will be a need for it.
  if (IsEnabled(res) || aEnabled == eIgnoreEnabledState) {
    res = gAliases[res - eCSSProperty_COUNT];
    MOZ_ASSERT(0 <= res && res < eCSSProperty_COUNT,
               "aliases must not point to other aliases");
    if (IsEnabled(res) || aEnabled == eIgnoreEnabledState) {
      return res;
    }
  }
  return eCSSProperty_UNKNOWN;
}

nsCSSProperty
nsCSSProps::LookupPropertyByIDLName(const nsACString& aPropertyIDLName,
                                    EnabledState aEnabled)
{
  nsCSSProperty res;
  if (!gPropertyIDLNameTable->Get(aPropertyIDLName, &res)) {
    return eCSSProperty_UNKNOWN;
  }
  MOZ_ASSERT(res < eCSSProperty_COUNT);
  if (!IsEnabled(res, aEnabled)) {
    return eCSSProperty_UNKNOWN;
  }
  return res;
}

nsCSSProperty
nsCSSProps::LookupPropertyByIDLName(const nsAString& aPropertyIDLName,
                                    EnabledState aEnabled)
{
  MOZ_ASSERT(gPropertyIDLNameTable, "no lookup table, needs addref");
  return LookupPropertyByIDLName(NS_ConvertUTF16toUTF8(aPropertyIDLName),
                                 aEnabled);
}

nsCSSFontDesc
nsCSSProps::LookupFontDesc(const nsACString& aFontDesc)
{
  MOZ_ASSERT(gFontDescTable, "no lookup table, needs addref");
  return nsCSSFontDesc(gFontDescTable->Lookup(aFontDesc));
}

nsCSSFontDesc
nsCSSProps::LookupFontDesc(const nsAString& aFontDesc)
{
  MOZ_ASSERT(gFontDescTable, "no lookup table, needs addref");
  nsCSSFontDesc which = nsCSSFontDesc(gFontDescTable->Lookup(aFontDesc));

  // check for unprefixed font-feature-settings/font-language-override
  if (which == eCSSFontDesc_UNKNOWN) {
    nsAutoString prefixedProp;
    prefixedProp.AppendLiteral("-moz-");
    prefixedProp.Append(aFontDesc);
    which = nsCSSFontDesc(gFontDescTable->Lookup(prefixedProp));
  }
  return which;
}

nsCSSCounterDesc
nsCSSProps::LookupCounterDesc(const nsAString& aProperty)
{
  MOZ_ASSERT(gCounterDescTable, "no lookup table, needs addref");
  return nsCSSCounterDesc(gCounterDescTable->Lookup(aProperty));
}

nsCSSCounterDesc
nsCSSProps::LookupCounterDesc(const nsACString& aProperty)
{
  MOZ_ASSERT(gCounterDescTable, "no lookup table, needs addref");
  return nsCSSCounterDesc(gCounterDescTable->Lookup(aProperty));
}

bool
nsCSSProps::IsPredefinedCounterStyle(const nsAString& aStyle)
{
  MOZ_ASSERT(gPredefinedCounterStyleTable,
             "no lookup table, needs addref");
  return gPredefinedCounterStyleTable->Lookup(aStyle) !=
    nsStaticCaseInsensitiveNameTable::NOT_FOUND;
}

bool
nsCSSProps::IsPredefinedCounterStyle(const nsACString& aStyle)
{
  MOZ_ASSERT(gPredefinedCounterStyleTable,
             "no lookup table, needs addref");
  return gPredefinedCounterStyleTable->Lookup(aStyle) !=
    nsStaticCaseInsensitiveNameTable::NOT_FOUND;
}

const nsAFlatCString&
nsCSSProps::GetStringValue(nsCSSProperty aProperty)
{
  MOZ_ASSERT(gPropertyTable, "no lookup table, needs addref");
  if (gPropertyTable) {
    return gPropertyTable->GetStringValue(int32_t(aProperty));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

const nsAFlatCString&
nsCSSProps::GetStringValue(nsCSSFontDesc aFontDescID)
{
  MOZ_ASSERT(gFontDescTable, "no lookup table, needs addref");
  if (gFontDescTable) {
    return gFontDescTable->GetStringValue(int32_t(aFontDescID));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

const nsAFlatCString&
nsCSSProps::GetStringValue(nsCSSCounterDesc aCounterDesc)
{
  MOZ_ASSERT(gCounterDescTable, "no lookup table, needs addref");
  if (gCounterDescTable) {
    return gCounterDescTable->GetStringValue(int32_t(aCounterDesc));
  } else {
    static nsDependentCString sNullStr("");
    return sNullStr;
  }
}

/***************************************************************************/

const KTableEntry nsCSSProps::kAnimationDirectionKTable[] = {
  { eCSSKeyword_normal, static_cast<uint32_t>(dom::PlaybackDirection::Normal) },
  { eCSSKeyword_reverse, static_cast<uint32_t>(dom::PlaybackDirection::Reverse) },
  { eCSSKeyword_alternate, static_cast<uint32_t>(dom::PlaybackDirection::Alternate) },
  { eCSSKeyword_alternate_reverse, static_cast<uint32_t>(dom::PlaybackDirection::Alternate_reverse) },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kAnimationFillModeKTable[] = {
  { eCSSKeyword_none, static_cast<uint32_t>(dom::FillMode::None) },
  { eCSSKeyword_forwards, static_cast<uint32_t>(dom::FillMode::Forwards) },
  { eCSSKeyword_backwards, static_cast<uint32_t>(dom::FillMode::Backwards) },
  { eCSSKeyword_both, static_cast<uint32_t>(dom::FillMode::Both) },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kAnimationIterationCountKTable[] = {
  { eCSSKeyword_infinite, NS_STYLE_ANIMATION_ITERATION_COUNT_INFINITE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kAnimationPlayStateKTable[] = {
  { eCSSKeyword_running, NS_STYLE_ANIMATION_PLAY_STATE_RUNNING },
  { eCSSKeyword_paused, NS_STYLE_ANIMATION_PLAY_STATE_PAUSED },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kAppearanceKTable[] = {
  { eCSSKeyword_none,                   NS_THEME_NONE },
  { eCSSKeyword_button,                 NS_THEME_BUTTON },
  { eCSSKeyword_radio,                  NS_THEME_RADIO },
  { eCSSKeyword_checkbox,               NS_THEME_CHECKBOX },
  { eCSSKeyword_button_bevel,           NS_THEME_BUTTON_BEVEL },
  { eCSSKeyword_toolbox,                NS_THEME_TOOLBOX },
  { eCSSKeyword_toolbar,                NS_THEME_TOOLBAR },
  { eCSSKeyword_toolbarbutton,          NS_THEME_TOOLBAR_BUTTON },
  { eCSSKeyword_toolbargripper,         NS_THEME_TOOLBAR_GRIPPER },
  { eCSSKeyword_dualbutton,             NS_THEME_TOOLBAR_DUAL_BUTTON },
  { eCSSKeyword_toolbarbutton_dropdown, NS_THEME_TOOLBAR_BUTTON_DROPDOWN },
  { eCSSKeyword_button_arrow_up,        NS_THEME_BUTTON_ARROW_UP },
  { eCSSKeyword_button_arrow_down,      NS_THEME_BUTTON_ARROW_DOWN },
  { eCSSKeyword_button_arrow_next,      NS_THEME_BUTTON_ARROW_NEXT },
  { eCSSKeyword_button_arrow_previous,  NS_THEME_BUTTON_ARROW_PREVIOUS },
  { eCSSKeyword_meterbar,               NS_THEME_METERBAR },
  { eCSSKeyword_meterchunk,             NS_THEME_METERBAR_CHUNK },
  { eCSSKeyword_number_input,           NS_THEME_NUMBER_INPUT },
  { eCSSKeyword_separator,              NS_THEME_TOOLBAR_SEPARATOR },
  { eCSSKeyword_splitter,               NS_THEME_SPLITTER },
  { eCSSKeyword_statusbar,              NS_THEME_STATUSBAR },
  { eCSSKeyword_statusbarpanel,         NS_THEME_STATUSBAR_PANEL },
  { eCSSKeyword_resizerpanel,           NS_THEME_STATUSBAR_RESIZER_PANEL },
  { eCSSKeyword_resizer,                NS_THEME_RESIZER },
  { eCSSKeyword_listbox,                NS_THEME_LISTBOX },
  { eCSSKeyword_listitem,               NS_THEME_LISTBOX_LISTITEM },
  { eCSSKeyword_treeview,               NS_THEME_TREEVIEW },
  { eCSSKeyword_treeitem,               NS_THEME_TREEVIEW_TREEITEM },
  { eCSSKeyword_treetwisty,             NS_THEME_TREEVIEW_TWISTY },
  { eCSSKeyword_treetwistyopen,         NS_THEME_TREEVIEW_TWISTY_OPEN },
  { eCSSKeyword_treeline,               NS_THEME_TREEVIEW_LINE },
  { eCSSKeyword_treeheader,             NS_THEME_TREEVIEW_HEADER },
  { eCSSKeyword_treeheadercell,         NS_THEME_TREEVIEW_HEADER_CELL },
  { eCSSKeyword_treeheadersortarrow,    NS_THEME_TREEVIEW_HEADER_SORTARROW },
  { eCSSKeyword_progressbar,            NS_THEME_PROGRESSBAR },
  { eCSSKeyword_progresschunk,          NS_THEME_PROGRESSBAR_CHUNK },
  { eCSSKeyword_progressbar_vertical,   NS_THEME_PROGRESSBAR_VERTICAL },
  { eCSSKeyword_progresschunk_vertical, NS_THEME_PROGRESSBAR_CHUNK_VERTICAL },
  { eCSSKeyword_tab,                    NS_THEME_TAB },
  { eCSSKeyword_tabpanels,              NS_THEME_TAB_PANELS },
  { eCSSKeyword_tabpanel,               NS_THEME_TAB_PANEL },
  { eCSSKeyword_tab_scroll_arrow_back,  NS_THEME_TAB_SCROLLARROW_BACK },
  { eCSSKeyword_tab_scroll_arrow_forward, NS_THEME_TAB_SCROLLARROW_FORWARD },
  { eCSSKeyword_tooltip,                NS_THEME_TOOLTIP },
  { eCSSKeyword_spinner,                NS_THEME_SPINNER },
  { eCSSKeyword_spinner_upbutton,       NS_THEME_SPINNER_UP_BUTTON },
  { eCSSKeyword_spinner_downbutton,     NS_THEME_SPINNER_DOWN_BUTTON },
  { eCSSKeyword_spinner_textfield,      NS_THEME_SPINNER_TEXTFIELD },
  { eCSSKeyword_scrollbar,              NS_THEME_SCROLLBAR },
  { eCSSKeyword_scrollbar_small,        NS_THEME_SCROLLBAR_SMALL },
  { eCSSKeyword_scrollbar_horizontal,   NS_THEME_SCROLLBAR_HORIZONTAL },
  { eCSSKeyword_scrollbar_vertical,     NS_THEME_SCROLLBAR_VERTICAL },
  { eCSSKeyword_scrollbarbutton_up,     NS_THEME_SCROLLBAR_BUTTON_UP },
  { eCSSKeyword_scrollbarbutton_down,   NS_THEME_SCROLLBAR_BUTTON_DOWN },
  { eCSSKeyword_scrollbarbutton_left,   NS_THEME_SCROLLBAR_BUTTON_LEFT },
  { eCSSKeyword_scrollbarbutton_right,  NS_THEME_SCROLLBAR_BUTTON_RIGHT },
  { eCSSKeyword_scrollbartrack_horizontal,    NS_THEME_SCROLLBAR_TRACK_HORIZONTAL },
  { eCSSKeyword_scrollbartrack_vertical,      NS_THEME_SCROLLBAR_TRACK_VERTICAL },
  { eCSSKeyword_scrollbarthumb_horizontal,    NS_THEME_SCROLLBAR_THUMB_HORIZONTAL },
  { eCSSKeyword_scrollbarthumb_vertical,      NS_THEME_SCROLLBAR_THUMB_VERTICAL },
  { eCSSKeyword_textfield,              NS_THEME_TEXTFIELD },
  { eCSSKeyword_textfield_multiline,    NS_THEME_TEXTFIELD_MULTILINE },
  { eCSSKeyword_caret,                  NS_THEME_TEXTFIELD_CARET },
  { eCSSKeyword_searchfield,            NS_THEME_SEARCHFIELD },
  { eCSSKeyword_menulist,               NS_THEME_DROPDOWN },
  { eCSSKeyword_menulist_button,        NS_THEME_DROPDOWN_BUTTON },
  { eCSSKeyword_menulist_text,          NS_THEME_DROPDOWN_TEXT },
  { eCSSKeyword_menulist_textfield,     NS_THEME_DROPDOWN_TEXTFIELD },
  { eCSSKeyword_range,                  NS_THEME_RANGE },
  { eCSSKeyword_range_thumb,            NS_THEME_RANGE_THUMB },
  { eCSSKeyword_scale_horizontal,       NS_THEME_SCALE_HORIZONTAL },
  { eCSSKeyword_scale_vertical,         NS_THEME_SCALE_VERTICAL },
  { eCSSKeyword_scalethumb_horizontal,  NS_THEME_SCALE_THUMB_HORIZONTAL },
  { eCSSKeyword_scalethumb_vertical,    NS_THEME_SCALE_THUMB_VERTICAL },
  { eCSSKeyword_scalethumbstart,        NS_THEME_SCALE_THUMB_START },
  { eCSSKeyword_scalethumbend,          NS_THEME_SCALE_THUMB_END },
  { eCSSKeyword_scalethumbtick,         NS_THEME_SCALE_TICK },
  { eCSSKeyword_groupbox,               NS_THEME_GROUPBOX },
  { eCSSKeyword_checkbox_container,     NS_THEME_CHECKBOX_CONTAINER },
  { eCSSKeyword_radio_container,        NS_THEME_RADIO_CONTAINER },
  { eCSSKeyword_checkbox_label,         NS_THEME_CHECKBOX_LABEL },
  { eCSSKeyword_radio_label,            NS_THEME_RADIO_LABEL },
  { eCSSKeyword_button_focus,           NS_THEME_BUTTON_FOCUS },
  { eCSSKeyword_window,                 NS_THEME_WINDOW },
  { eCSSKeyword_dialog,                 NS_THEME_DIALOG },
  { eCSSKeyword_menubar,                NS_THEME_MENUBAR },
  { eCSSKeyword_menupopup,              NS_THEME_MENUPOPUP },
  { eCSSKeyword_menuitem,               NS_THEME_MENUITEM },
  { eCSSKeyword_checkmenuitem,          NS_THEME_CHECKMENUITEM },
  { eCSSKeyword_radiomenuitem,          NS_THEME_RADIOMENUITEM },
  { eCSSKeyword_menucheckbox,           NS_THEME_MENUCHECKBOX },
  { eCSSKeyword_menuradio,              NS_THEME_MENURADIO },
  { eCSSKeyword_menuseparator,          NS_THEME_MENUSEPARATOR },
  { eCSSKeyword_menuarrow,              NS_THEME_MENUARROW },
  { eCSSKeyword_menuimage,              NS_THEME_MENUIMAGE },
  { eCSSKeyword_menuitemtext,           NS_THEME_MENUITEMTEXT },
  { eCSSKeyword__moz_win_media_toolbox, NS_THEME_WIN_MEDIA_TOOLBOX },
  { eCSSKeyword__moz_win_communications_toolbox, NS_THEME_WIN_COMMUNICATIONS_TOOLBOX },
  { eCSSKeyword__moz_win_browsertabbar_toolbox,  NS_THEME_WIN_BROWSER_TAB_BAR_TOOLBOX },
  { eCSSKeyword__moz_win_glass,         NS_THEME_WIN_GLASS },
  { eCSSKeyword__moz_win_borderless_glass,      NS_THEME_WIN_BORDERLESS_GLASS },
  { eCSSKeyword__moz_mac_fullscreen_button,     NS_THEME_MOZ_MAC_FULLSCREEN_BUTTON },
  { eCSSKeyword__moz_mac_help_button,           NS_THEME_MOZ_MAC_HELP_BUTTON },
  { eCSSKeyword__moz_window_titlebar,           NS_THEME_WINDOW_TITLEBAR },
  { eCSSKeyword__moz_window_titlebar_maximized, NS_THEME_WINDOW_TITLEBAR_MAXIMIZED },
  { eCSSKeyword__moz_window_frame_left,         NS_THEME_WINDOW_FRAME_LEFT },
  { eCSSKeyword__moz_window_frame_right,        NS_THEME_WINDOW_FRAME_RIGHT },
  { eCSSKeyword__moz_window_frame_bottom,       NS_THEME_WINDOW_FRAME_BOTTOM },
  { eCSSKeyword__moz_window_button_close,       NS_THEME_WINDOW_BUTTON_CLOSE },
  { eCSSKeyword__moz_window_button_minimize,    NS_THEME_WINDOW_BUTTON_MINIMIZE },
  { eCSSKeyword__moz_window_button_maximize,    NS_THEME_WINDOW_BUTTON_MAXIMIZE },
  { eCSSKeyword__moz_window_button_restore,     NS_THEME_WINDOW_BUTTON_RESTORE },
  { eCSSKeyword__moz_window_button_box,         NS_THEME_WINDOW_BUTTON_BOX },
  { eCSSKeyword__moz_window_button_box_maximized, NS_THEME_WINDOW_BUTTON_BOX_MAXIMIZED },
  { eCSSKeyword__moz_win_exclude_glass,         NS_THEME_WIN_EXCLUDE_GLASS },
  { eCSSKeyword__moz_mac_vibrancy_light,        NS_THEME_MAC_VIBRANCY_LIGHT },
  { eCSSKeyword__moz_mac_vibrancy_dark,         NS_THEME_MAC_VIBRANCY_DARK },
  { eCSSKeyword__moz_mac_disclosure_button_open,   NS_THEME_MAC_DISCLOSURE_BUTTON_OPEN },
  { eCSSKeyword__moz_mac_disclosure_button_closed, NS_THEME_MAC_DISCLOSURE_BUTTON_CLOSED },
  { eCSSKeyword__moz_gtk_info_bar,              NS_THEME_GTK_INFO_BAR },
  { eCSSKeyword_UNKNOWN,                        -1 }
};

const KTableEntry nsCSSProps::kBackfaceVisibilityKTable[] = {
  { eCSSKeyword_visible, NS_STYLE_BACKFACE_VISIBILITY_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_BACKFACE_VISIBILITY_HIDDEN },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTransformStyleKTable[] = {
  { eCSSKeyword_flat, NS_STYLE_TRANSFORM_STYLE_FLAT },
  { eCSSKeyword_preserve_3d, NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBackgroundAttachmentKTable[] = {
  { eCSSKeyword_fixed, NS_STYLE_BG_ATTACHMENT_FIXED },
  { eCSSKeyword_scroll, NS_STYLE_BG_ATTACHMENT_SCROLL },
  { eCSSKeyword_local, NS_STYLE_BG_ATTACHMENT_LOCAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

static_assert(NS_STYLE_BG_CLIP_BORDER == NS_STYLE_BG_ORIGIN_BORDER &&
              NS_STYLE_BG_CLIP_PADDING == NS_STYLE_BG_ORIGIN_PADDING &&
              NS_STYLE_BG_CLIP_CONTENT == NS_STYLE_BG_ORIGIN_CONTENT,
              "bg-clip and bg-origin style constants must agree");
const KTableEntry nsCSSProps::kBackgroundOriginKTable[] = {
  { eCSSKeyword_border_box, NS_STYLE_BG_ORIGIN_BORDER },
  { eCSSKeyword_padding_box, NS_STYLE_BG_ORIGIN_PADDING },
  { eCSSKeyword_content_box, NS_STYLE_BG_ORIGIN_CONTENT },
  { eCSSKeyword_UNKNOWN, -1 }
};

// Note: Don't change this table unless you update
// parseBackgroundPosition!

const KTableEntry nsCSSProps::kBackgroundPositionKTable[] = {
  { eCSSKeyword_center, NS_STYLE_BG_POSITION_CENTER },
  { eCSSKeyword_top, NS_STYLE_BG_POSITION_TOP },
  { eCSSKeyword_bottom, NS_STYLE_BG_POSITION_BOTTOM },
  { eCSSKeyword_left, NS_STYLE_BG_POSITION_LEFT },
  { eCSSKeyword_right, NS_STYLE_BG_POSITION_RIGHT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBackgroundRepeatKTable[] = {
  { eCSSKeyword_no_repeat,  NS_STYLE_BG_REPEAT_NO_REPEAT },
  { eCSSKeyword_repeat,     NS_STYLE_BG_REPEAT_REPEAT },
  { eCSSKeyword_repeat_x,   NS_STYLE_BG_REPEAT_REPEAT_X },
  { eCSSKeyword_repeat_y,   NS_STYLE_BG_REPEAT_REPEAT_Y },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBackgroundRepeatPartKTable[] = {
  { eCSSKeyword_no_repeat,  NS_STYLE_BG_REPEAT_NO_REPEAT },
  { eCSSKeyword_repeat,     NS_STYLE_BG_REPEAT_REPEAT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBackgroundSizeKTable[] = {
  { eCSSKeyword_contain, NS_STYLE_BG_SIZE_CONTAIN },
  { eCSSKeyword_cover,   NS_STYLE_BG_SIZE_COVER },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBlendModeKTable[] = {
    { eCSSKeyword_normal,      NS_STYLE_BLEND_NORMAL },
    { eCSSKeyword_multiply,    NS_STYLE_BLEND_MULTIPLY },
    { eCSSKeyword_screen,      NS_STYLE_BLEND_SCREEN },
    { eCSSKeyword_overlay,     NS_STYLE_BLEND_OVERLAY },
    { eCSSKeyword_darken,      NS_STYLE_BLEND_DARKEN },
    { eCSSKeyword_lighten,     NS_STYLE_BLEND_LIGHTEN },
    { eCSSKeyword_color_dodge, NS_STYLE_BLEND_COLOR_DODGE },
    { eCSSKeyword_color_burn,  NS_STYLE_BLEND_COLOR_BURN },
    { eCSSKeyword_hard_light,  NS_STYLE_BLEND_HARD_LIGHT },
    { eCSSKeyword_soft_light,  NS_STYLE_BLEND_SOFT_LIGHT },
    { eCSSKeyword_difference,  NS_STYLE_BLEND_DIFFERENCE },
    { eCSSKeyword_exclusion,   NS_STYLE_BLEND_EXCLUSION },
    { eCSSKeyword_hue,         NS_STYLE_BLEND_HUE },
    { eCSSKeyword_saturation,  NS_STYLE_BLEND_SATURATION },
    { eCSSKeyword_color,       NS_STYLE_BLEND_COLOR },
    { eCSSKeyword_luminosity,  NS_STYLE_BLEND_LUMINOSITY },
    { eCSSKeyword_UNKNOWN,     -1 }
};

const KTableEntry nsCSSProps::kBorderCollapseKTable[] = {
  { eCSSKeyword_collapse,  NS_STYLE_BORDER_COLLAPSE },
  { eCSSKeyword_separate,  NS_STYLE_BORDER_SEPARATE },
  { eCSSKeyword_UNKNOWN,   -1 }
};

const KTableEntry nsCSSProps::kBorderColorKTable[] = {
  { eCSSKeyword__moz_use_text_color, NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderImageRepeatKTable[] = {
  { eCSSKeyword_stretch, NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH },
  { eCSSKeyword_repeat, NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT },
  { eCSSKeyword_round, NS_STYLE_BORDER_IMAGE_REPEAT_ROUND },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderImageSliceKTable[] = {
  { eCSSKeyword_fill, NS_STYLE_BORDER_IMAGE_SLICE_FILL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderStyleKTable[] = {
  { eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE },
  { eCSSKeyword_hidden, NS_STYLE_BORDER_STYLE_HIDDEN },
  { eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED },
  { eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE },
  { eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE },
  { eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE },
  { eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET },
  { eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBorderWidthKTable[] = {
  { eCSSKeyword_thin, NS_STYLE_BORDER_WIDTH_THIN },
  { eCSSKeyword_medium, NS_STYLE_BORDER_WIDTH_MEDIUM },
  { eCSSKeyword_thick, NS_STYLE_BORDER_WIDTH_THICK },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxDecorationBreakKTable[] = {
  { eCSSKeyword_slice, NS_STYLE_BOX_DECORATION_BREAK_SLICE },
  { eCSSKeyword_clone, NS_STYLE_BOX_DECORATION_BREAK_CLONE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxShadowTypeKTable[] = {
  { eCSSKeyword_inset, NS_STYLE_BOX_SHADOW_INSET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxSizingKTable[] = {
  { eCSSKeyword_content_box,  uint8_t(StyleBoxSizing::Content) },
  { eCSSKeyword_border_box,   uint8_t(StyleBoxSizing::Border) },
  { eCSSKeyword_padding_box,  uint8_t(StyleBoxSizing::Padding) },
  { eCSSKeyword_UNKNOWN,      -1 }
};

const KTableEntry nsCSSProps::kCaptionSideKTable[] = {
  { eCSSKeyword_top,                  NS_STYLE_CAPTION_SIDE_TOP },
  { eCSSKeyword_right,                NS_STYLE_CAPTION_SIDE_RIGHT },
  { eCSSKeyword_bottom,               NS_STYLE_CAPTION_SIDE_BOTTOM },
  { eCSSKeyword_left,                 NS_STYLE_CAPTION_SIDE_LEFT },
  { eCSSKeyword_top_outside,          NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE },
  { eCSSKeyword_bottom_outside,       NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE },
  { eCSSKeyword_UNKNOWN,              -1 }
};

KTableEntry nsCSSProps::kClearKTable[] = {
  { eCSSKeyword_none,         NS_STYLE_CLEAR_NONE },
  { eCSSKeyword_left,         NS_STYLE_CLEAR_LEFT },
  { eCSSKeyword_right,        NS_STYLE_CLEAR_RIGHT },
  { eCSSKeyword_inline_start, NS_STYLE_CLEAR_INLINE_START },
  { eCSSKeyword_inline_end,   NS_STYLE_CLEAR_INLINE_END },
  { eCSSKeyword_both,         NS_STYLE_CLEAR_BOTH },
  { eCSSKeyword_UNKNOWN,      -1 }
};

// See also kContextPatternKTable for SVG paint-specific values
const KTableEntry nsCSSProps::kColorKTable[] = {
  { eCSSKeyword_activeborder, LookAndFeel::eColorID_activeborder },
  { eCSSKeyword_activecaption, LookAndFeel::eColorID_activecaption },
  { eCSSKeyword_appworkspace, LookAndFeel::eColorID_appworkspace },
  { eCSSKeyword_background, LookAndFeel::eColorID_background },
  { eCSSKeyword_buttonface, LookAndFeel::eColorID_buttonface },
  { eCSSKeyword_buttonhighlight, LookAndFeel::eColorID_buttonhighlight },
  { eCSSKeyword_buttonshadow, LookAndFeel::eColorID_buttonshadow },
  { eCSSKeyword_buttontext, LookAndFeel::eColorID_buttontext },
  { eCSSKeyword_captiontext, LookAndFeel::eColorID_captiontext },
  { eCSSKeyword_graytext, LookAndFeel::eColorID_graytext },
  { eCSSKeyword_highlight, LookAndFeel::eColorID_highlight },
  { eCSSKeyword_highlighttext, LookAndFeel::eColorID_highlighttext },
  { eCSSKeyword_inactiveborder, LookAndFeel::eColorID_inactiveborder },
  { eCSSKeyword_inactivecaption, LookAndFeel::eColorID_inactivecaption },
  { eCSSKeyword_inactivecaptiontext, LookAndFeel::eColorID_inactivecaptiontext },
  { eCSSKeyword_infobackground, LookAndFeel::eColorID_infobackground },
  { eCSSKeyword_infotext, LookAndFeel::eColorID_infotext },
  { eCSSKeyword_menu, LookAndFeel::eColorID_menu },
  { eCSSKeyword_menutext, LookAndFeel::eColorID_menutext },
  { eCSSKeyword_scrollbar, LookAndFeel::eColorID_scrollbar },
  { eCSSKeyword_threeddarkshadow, LookAndFeel::eColorID_threeddarkshadow },
  { eCSSKeyword_threedface, LookAndFeel::eColorID_threedface },
  { eCSSKeyword_threedhighlight, LookAndFeel::eColorID_threedhighlight },
  { eCSSKeyword_threedlightshadow, LookAndFeel::eColorID_threedlightshadow },
  { eCSSKeyword_threedshadow, LookAndFeel::eColorID_threedshadow },
  { eCSSKeyword_window, LookAndFeel::eColorID_window },
  { eCSSKeyword_windowframe, LookAndFeel::eColorID_windowframe },
  { eCSSKeyword_windowtext, LookAndFeel::eColorID_windowtext },
  { eCSSKeyword__moz_activehyperlinktext, NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT },
  { eCSSKeyword__moz_buttondefault, LookAndFeel::eColorID__moz_buttondefault },
  { eCSSKeyword__moz_buttonhoverface, LookAndFeel::eColorID__moz_buttonhoverface },
  { eCSSKeyword__moz_buttonhovertext, LookAndFeel::eColorID__moz_buttonhovertext },
  { eCSSKeyword__moz_cellhighlight, LookAndFeel::eColorID__moz_cellhighlight },
  { eCSSKeyword__moz_cellhighlighttext, LookAndFeel::eColorID__moz_cellhighlighttext },
  { eCSSKeyword__moz_eventreerow, LookAndFeel::eColorID__moz_eventreerow },
  { eCSSKeyword__moz_field, LookAndFeel::eColorID__moz_field },
  { eCSSKeyword__moz_fieldtext, LookAndFeel::eColorID__moz_fieldtext },
  { eCSSKeyword__moz_default_background_color, NS_COLOR_MOZ_DEFAULT_BACKGROUND_COLOR },
  { eCSSKeyword__moz_default_color, NS_COLOR_MOZ_DEFAULT_COLOR },
  { eCSSKeyword__moz_dialog, LookAndFeel::eColorID__moz_dialog },
  { eCSSKeyword__moz_dialogtext, LookAndFeel::eColorID__moz_dialogtext },
  { eCSSKeyword__moz_dragtargetzone, LookAndFeel::eColorID__moz_dragtargetzone },
  { eCSSKeyword__moz_gtk_info_bar_text, LookAndFeel::eColorID__moz_gtk_info_bar_text },
  { eCSSKeyword__moz_hyperlinktext, NS_COLOR_MOZ_HYPERLINKTEXT },
  { eCSSKeyword__moz_html_cellhighlight, LookAndFeel::eColorID__moz_html_cellhighlight },
  { eCSSKeyword__moz_html_cellhighlighttext, LookAndFeel::eColorID__moz_html_cellhighlighttext },
  { eCSSKeyword__moz_mac_buttonactivetext, LookAndFeel::eColorID__moz_mac_buttonactivetext },
  { eCSSKeyword__moz_mac_chrome_active, LookAndFeel::eColorID__moz_mac_chrome_active },
  { eCSSKeyword__moz_mac_chrome_inactive, LookAndFeel::eColorID__moz_mac_chrome_inactive },
  { eCSSKeyword__moz_mac_defaultbuttontext, LookAndFeel::eColorID__moz_mac_defaultbuttontext },
  { eCSSKeyword__moz_mac_focusring, LookAndFeel::eColorID__moz_mac_focusring },
  { eCSSKeyword__moz_mac_menuselect, LookAndFeel::eColorID__moz_mac_menuselect },
  { eCSSKeyword__moz_mac_menushadow, LookAndFeel::eColorID__moz_mac_menushadow },
  { eCSSKeyword__moz_mac_menutextdisable, LookAndFeel::eColorID__moz_mac_menutextdisable },
  { eCSSKeyword__moz_mac_menutextselect, LookAndFeel::eColorID__moz_mac_menutextselect },
  { eCSSKeyword__moz_mac_disabledtoolbartext, LookAndFeel::eColorID__moz_mac_disabledtoolbartext },
  { eCSSKeyword__moz_mac_secondaryhighlight, LookAndFeel::eColorID__moz_mac_secondaryhighlight },
  { eCSSKeyword__moz_menuhover, LookAndFeel::eColorID__moz_menuhover },
  { eCSSKeyword__moz_menuhovertext, LookAndFeel::eColorID__moz_menuhovertext },
  { eCSSKeyword__moz_menubartext, LookAndFeel::eColorID__moz_menubartext },
  { eCSSKeyword__moz_menubarhovertext, LookAndFeel::eColorID__moz_menubarhovertext },
  { eCSSKeyword__moz_oddtreerow, LookAndFeel::eColorID__moz_oddtreerow },
  { eCSSKeyword__moz_visitedhyperlinktext, NS_COLOR_MOZ_VISITEDHYPERLINKTEXT },
  { eCSSKeyword_currentcolor, NS_COLOR_CURRENTCOLOR },
  { eCSSKeyword__moz_win_mediatext, LookAndFeel::eColorID__moz_win_mediatext },
  { eCSSKeyword__moz_win_communicationstext, LookAndFeel::eColorID__moz_win_communicationstext },
  { eCSSKeyword__moz_nativehyperlinktext, LookAndFeel::eColorID__moz_nativehyperlinktext },
  { eCSSKeyword__moz_comboboxtext, LookAndFeel::eColorID__moz_comboboxtext },
  { eCSSKeyword__moz_combobox, LookAndFeel::eColorID__moz_combobox },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kContentKTable[] = {
  { eCSSKeyword_open_quote, NS_STYLE_CONTENT_OPEN_QUOTE },
  { eCSSKeyword_close_quote, NS_STYLE_CONTENT_CLOSE_QUOTE },
  { eCSSKeyword_no_open_quote, NS_STYLE_CONTENT_NO_OPEN_QUOTE },
  { eCSSKeyword_no_close_quote, NS_STYLE_CONTENT_NO_CLOSE_QUOTE },
  { eCSSKeyword__moz_alt_content, NS_STYLE_CONTENT_ALT_CONTENT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kControlCharacterVisibilityKTable[] = {
  { eCSSKeyword_hidden, NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN },
  { eCSSKeyword_visible, NS_STYLE_CONTROL_CHARACTER_VISIBILITY_VISIBLE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCounterRangeKTable[] = {
  { eCSSKeyword_infinite, NS_STYLE_COUNTER_RANGE_INFINITE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCounterSpeakAsKTable[] = {
  { eCSSKeyword_bullets, NS_STYLE_COUNTER_SPEAKAS_BULLETS },
  { eCSSKeyword_numbers, NS_STYLE_COUNTER_SPEAKAS_NUMBERS },
  { eCSSKeyword_words, NS_STYLE_COUNTER_SPEAKAS_WORDS },
  { eCSSKeyword_spell_out, NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCounterSymbolsSystemKTable[] = {
  { eCSSKeyword_cyclic, NS_STYLE_COUNTER_SYSTEM_CYCLIC },
  { eCSSKeyword_numeric, NS_STYLE_COUNTER_SYSTEM_NUMERIC },
  { eCSSKeyword_alphabetic, NS_STYLE_COUNTER_SYSTEM_ALPHABETIC },
  { eCSSKeyword_symbolic, NS_STYLE_COUNTER_SYSTEM_SYMBOLIC },
  { eCSSKeyword_fixed, NS_STYLE_COUNTER_SYSTEM_FIXED },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCounterSystemKTable[] = {
  { eCSSKeyword_cyclic, NS_STYLE_COUNTER_SYSTEM_CYCLIC },
  { eCSSKeyword_numeric, NS_STYLE_COUNTER_SYSTEM_NUMERIC },
  { eCSSKeyword_alphabetic, NS_STYLE_COUNTER_SYSTEM_ALPHABETIC },
  { eCSSKeyword_symbolic, NS_STYLE_COUNTER_SYSTEM_SYMBOLIC },
  { eCSSKeyword_additive, NS_STYLE_COUNTER_SYSTEM_ADDITIVE },
  { eCSSKeyword_fixed, NS_STYLE_COUNTER_SYSTEM_FIXED },
  { eCSSKeyword_extends, NS_STYLE_COUNTER_SYSTEM_EXTENDS },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kCursorKTable[] = {
  // CSS 2.0
  { eCSSKeyword_auto, NS_STYLE_CURSOR_AUTO },
  { eCSSKeyword_crosshair, NS_STYLE_CURSOR_CROSSHAIR },
  { eCSSKeyword_default, NS_STYLE_CURSOR_DEFAULT },
  { eCSSKeyword_pointer, NS_STYLE_CURSOR_POINTER },
  { eCSSKeyword_move, NS_STYLE_CURSOR_MOVE },
  { eCSSKeyword_e_resize, NS_STYLE_CURSOR_E_RESIZE },
  { eCSSKeyword_ne_resize, NS_STYLE_CURSOR_NE_RESIZE },
  { eCSSKeyword_nw_resize, NS_STYLE_CURSOR_NW_RESIZE },
  { eCSSKeyword_n_resize, NS_STYLE_CURSOR_N_RESIZE },
  { eCSSKeyword_se_resize, NS_STYLE_CURSOR_SE_RESIZE },
  { eCSSKeyword_sw_resize, NS_STYLE_CURSOR_SW_RESIZE },
  { eCSSKeyword_s_resize, NS_STYLE_CURSOR_S_RESIZE },
  { eCSSKeyword_w_resize, NS_STYLE_CURSOR_W_RESIZE },
  { eCSSKeyword_text, NS_STYLE_CURSOR_TEXT },
  { eCSSKeyword_wait, NS_STYLE_CURSOR_WAIT },
  { eCSSKeyword_help, NS_STYLE_CURSOR_HELP },
  // CSS 2.1
  { eCSSKeyword_progress, NS_STYLE_CURSOR_SPINNING },
  // CSS3 basic user interface module
  { eCSSKeyword_copy, NS_STYLE_CURSOR_COPY },
  { eCSSKeyword_alias, NS_STYLE_CURSOR_ALIAS },
  { eCSSKeyword_context_menu, NS_STYLE_CURSOR_CONTEXT_MENU },
  { eCSSKeyword_cell, NS_STYLE_CURSOR_CELL },
  { eCSSKeyword_not_allowed, NS_STYLE_CURSOR_NOT_ALLOWED },
  { eCSSKeyword_col_resize, NS_STYLE_CURSOR_COL_RESIZE },
  { eCSSKeyword_row_resize, NS_STYLE_CURSOR_ROW_RESIZE },
  { eCSSKeyword_no_drop, NS_STYLE_CURSOR_NO_DROP },
  { eCSSKeyword_vertical_text, NS_STYLE_CURSOR_VERTICAL_TEXT },
  { eCSSKeyword_all_scroll, NS_STYLE_CURSOR_ALL_SCROLL },
  { eCSSKeyword_nesw_resize, NS_STYLE_CURSOR_NESW_RESIZE },
  { eCSSKeyword_nwse_resize, NS_STYLE_CURSOR_NWSE_RESIZE },
  { eCSSKeyword_ns_resize, NS_STYLE_CURSOR_NS_RESIZE },
  { eCSSKeyword_ew_resize, NS_STYLE_CURSOR_EW_RESIZE },
  { eCSSKeyword_none, NS_STYLE_CURSOR_NONE },
  { eCSSKeyword_grab, NS_STYLE_CURSOR_GRAB },
  { eCSSKeyword_grabbing, NS_STYLE_CURSOR_GRABBING },
  { eCSSKeyword_zoom_in, NS_STYLE_CURSOR_ZOOM_IN },
  { eCSSKeyword_zoom_out, NS_STYLE_CURSOR_ZOOM_OUT },
  // -moz- prefixed vendor specific
  { eCSSKeyword__moz_grab, NS_STYLE_CURSOR_GRAB },
  { eCSSKeyword__moz_grabbing, NS_STYLE_CURSOR_GRABBING },
  { eCSSKeyword__moz_zoom_in, NS_STYLE_CURSOR_ZOOM_IN },
  { eCSSKeyword__moz_zoom_out, NS_STYLE_CURSOR_ZOOM_OUT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kDirectionKTable[] = {
  { eCSSKeyword_ltr,      NS_STYLE_DIRECTION_LTR },
  { eCSSKeyword_rtl,      NS_STYLE_DIRECTION_RTL },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kDisplayKTable[] = {
  { eCSSKeyword_none,                NS_STYLE_DISPLAY_NONE },
  { eCSSKeyword_inline,              NS_STYLE_DISPLAY_INLINE },
  { eCSSKeyword_block,               NS_STYLE_DISPLAY_BLOCK },
  { eCSSKeyword_inline_block,        NS_STYLE_DISPLAY_INLINE_BLOCK },
  { eCSSKeyword_list_item,           NS_STYLE_DISPLAY_LIST_ITEM },
  { eCSSKeyword_table,               NS_STYLE_DISPLAY_TABLE },
  { eCSSKeyword_inline_table,        NS_STYLE_DISPLAY_INLINE_TABLE },
  { eCSSKeyword_table_row_group,     NS_STYLE_DISPLAY_TABLE_ROW_GROUP },
  { eCSSKeyword_table_header_group,  NS_STYLE_DISPLAY_TABLE_HEADER_GROUP },
  { eCSSKeyword_table_footer_group,  NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP },
  { eCSSKeyword_table_row,           NS_STYLE_DISPLAY_TABLE_ROW },
  { eCSSKeyword_table_column_group,  NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP },
  { eCSSKeyword_table_column,        NS_STYLE_DISPLAY_TABLE_COLUMN },
  { eCSSKeyword_table_cell,          NS_STYLE_DISPLAY_TABLE_CELL },
  { eCSSKeyword_table_caption,       NS_STYLE_DISPLAY_TABLE_CAPTION },
  // Make sure this is kept in sync with the code in
  // nsCSSFrameConstructor::ConstructXULFrame
  { eCSSKeyword__moz_box,            NS_STYLE_DISPLAY_BOX },
  { eCSSKeyword__moz_inline_box,     NS_STYLE_DISPLAY_INLINE_BOX },
#ifdef MOZ_XUL
  { eCSSKeyword__moz_grid,           NS_STYLE_DISPLAY_XUL_GRID },
  { eCSSKeyword__moz_inline_grid,    NS_STYLE_DISPLAY_INLINE_XUL_GRID },
  { eCSSKeyword__moz_grid_group,     NS_STYLE_DISPLAY_XUL_GRID_GROUP },
  { eCSSKeyword__moz_grid_line,      NS_STYLE_DISPLAY_XUL_GRID_LINE },
  { eCSSKeyword__moz_stack,          NS_STYLE_DISPLAY_STACK },
  { eCSSKeyword__moz_inline_stack,   NS_STYLE_DISPLAY_INLINE_STACK },
  { eCSSKeyword__moz_deck,           NS_STYLE_DISPLAY_DECK },
  { eCSSKeyword__moz_popup,          NS_STYLE_DISPLAY_POPUP },
  { eCSSKeyword__moz_groupbox,       NS_STYLE_DISPLAY_GROUPBOX },
#endif
  { eCSSKeyword_flex,                NS_STYLE_DISPLAY_FLEX },
  { eCSSKeyword_inline_flex,         NS_STYLE_DISPLAY_INLINE_FLEX },
  { eCSSKeyword_ruby,                NS_STYLE_DISPLAY_RUBY },
  { eCSSKeyword_ruby_base,           NS_STYLE_DISPLAY_RUBY_BASE },
  { eCSSKeyword_ruby_base_container, NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER },
  { eCSSKeyword_ruby_text,           NS_STYLE_DISPLAY_RUBY_TEXT },
  { eCSSKeyword_ruby_text_container, NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER },
  // The next two entries are controlled by the layout.css.grid.enabled pref.
  { eCSSKeyword_grid,                NS_STYLE_DISPLAY_GRID },
  { eCSSKeyword_inline_grid,         NS_STYLE_DISPLAY_INLINE_GRID },
  // The next entry is controlled by the layout.css.display-contents.enabled
  // pref.
  { eCSSKeyword_contents,            NS_STYLE_DISPLAY_CONTENTS },
  { eCSSKeyword_UNKNOWN,             -1 }
};

const KTableEntry nsCSSProps::kEmptyCellsKTable[] = {
  { eCSSKeyword_show,                 NS_STYLE_TABLE_EMPTY_CELLS_SHOW },
  { eCSSKeyword_hide,                 NS_STYLE_TABLE_EMPTY_CELLS_HIDE },
  { eCSSKeyword_UNKNOWN,              -1 }
};

const KTableEntry nsCSSProps::kAlignAllKeywords[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_space_between, NS_STYLE_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around,  NS_STYLE_ALIGN_SPACE_AROUND },
  { eCSSKeyword_space_evenly,  NS_STYLE_ALIGN_SPACE_EVENLY },
  { eCSSKeyword_legacy,        NS_STYLE_ALIGN_LEGACY },
  { eCSSKeyword_safe,          NS_STYLE_ALIGN_SAFE },
  { eCSSKeyword_true,          NS_STYLE_ALIGN_TRUE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignOverflowPosition[] = {
  { eCSSKeyword_true,          NS_STYLE_ALIGN_TRUE },
  { eCSSKeyword_safe,          NS_STYLE_ALIGN_SAFE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignSelfPosition[] = {
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_self_start,    NS_STYLE_ALIGN_SELF_START },
  { eCSSKeyword_self_end,      NS_STYLE_ALIGN_SELF_END },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignLegacy[] = {
  { eCSSKeyword_legacy,        NS_STYLE_ALIGN_LEGACY },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignLegacyPosition[] = {
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignAutoStretchBaseline[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignAutoBaseline[] = {
  { eCSSKeyword_auto,          NS_STYLE_ALIGN_AUTO },
  { eCSSKeyword_baseline,      NS_STYLE_ALIGN_BASELINE },
  { eCSSKeyword_last_baseline, NS_STYLE_ALIGN_LAST_BASELINE },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignContentDistribution[] = {
  { eCSSKeyword_stretch,       NS_STYLE_ALIGN_STRETCH },
  { eCSSKeyword_space_between, NS_STYLE_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around,  NS_STYLE_ALIGN_SPACE_AROUND },
  { eCSSKeyword_space_evenly,  NS_STYLE_ALIGN_SPACE_EVENLY },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kAlignContentPosition[] = {
  { eCSSKeyword_start,         NS_STYLE_ALIGN_START },
  { eCSSKeyword_end,           NS_STYLE_ALIGN_END },
  { eCSSKeyword_flex_start,    NS_STYLE_ALIGN_FLEX_START },
  { eCSSKeyword_flex_end,      NS_STYLE_ALIGN_FLEX_END },
  { eCSSKeyword_center,        NS_STYLE_ALIGN_CENTER },
  { eCSSKeyword_left,          NS_STYLE_ALIGN_LEFT },
  { eCSSKeyword_right,         NS_STYLE_ALIGN_RIGHT },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kFlexDirectionKTable[] = {
  { eCSSKeyword_row,            NS_STYLE_FLEX_DIRECTION_ROW },
  { eCSSKeyword_row_reverse,    NS_STYLE_FLEX_DIRECTION_ROW_REVERSE },
  { eCSSKeyword_column,         NS_STYLE_FLEX_DIRECTION_COLUMN },
  { eCSSKeyword_column_reverse, NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE },
  { eCSSKeyword_UNKNOWN,        -1 }
};

const KTableEntry nsCSSProps::kFlexWrapKTable[] = {
  { eCSSKeyword_nowrap,       NS_STYLE_FLEX_WRAP_NOWRAP },
  { eCSSKeyword_wrap,         NS_STYLE_FLEX_WRAP_WRAP },
  { eCSSKeyword_wrap_reverse, NS_STYLE_FLEX_WRAP_WRAP_REVERSE },
  { eCSSKeyword_UNKNOWN,      -1 }
};

const KTableEntry nsCSSProps::kHyphensKTable[] = {
  { eCSSKeyword_none, NS_STYLE_HYPHENS_NONE },
  { eCSSKeyword_manual, NS_STYLE_HYPHENS_MANUAL },
  { eCSSKeyword_auto, NS_STYLE_HYPHENS_AUTO },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kFloatKTable[] = {
  { eCSSKeyword_none,         NS_STYLE_FLOAT_NONE },
  { eCSSKeyword_left,         NS_STYLE_FLOAT_LEFT },
  { eCSSKeyword_right,        NS_STYLE_FLOAT_RIGHT },
  { eCSSKeyword_inline_start, NS_STYLE_FLOAT_INLINE_START },
  { eCSSKeyword_inline_end,   NS_STYLE_FLOAT_INLINE_END },
  { eCSSKeyword_UNKNOWN,      -1 }
};

const KTableEntry nsCSSProps::kFloatEdgeKTable[] = {
  { eCSSKeyword_content_box, NS_STYLE_FLOAT_EDGE_CONTENT },
  { eCSSKeyword_margin_box, NS_STYLE_FLOAT_EDGE_MARGIN },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontKTable[] = {
  // CSS2.
  { eCSSKeyword_caption, NS_STYLE_FONT_CAPTION },
  { eCSSKeyword_icon, NS_STYLE_FONT_ICON },
  { eCSSKeyword_menu, NS_STYLE_FONT_MENU },
  { eCSSKeyword_message_box, NS_STYLE_FONT_MESSAGE_BOX },
  { eCSSKeyword_small_caption, NS_STYLE_FONT_SMALL_CAPTION },
  { eCSSKeyword_status_bar, NS_STYLE_FONT_STATUS_BAR },

  // Proposed for CSS3.
  { eCSSKeyword__moz_window, NS_STYLE_FONT_WINDOW },
  { eCSSKeyword__moz_document, NS_STYLE_FONT_DOCUMENT },
  { eCSSKeyword__moz_workspace, NS_STYLE_FONT_WORKSPACE },
  { eCSSKeyword__moz_desktop, NS_STYLE_FONT_DESKTOP },
  { eCSSKeyword__moz_info, NS_STYLE_FONT_INFO },
  { eCSSKeyword__moz_dialog, NS_STYLE_FONT_DIALOG },
  { eCSSKeyword__moz_button, NS_STYLE_FONT_BUTTON },
  { eCSSKeyword__moz_pull_down_menu, NS_STYLE_FONT_PULL_DOWN_MENU },
  { eCSSKeyword__moz_list, NS_STYLE_FONT_LIST },
  { eCSSKeyword__moz_field, NS_STYLE_FONT_FIELD },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontKerningKTable[] = {
  { eCSSKeyword_auto, NS_FONT_KERNING_AUTO },
  { eCSSKeyword_none, NS_FONT_KERNING_NONE },
  { eCSSKeyword_normal, NS_FONT_KERNING_NORMAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontSizeKTable[] = {
  { eCSSKeyword_xx_small, NS_STYLE_FONT_SIZE_XXSMALL },
  { eCSSKeyword_x_small, NS_STYLE_FONT_SIZE_XSMALL },
  { eCSSKeyword_small, NS_STYLE_FONT_SIZE_SMALL },
  { eCSSKeyword_medium, NS_STYLE_FONT_SIZE_MEDIUM },
  { eCSSKeyword_large, NS_STYLE_FONT_SIZE_LARGE },
  { eCSSKeyword_x_large, NS_STYLE_FONT_SIZE_XLARGE },
  { eCSSKeyword_xx_large, NS_STYLE_FONT_SIZE_XXLARGE },
  { eCSSKeyword_larger, NS_STYLE_FONT_SIZE_LARGER },
  { eCSSKeyword_smaller, NS_STYLE_FONT_SIZE_SMALLER },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontSmoothingKTable[] = {
  { eCSSKeyword_auto, NS_FONT_SMOOTHING_AUTO },
  { eCSSKeyword_grayscale, NS_FONT_SMOOTHING_GRAYSCALE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontStretchKTable[] = {
  { eCSSKeyword_ultra_condensed, NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED },
  { eCSSKeyword_extra_condensed, NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED },
  { eCSSKeyword_condensed, NS_STYLE_FONT_STRETCH_CONDENSED },
  { eCSSKeyword_semi_condensed, NS_STYLE_FONT_STRETCH_SEMI_CONDENSED },
  { eCSSKeyword_normal, NS_STYLE_FONT_STRETCH_NORMAL },
  { eCSSKeyword_semi_expanded, NS_STYLE_FONT_STRETCH_SEMI_EXPANDED },
  { eCSSKeyword_expanded, NS_STYLE_FONT_STRETCH_EXPANDED },
  { eCSSKeyword_extra_expanded, NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED },
  { eCSSKeyword_ultra_expanded, NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontStyleKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_FONT_STYLE_NORMAL },
  { eCSSKeyword_italic, NS_STYLE_FONT_STYLE_ITALIC },
  { eCSSKeyword_oblique, NS_STYLE_FONT_STYLE_OBLIQUE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontSynthesisKTable[] = {
  { eCSSKeyword_weight, NS_FONT_SYNTHESIS_WEIGHT },
  { eCSSKeyword_style, NS_FONT_SYNTHESIS_STYLE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantAlternatesKTable[] = {
  { eCSSKeyword_historical_forms, NS_FONT_VARIANT_ALTERNATES_HISTORICAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantAlternatesFuncsKTable[] = {
  { eCSSKeyword_stylistic, NS_FONT_VARIANT_ALTERNATES_STYLISTIC },
  { eCSSKeyword_styleset, NS_FONT_VARIANT_ALTERNATES_STYLESET },
  { eCSSKeyword_character_variant, NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT },
  { eCSSKeyword_swash, NS_FONT_VARIANT_ALTERNATES_SWASH },
  { eCSSKeyword_ornaments, NS_FONT_VARIANT_ALTERNATES_ORNAMENTS },
  { eCSSKeyword_annotation, NS_FONT_VARIANT_ALTERNATES_ANNOTATION },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantCapsKTable[] = {
  { eCSSKeyword_small_caps, NS_FONT_VARIANT_CAPS_SMALLCAPS },
  { eCSSKeyword_all_small_caps, NS_FONT_VARIANT_CAPS_ALLSMALL },
  { eCSSKeyword_petite_caps, NS_FONT_VARIANT_CAPS_PETITECAPS },
  { eCSSKeyword_all_petite_caps, NS_FONT_VARIANT_CAPS_ALLPETITE },
  { eCSSKeyword_titling_caps, NS_FONT_VARIANT_CAPS_TITLING },
  { eCSSKeyword_unicase, NS_FONT_VARIANT_CAPS_UNICASE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantEastAsianKTable[] = {
  { eCSSKeyword_jis78, NS_FONT_VARIANT_EAST_ASIAN_JIS78 },
  { eCSSKeyword_jis83, NS_FONT_VARIANT_EAST_ASIAN_JIS83 },
  { eCSSKeyword_jis90, NS_FONT_VARIANT_EAST_ASIAN_JIS90 },
  { eCSSKeyword_jis04, NS_FONT_VARIANT_EAST_ASIAN_JIS04 },
  { eCSSKeyword_simplified, NS_FONT_VARIANT_EAST_ASIAN_SIMPLIFIED },
  { eCSSKeyword_traditional, NS_FONT_VARIANT_EAST_ASIAN_TRADITIONAL },
  { eCSSKeyword_full_width, NS_FONT_VARIANT_EAST_ASIAN_FULL_WIDTH },
  { eCSSKeyword_proportional_width, NS_FONT_VARIANT_EAST_ASIAN_PROP_WIDTH },
  { eCSSKeyword_ruby, NS_FONT_VARIANT_EAST_ASIAN_RUBY },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantLigaturesKTable[] = {
  { eCSSKeyword_common_ligatures, NS_FONT_VARIANT_LIGATURES_COMMON },
  { eCSSKeyword_no_common_ligatures, NS_FONT_VARIANT_LIGATURES_NO_COMMON },
  { eCSSKeyword_discretionary_ligatures, NS_FONT_VARIANT_LIGATURES_DISCRETIONARY },
  { eCSSKeyword_no_discretionary_ligatures, NS_FONT_VARIANT_LIGATURES_NO_DISCRETIONARY },
  { eCSSKeyword_historical_ligatures, NS_FONT_VARIANT_LIGATURES_HISTORICAL },
  { eCSSKeyword_no_historical_ligatures, NS_FONT_VARIANT_LIGATURES_NO_HISTORICAL },
  { eCSSKeyword_contextual, NS_FONT_VARIANT_LIGATURES_CONTEXTUAL },
  { eCSSKeyword_no_contextual, NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantNumericKTable[] = {
  { eCSSKeyword_lining_nums, NS_FONT_VARIANT_NUMERIC_LINING },
  { eCSSKeyword_oldstyle_nums, NS_FONT_VARIANT_NUMERIC_OLDSTYLE },
  { eCSSKeyword_proportional_nums, NS_FONT_VARIANT_NUMERIC_PROPORTIONAL },
  { eCSSKeyword_tabular_nums, NS_FONT_VARIANT_NUMERIC_TABULAR },
  { eCSSKeyword_diagonal_fractions, NS_FONT_VARIANT_NUMERIC_DIAGONAL_FRACTIONS },
  { eCSSKeyword_stacked_fractions, NS_FONT_VARIANT_NUMERIC_STACKED_FRACTIONS },
  { eCSSKeyword_slashed_zero, NS_FONT_VARIANT_NUMERIC_SLASHZERO },
  { eCSSKeyword_ordinal, NS_FONT_VARIANT_NUMERIC_ORDINAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontVariantPositionKTable[] = {
  { eCSSKeyword_super, NS_FONT_VARIANT_POSITION_SUPER },
  { eCSSKeyword_sub, NS_FONT_VARIANT_POSITION_SUB },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFontWeightKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_FONT_WEIGHT_NORMAL },
  { eCSSKeyword_bold, NS_STYLE_FONT_WEIGHT_BOLD },
  { eCSSKeyword_bolder, NS_STYLE_FONT_WEIGHT_BOLDER },
  { eCSSKeyword_lighter, NS_STYLE_FONT_WEIGHT_LIGHTER },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kGridAutoFlowKTable[] = {
  { eCSSKeyword_row, NS_STYLE_GRID_AUTO_FLOW_ROW },
  { eCSSKeyword_column, NS_STYLE_GRID_AUTO_FLOW_COLUMN },
  { eCSSKeyword_dense, NS_STYLE_GRID_AUTO_FLOW_DENSE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kGridTrackBreadthKTable[] = {
  { eCSSKeyword_min_content, NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT },
  { eCSSKeyword_max_content, NS_STYLE_GRID_TRACK_BREADTH_MAX_CONTENT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kImageOrientationKTable[] = {
  { eCSSKeyword_flip, NS_STYLE_IMAGE_ORIENTATION_FLIP },
  { eCSSKeyword_from_image, NS_STYLE_IMAGE_ORIENTATION_FROM_IMAGE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kImageOrientationFlipKTable[] = {
  { eCSSKeyword_flip, NS_STYLE_IMAGE_ORIENTATION_FLIP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kIsolationKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_ISOLATION_AUTO },
  { eCSSKeyword_isolate, NS_STYLE_ISOLATION_ISOLATE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kIMEModeKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_IME_MODE_NORMAL },
  { eCSSKeyword_auto, NS_STYLE_IME_MODE_AUTO },
  { eCSSKeyword_active, NS_STYLE_IME_MODE_ACTIVE },
  { eCSSKeyword_disabled, NS_STYLE_IME_MODE_DISABLED },
  { eCSSKeyword_inactive, NS_STYLE_IME_MODE_INACTIVE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kLineHeightKTable[] = {
  // -moz- prefixed, intended for internal use for single-line controls
  { eCSSKeyword__moz_block_height, NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kListStylePositionKTable[] = {
  { eCSSKeyword_inside, NS_STYLE_LIST_STYLE_POSITION_INSIDE },
  { eCSSKeyword_outside, NS_STYLE_LIST_STYLE_POSITION_OUTSIDE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kListStyleKTable[] = {
  // none and decimal are not redefinable, so they should not be moved.
  { eCSSKeyword_none, NS_STYLE_LIST_STYLE_NONE },
  { eCSSKeyword_decimal, NS_STYLE_LIST_STYLE_DECIMAL },
  // the following graphic styles are processed in a different way.
  { eCSSKeyword_disc, NS_STYLE_LIST_STYLE_DISC },
  { eCSSKeyword_circle, NS_STYLE_LIST_STYLE_CIRCLE },
  { eCSSKeyword_square, NS_STYLE_LIST_STYLE_SQUARE },
  { eCSSKeyword_disclosure_closed, NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED },
  { eCSSKeyword_disclosure_open, NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN },
  // the following counter styles require specific algorithms to generate.
  { eCSSKeyword_hebrew, NS_STYLE_LIST_STYLE_HEBREW },
  { eCSSKeyword_japanese_informal, NS_STYLE_LIST_STYLE_JAPANESE_INFORMAL },
  { eCSSKeyword_japanese_formal, NS_STYLE_LIST_STYLE_JAPANESE_FORMAL },
  { eCSSKeyword_korean_hangul_formal, NS_STYLE_LIST_STYLE_KOREAN_HANGUL_FORMAL },
  { eCSSKeyword_korean_hanja_informal, NS_STYLE_LIST_STYLE_KOREAN_HANJA_INFORMAL },
  { eCSSKeyword_korean_hanja_formal, NS_STYLE_LIST_STYLE_KOREAN_HANJA_FORMAL },
  { eCSSKeyword_simp_chinese_informal, NS_STYLE_LIST_STYLE_SIMP_CHINESE_INFORMAL },
  { eCSSKeyword_simp_chinese_formal, NS_STYLE_LIST_STYLE_SIMP_CHINESE_FORMAL },
  { eCSSKeyword_trad_chinese_informal, NS_STYLE_LIST_STYLE_TRAD_CHINESE_INFORMAL },
  { eCSSKeyword_trad_chinese_formal, NS_STYLE_LIST_STYLE_TRAD_CHINESE_FORMAL },
  { eCSSKeyword_ethiopic_numeric, NS_STYLE_LIST_STYLE_ETHIOPIC_NUMERIC },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kMathVariantKTable[] = {
  { eCSSKeyword_none, NS_MATHML_MATHVARIANT_NONE },
  { eCSSKeyword_normal, NS_MATHML_MATHVARIANT_NORMAL },
  { eCSSKeyword_bold, NS_MATHML_MATHVARIANT_BOLD },
  { eCSSKeyword_italic, NS_MATHML_MATHVARIANT_ITALIC },
  { eCSSKeyword_bold_italic, NS_MATHML_MATHVARIANT_BOLD_ITALIC },
  { eCSSKeyword_script, NS_MATHML_MATHVARIANT_SCRIPT },
  { eCSSKeyword_bold_script, NS_MATHML_MATHVARIANT_BOLD_SCRIPT },
  { eCSSKeyword_fraktur, NS_MATHML_MATHVARIANT_FRAKTUR },
  { eCSSKeyword_double_struck, NS_MATHML_MATHVARIANT_DOUBLE_STRUCK },
  { eCSSKeyword_bold_fraktur, NS_MATHML_MATHVARIANT_BOLD_FRAKTUR },
  { eCSSKeyword_sans_serif, NS_MATHML_MATHVARIANT_SANS_SERIF },
  { eCSSKeyword_bold_sans_serif, NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF },
  { eCSSKeyword_sans_serif_italic, NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC },
  { eCSSKeyword_sans_serif_bold_italic, NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC },
  { eCSSKeyword_monospace, NS_MATHML_MATHVARIANT_MONOSPACE },
  { eCSSKeyword_initial, NS_MATHML_MATHVARIANT_INITIAL },
  { eCSSKeyword_tailed, NS_MATHML_MATHVARIANT_TAILED },
  { eCSSKeyword_looped, NS_MATHML_MATHVARIANT_LOOPED },
  { eCSSKeyword_stretched, NS_MATHML_MATHVARIANT_STRETCHED },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kMathDisplayKTable[] = {
  { eCSSKeyword_inline, NS_MATHML_DISPLAYSTYLE_INLINE },
  { eCSSKeyword_block, NS_MATHML_DISPLAYSTYLE_BLOCK },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kContainKTable[] = {
  { eCSSKeyword_none,    NS_STYLE_CONTAIN_NONE },
  { eCSSKeyword_strict,  NS_STYLE_CONTAIN_STRICT },
  { eCSSKeyword_layout,  NS_STYLE_CONTAIN_LAYOUT },
  { eCSSKeyword_style,   NS_STYLE_CONTAIN_STYLE },
  { eCSSKeyword_paint,   NS_STYLE_CONTAIN_PAINT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kContextOpacityKTable[] = {
  { eCSSKeyword_context_fill_opacity, NS_STYLE_CONTEXT_FILL_OPACITY },
  { eCSSKeyword_context_stroke_opacity, NS_STYLE_CONTEXT_STROKE_OPACITY },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kContextPatternKTable[] = {
  { eCSSKeyword_context_fill, NS_COLOR_CONTEXT_FILL },
  { eCSSKeyword_context_stroke, NS_COLOR_CONTEXT_STROKE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kObjectFitKTable[] = {
  { eCSSKeyword_fill,       NS_STYLE_OBJECT_FIT_FILL },
  { eCSSKeyword_contain,    NS_STYLE_OBJECT_FIT_CONTAIN },
  { eCSSKeyword_cover,      NS_STYLE_OBJECT_FIT_COVER },
  { eCSSKeyword_none,       NS_STYLE_OBJECT_FIT_NONE },
  { eCSSKeyword_scale_down, NS_STYLE_OBJECT_FIT_SCALE_DOWN },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kOrientKTable[] = {
  { eCSSKeyword_inline,     NS_STYLE_ORIENT_INLINE },
  { eCSSKeyword_block,      NS_STYLE_ORIENT_BLOCK },
  { eCSSKeyword_horizontal, NS_STYLE_ORIENT_HORIZONTAL },
  { eCSSKeyword_vertical,   NS_STYLE_ORIENT_VERTICAL },
  { eCSSKeyword_UNKNOWN,    -1 }
};

// Same as kBorderStyleKTable except 'hidden'.
const KTableEntry nsCSSProps::kOutlineStyleKTable[] = {
  { eCSSKeyword_none,   NS_STYLE_BORDER_STYLE_NONE },
  { eCSSKeyword_auto,   NS_STYLE_BORDER_STYLE_AUTO },
  { eCSSKeyword_dotted, NS_STYLE_BORDER_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_BORDER_STYLE_DASHED },
  { eCSSKeyword_solid,  NS_STYLE_BORDER_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_BORDER_STYLE_DOUBLE },
  { eCSSKeyword_groove, NS_STYLE_BORDER_STYLE_GROOVE },
  { eCSSKeyword_ridge,  NS_STYLE_BORDER_STYLE_RIDGE },
  { eCSSKeyword_inset,  NS_STYLE_BORDER_STYLE_INSET },
  { eCSSKeyword_outset, NS_STYLE_BORDER_STYLE_OUTSET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOutlineColorKTable[] = {
  { eCSSKeyword__moz_use_text_color, NS_STYLE_COLOR_MOZ_USE_TEXT_COLOR },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO },
  { eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL },
  // Deprecated:
  { eCSSKeyword__moz_scrollbars_none, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword__moz_scrollbars_horizontal, NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL },
  { eCSSKeyword__moz_scrollbars_vertical, NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL },
  { eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowClipBoxKTable[] = {
  { eCSSKeyword_padding_box, NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX },
  { eCSSKeyword_content_box, NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kOverflowSubKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_OVERFLOW_AUTO },
  { eCSSKeyword_visible, NS_STYLE_OVERFLOW_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_OVERFLOW_HIDDEN },
  { eCSSKeyword_scroll, NS_STYLE_OVERFLOW_SCROLL },
  // Deprecated:
  { eCSSKeyword__moz_hidden_unscrollable, NS_STYLE_OVERFLOW_CLIP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kPageBreakKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_PAGE_BREAK_AUTO },
  { eCSSKeyword_always, NS_STYLE_PAGE_BREAK_ALWAYS },
  { eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID },
  { eCSSKeyword_left, NS_STYLE_PAGE_BREAK_LEFT },
  { eCSSKeyword_right, NS_STYLE_PAGE_BREAK_RIGHT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kPageBreakInsideKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_PAGE_BREAK_AUTO },
  { eCSSKeyword_avoid, NS_STYLE_PAGE_BREAK_AVOID },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kPageMarksKTable[] = {
  { eCSSKeyword_none, NS_STYLE_PAGE_MARKS_NONE },
  { eCSSKeyword_crop, NS_STYLE_PAGE_MARKS_CROP },
  { eCSSKeyword_cross, NS_STYLE_PAGE_MARKS_REGISTER },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kPageSizeKTable[] = {
  { eCSSKeyword_landscape, NS_STYLE_PAGE_SIZE_LANDSCAPE },
  { eCSSKeyword_portrait, NS_STYLE_PAGE_SIZE_PORTRAIT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kPointerEventsKTable[] = {
  { eCSSKeyword_none, NS_STYLE_POINTER_EVENTS_NONE },
  { eCSSKeyword_visiblepainted, NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED },
  { eCSSKeyword_visiblefill, NS_STYLE_POINTER_EVENTS_VISIBLEFILL },
  { eCSSKeyword_visiblestroke, NS_STYLE_POINTER_EVENTS_VISIBLESTROKE },
  { eCSSKeyword_visible, NS_STYLE_POINTER_EVENTS_VISIBLE },
  { eCSSKeyword_painted, NS_STYLE_POINTER_EVENTS_PAINTED },
  { eCSSKeyword_fill, NS_STYLE_POINTER_EVENTS_FILL },
  { eCSSKeyword_stroke, NS_STYLE_POINTER_EVENTS_STROKE },
  { eCSSKeyword_all, NS_STYLE_POINTER_EVENTS_ALL },
  { eCSSKeyword_auto, NS_STYLE_POINTER_EVENTS_AUTO },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kPositionKTable[] = {
  { eCSSKeyword_static, NS_STYLE_POSITION_STATIC },
  { eCSSKeyword_relative, NS_STYLE_POSITION_RELATIVE },
  { eCSSKeyword_absolute, NS_STYLE_POSITION_ABSOLUTE },
  { eCSSKeyword_fixed, NS_STYLE_POSITION_FIXED },
  // The next entry is controlled by the layout.css.sticky.enabled pref.
  { eCSSKeyword_sticky, NS_STYLE_POSITION_STICKY },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kRadialGradientShapeKTable[] = {
  { eCSSKeyword_circle,  NS_STYLE_GRADIENT_SHAPE_CIRCULAR },
  { eCSSKeyword_ellipse, NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kRadialGradientSizeKTable[] = {
  { eCSSKeyword_closest_side,    NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE },
  { eCSSKeyword_closest_corner,  NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER },
  { eCSSKeyword_farthest_side,   NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE },
  { eCSSKeyword_farthest_corner, NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER },
  { eCSSKeyword_UNKNOWN,         -1 }
};

const KTableEntry nsCSSProps::kRadialGradientLegacySizeKTable[] = {
  { eCSSKeyword_closest_side,    NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE },
  { eCSSKeyword_closest_corner,  NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER },
  { eCSSKeyword_farthest_side,   NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE },
  { eCSSKeyword_farthest_corner, NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER },
  // synonyms
  { eCSSKeyword_contain,         NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE },
  { eCSSKeyword_cover,           NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER },
  { eCSSKeyword_UNKNOWN,         -1 }
};

const KTableEntry nsCSSProps::kResizeKTable[] = {
  { eCSSKeyword_none,       NS_STYLE_RESIZE_NONE },
  { eCSSKeyword_both,       NS_STYLE_RESIZE_BOTH },
  { eCSSKeyword_horizontal, NS_STYLE_RESIZE_HORIZONTAL },
  { eCSSKeyword_vertical,   NS_STYLE_RESIZE_VERTICAL },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kRubyAlignKTable[] = {
  { eCSSKeyword_start, NS_STYLE_RUBY_ALIGN_START },
  { eCSSKeyword_center, NS_STYLE_RUBY_ALIGN_CENTER },
  { eCSSKeyword_space_between, NS_STYLE_RUBY_ALIGN_SPACE_BETWEEN },
  { eCSSKeyword_space_around, NS_STYLE_RUBY_ALIGN_SPACE_AROUND },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kRubyPositionKTable[] = {
  { eCSSKeyword_over, NS_STYLE_RUBY_POSITION_OVER },
  { eCSSKeyword_under, NS_STYLE_RUBY_POSITION_UNDER },
  // bug 1055672 for 'inter-character' support
  // { eCSSKeyword_inter_character, NS_STYLE_RUBY_POSITION_INTER_CHARACTER },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kScrollBehaviorKTable[] = {
  { eCSSKeyword_auto,       NS_STYLE_SCROLL_BEHAVIOR_AUTO },
  { eCSSKeyword_smooth,     NS_STYLE_SCROLL_BEHAVIOR_SMOOTH },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kScrollSnapTypeKTable[] = {
  { eCSSKeyword_none,      NS_STYLE_SCROLL_SNAP_TYPE_NONE },
  { eCSSKeyword_mandatory, NS_STYLE_SCROLL_SNAP_TYPE_MANDATORY },
  { eCSSKeyword_proximity, NS_STYLE_SCROLL_SNAP_TYPE_PROXIMITY },
  { eCSSKeyword_UNKNOWN,   -1 }
};

const KTableEntry nsCSSProps::kStackSizingKTable[] = {
  { eCSSKeyword_ignore, NS_STYLE_STACK_SIZING_IGNORE },
  { eCSSKeyword_stretch_to_fit, NS_STYLE_STACK_SIZING_STRETCH_TO_FIT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTableLayoutKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_TABLE_LAYOUT_AUTO },
  { eCSSKeyword_fixed, NS_STYLE_TABLE_LAYOUT_FIXED },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kTextAlignKTable[] = {
  { eCSSKeyword_left, NS_STYLE_TEXT_ALIGN_LEFT },
  { eCSSKeyword_right, NS_STYLE_TEXT_ALIGN_RIGHT },
  { eCSSKeyword_center, NS_STYLE_TEXT_ALIGN_CENTER },
  { eCSSKeyword_justify, NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { eCSSKeyword__moz_center, NS_STYLE_TEXT_ALIGN_MOZ_CENTER },
  { eCSSKeyword__moz_right, NS_STYLE_TEXT_ALIGN_MOZ_RIGHT },
  { eCSSKeyword__moz_left, NS_STYLE_TEXT_ALIGN_MOZ_LEFT },
  { eCSSKeyword_start, NS_STYLE_TEXT_ALIGN_DEFAULT },
  { eCSSKeyword_end, NS_STYLE_TEXT_ALIGN_END },
  { eCSSKeyword_true, NS_STYLE_TEXT_ALIGN_TRUE },
  { eCSSKeyword_match_parent, NS_STYLE_TEXT_ALIGN_MATCH_PARENT },
  { eCSSKeyword_UNKNOWN, -1 }
};

KTableEntry nsCSSProps::kTextAlignLastKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_TEXT_ALIGN_AUTO },
  { eCSSKeyword_left, NS_STYLE_TEXT_ALIGN_LEFT },
  { eCSSKeyword_right, NS_STYLE_TEXT_ALIGN_RIGHT },
  { eCSSKeyword_center, NS_STYLE_TEXT_ALIGN_CENTER },
  { eCSSKeyword_justify, NS_STYLE_TEXT_ALIGN_JUSTIFY },
  { eCSSKeyword_start, NS_STYLE_TEXT_ALIGN_DEFAULT },
  { eCSSKeyword_end, NS_STYLE_TEXT_ALIGN_END },
  { eCSSKeyword_true, NS_STYLE_TEXT_ALIGN_TRUE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextCombineUprightKTable[] = {
  { eCSSKeyword_none, NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE },
  { eCSSKeyword_all, NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL },
  { eCSSKeyword_digits, NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2 }, // w/o number ==> 2
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextDecorationLineKTable[] = {
  { eCSSKeyword_none, NS_STYLE_TEXT_DECORATION_LINE_NONE },
  { eCSSKeyword_underline, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE },
  { eCSSKeyword_overline, NS_STYLE_TEXT_DECORATION_LINE_OVERLINE },
  { eCSSKeyword_line_through, NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH },
  { eCSSKeyword_blink, NS_STYLE_TEXT_DECORATION_LINE_BLINK },
  { eCSSKeyword__moz_anchor_decoration, NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextDecorationStyleKTable[] = {
  { eCSSKeyword__moz_none, NS_STYLE_TEXT_DECORATION_STYLE_NONE },
  { eCSSKeyword_solid, NS_STYLE_TEXT_DECORATION_STYLE_SOLID },
  { eCSSKeyword_double, NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE },
  { eCSSKeyword_dotted, NS_STYLE_TEXT_DECORATION_STYLE_DOTTED },
  { eCSSKeyword_dashed, NS_STYLE_TEXT_DECORATION_STYLE_DASHED },
  { eCSSKeyword_wavy, NS_STYLE_TEXT_DECORATION_STYLE_WAVY },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextOrientationKTable[] = {
  { eCSSKeyword_mixed, NS_STYLE_TEXT_ORIENTATION_MIXED },
  { eCSSKeyword_upright, NS_STYLE_TEXT_ORIENTATION_UPRIGHT },
  { eCSSKeyword_sideways, NS_STYLE_TEXT_ORIENTATION_SIDEWAYS },
  { eCSSKeyword_sideways_right, NS_STYLE_TEXT_ORIENTATION_SIDEWAYS },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextEmphasisPositionKTable[] = {
  { eCSSKeyword_over, NS_STYLE_TEXT_EMPHASIS_POSITION_OVER },
  { eCSSKeyword_under, NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER },
  { eCSSKeyword_left, NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT },
  { eCSSKeyword_right, NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextEmphasisStyleFillKTable[] = {
  { eCSSKeyword_filled, NS_STYLE_TEXT_EMPHASIS_STYLE_FILLED },
  { eCSSKeyword_open, NS_STYLE_TEXT_EMPHASIS_STYLE_OPEN },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextEmphasisStyleShapeKTable[] = {
  { eCSSKeyword_dot, NS_STYLE_TEXT_EMPHASIS_STYLE_DOT },
  { eCSSKeyword_circle, NS_STYLE_TEXT_EMPHASIS_STYLE_CIRCLE },
  { eCSSKeyword_double_circle, NS_STYLE_TEXT_EMPHASIS_STYLE_DOUBLE_CIRCLE },
  { eCSSKeyword_triangle, NS_STYLE_TEXT_EMPHASIS_STYLE_TRIANGLE },
  { eCSSKeyword_sesame, NS_STYLE_TEXT_EMPHASIS_STYLE_SESAME} ,
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextOverflowKTable[] = {
  { eCSSKeyword_clip, NS_STYLE_TEXT_OVERFLOW_CLIP },
  { eCSSKeyword_ellipsis, NS_STYLE_TEXT_OVERFLOW_ELLIPSIS },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextTransformKTable[] = {
  { eCSSKeyword_none, NS_STYLE_TEXT_TRANSFORM_NONE },
  { eCSSKeyword_capitalize, NS_STYLE_TEXT_TRANSFORM_CAPITALIZE },
  { eCSSKeyword_lowercase, NS_STYLE_TEXT_TRANSFORM_LOWERCASE },
  { eCSSKeyword_uppercase, NS_STYLE_TEXT_TRANSFORM_UPPERCASE },
  { eCSSKeyword_full_width, NS_STYLE_TEXT_TRANSFORM_FULLWIDTH },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTouchActionKTable[] = {
  { eCSSKeyword_none,         NS_STYLE_TOUCH_ACTION_NONE },
  { eCSSKeyword_auto,         NS_STYLE_TOUCH_ACTION_AUTO },
  { eCSSKeyword_pan_x,        NS_STYLE_TOUCH_ACTION_PAN_X },
  { eCSSKeyword_pan_y,        NS_STYLE_TOUCH_ACTION_PAN_Y },
  { eCSSKeyword_manipulation, NS_STYLE_TOUCH_ACTION_MANIPULATION },
  { eCSSKeyword_UNKNOWN,      -1 }
};

const KTableEntry nsCSSProps::kTopLayerKTable[] = {
  { eCSSKeyword_none,     NS_STYLE_TOP_LAYER_NONE },
  { eCSSKeyword_top,      NS_STYLE_TOP_LAYER_TOP },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTransformBoxKTable[] = {
  { eCSSKeyword_border_box, NS_STYLE_TRANSFORM_BOX_BORDER_BOX },
  { eCSSKeyword_fill_box, NS_STYLE_TRANSFORM_BOX_FILL_BOX },
  { eCSSKeyword_view_box, NS_STYLE_TRANSFORM_BOX_VIEW_BOX },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTransitionTimingFunctionKTable[] = {
  { eCSSKeyword_ease, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE },
  { eCSSKeyword_linear, NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR },
  { eCSSKeyword_ease_in, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN },
  { eCSSKeyword_ease_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT },
  { eCSSKeyword_ease_in_out, NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT },
  { eCSSKeyword_step_start, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START },
  { eCSSKeyword_step_end, NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kUnicodeBidiKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_UNICODE_BIDI_NORMAL },
  { eCSSKeyword_embed, NS_STYLE_UNICODE_BIDI_EMBED },
  { eCSSKeyword_bidi_override, NS_STYLE_UNICODE_BIDI_OVERRIDE },
  { eCSSKeyword__moz_isolate, NS_STYLE_UNICODE_BIDI_ISOLATE },
  { eCSSKeyword__moz_isolate_override, NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE },
  { eCSSKeyword__moz_plaintext, NS_STYLE_UNICODE_BIDI_PLAINTEXT },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kUserFocusKTable[] = {
  { eCSSKeyword_none,           NS_STYLE_USER_FOCUS_NONE },
  { eCSSKeyword_normal,         NS_STYLE_USER_FOCUS_NORMAL },
  { eCSSKeyword_ignore,         NS_STYLE_USER_FOCUS_IGNORE },
  { eCSSKeyword_select_all,     NS_STYLE_USER_FOCUS_SELECT_ALL },
  { eCSSKeyword_select_before,  NS_STYLE_USER_FOCUS_SELECT_BEFORE },
  { eCSSKeyword_select_after,   NS_STYLE_USER_FOCUS_SELECT_AFTER },
  { eCSSKeyword_select_same,    NS_STYLE_USER_FOCUS_SELECT_SAME },
  { eCSSKeyword_select_menu,    NS_STYLE_USER_FOCUS_SELECT_MENU },
  { eCSSKeyword_UNKNOWN,        -1 }
};

const KTableEntry nsCSSProps::kUserInputKTable[] = {
  { eCSSKeyword_none,     NS_STYLE_USER_INPUT_NONE },
  { eCSSKeyword_auto,     NS_STYLE_USER_INPUT_AUTO },
  { eCSSKeyword_enabled,  NS_STYLE_USER_INPUT_ENABLED },
  { eCSSKeyword_disabled, NS_STYLE_USER_INPUT_DISABLED },
  { eCSSKeyword_UNKNOWN,  -1 }
};

const KTableEntry nsCSSProps::kUserModifyKTable[] = {
  { eCSSKeyword_read_only,  NS_STYLE_USER_MODIFY_READ_ONLY },
  { eCSSKeyword_read_write, NS_STYLE_USER_MODIFY_READ_WRITE },
  { eCSSKeyword_write_only, NS_STYLE_USER_MODIFY_WRITE_ONLY },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kUserSelectKTable[] = {
  { eCSSKeyword_none,       NS_STYLE_USER_SELECT_NONE },
  { eCSSKeyword_auto,       NS_STYLE_USER_SELECT_AUTO },
  { eCSSKeyword_text,       NS_STYLE_USER_SELECT_TEXT },
  { eCSSKeyword_element,    NS_STYLE_USER_SELECT_ELEMENT },
  { eCSSKeyword_elements,   NS_STYLE_USER_SELECT_ELEMENTS },
  { eCSSKeyword_all,        NS_STYLE_USER_SELECT_ALL },
  { eCSSKeyword_toggle,     NS_STYLE_USER_SELECT_TOGGLE },
  { eCSSKeyword_tri_state,  NS_STYLE_USER_SELECT_TRI_STATE },
  { eCSSKeyword__moz_all,   NS_STYLE_USER_SELECT_MOZ_ALL },
  { eCSSKeyword__moz_none,  NS_STYLE_USER_SELECT_NONE },
  { eCSSKeyword__moz_text,  NS_STYLE_USER_SELECT_MOZ_TEXT },
  { eCSSKeyword_UNKNOWN,    -1 }
};

const KTableEntry nsCSSProps::kVerticalAlignKTable[] = {
  { eCSSKeyword_baseline, NS_STYLE_VERTICAL_ALIGN_BASELINE },
  { eCSSKeyword_sub, NS_STYLE_VERTICAL_ALIGN_SUB },
  { eCSSKeyword_super, NS_STYLE_VERTICAL_ALIGN_SUPER },
  { eCSSKeyword_top, NS_STYLE_VERTICAL_ALIGN_TOP },
  { eCSSKeyword_text_top, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP },
  { eCSSKeyword_middle, NS_STYLE_VERTICAL_ALIGN_MIDDLE },
  { eCSSKeyword__moz_middle_with_baseline, NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE },
  { eCSSKeyword_bottom, NS_STYLE_VERTICAL_ALIGN_BOTTOM },
  { eCSSKeyword_text_bottom, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kVisibilityKTable[] = {
  { eCSSKeyword_visible, NS_STYLE_VISIBILITY_VISIBLE },
  { eCSSKeyword_hidden, NS_STYLE_VISIBILITY_HIDDEN },
  { eCSSKeyword_collapse, NS_STYLE_VISIBILITY_COLLAPSE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWhitespaceKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_WHITESPACE_NORMAL },
  { eCSSKeyword_pre, NS_STYLE_WHITESPACE_PRE },
  { eCSSKeyword_nowrap, NS_STYLE_WHITESPACE_NOWRAP },
  { eCSSKeyword_pre_wrap, NS_STYLE_WHITESPACE_PRE_WRAP },
  { eCSSKeyword_pre_line, NS_STYLE_WHITESPACE_PRE_LINE },
  { eCSSKeyword__moz_pre_space, NS_STYLE_WHITESPACE_PRE_SPACE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWidthKTable[] = {
  { eCSSKeyword__moz_max_content, NS_STYLE_WIDTH_MAX_CONTENT },
  { eCSSKeyword__moz_min_content, NS_STYLE_WIDTH_MIN_CONTENT },
  { eCSSKeyword__moz_fit_content, NS_STYLE_WIDTH_FIT_CONTENT },
  { eCSSKeyword__moz_available, NS_STYLE_WIDTH_AVAILABLE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWindowDraggingKTable[] = {
  { eCSSKeyword_drag, NS_STYLE_WINDOW_DRAGGING_DRAG },
  { eCSSKeyword_no_drag, NS_STYLE_WINDOW_DRAGGING_NO_DRAG },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWindowShadowKTable[] = {
  { eCSSKeyword_none, NS_STYLE_WINDOW_SHADOW_NONE },
  { eCSSKeyword_default, NS_STYLE_WINDOW_SHADOW_DEFAULT },
  { eCSSKeyword_menu, NS_STYLE_WINDOW_SHADOW_MENU },
  { eCSSKeyword_tooltip, NS_STYLE_WINDOW_SHADOW_TOOLTIP },
  { eCSSKeyword_sheet, NS_STYLE_WINDOW_SHADOW_SHEET },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWordBreakKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_WORDBREAK_NORMAL },
  { eCSSKeyword_break_all, NS_STYLE_WORDBREAK_BREAK_ALL },
  { eCSSKeyword_keep_all, NS_STYLE_WORDBREAK_KEEP_ALL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWordWrapKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_WORDWRAP_NORMAL },
  { eCSSKeyword_break_word, NS_STYLE_WORDWRAP_BREAK_WORD },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kWritingModeKTable[] = {
  { eCSSKeyword_horizontal_tb, NS_STYLE_WRITING_MODE_HORIZONTAL_TB },
  { eCSSKeyword_vertical_lr, NS_STYLE_WRITING_MODE_VERTICAL_LR },
  { eCSSKeyword_vertical_rl, NS_STYLE_WRITING_MODE_VERTICAL_RL },
  { eCSSKeyword_sideways_lr, NS_STYLE_WRITING_MODE_SIDEWAYS_LR },
  { eCSSKeyword_sideways_rl, NS_STYLE_WRITING_MODE_SIDEWAYS_RL },
  { eCSSKeyword_lr, NS_STYLE_WRITING_MODE_HORIZONTAL_TB },
  { eCSSKeyword_lr_tb, NS_STYLE_WRITING_MODE_HORIZONTAL_TB },
  { eCSSKeyword_rl, NS_STYLE_WRITING_MODE_HORIZONTAL_TB },
  { eCSSKeyword_rl_tb, NS_STYLE_WRITING_MODE_HORIZONTAL_TB },
  { eCSSKeyword_tb, NS_STYLE_WRITING_MODE_VERTICAL_RL },
  { eCSSKeyword_tb_rl, NS_STYLE_WRITING_MODE_VERTICAL_RL },
  { eCSSKeyword_UNKNOWN, -1 }
};

// Specific keyword tables for XUL.properties
const KTableEntry nsCSSProps::kBoxAlignKTable[] = {
  { eCSSKeyword_stretch, NS_STYLE_BOX_ALIGN_STRETCH },
  { eCSSKeyword_start, NS_STYLE_BOX_ALIGN_START },
  { eCSSKeyword_center, NS_STYLE_BOX_ALIGN_CENTER },
  { eCSSKeyword_baseline, NS_STYLE_BOX_ALIGN_BASELINE },
  { eCSSKeyword_end, NS_STYLE_BOX_ALIGN_END },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxDirectionKTable[] = {
  { eCSSKeyword_normal, NS_STYLE_BOX_DIRECTION_NORMAL },
  { eCSSKeyword_reverse, NS_STYLE_BOX_DIRECTION_REVERSE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxOrientKTable[] = {
  { eCSSKeyword_horizontal, NS_STYLE_BOX_ORIENT_HORIZONTAL },
  { eCSSKeyword_vertical, NS_STYLE_BOX_ORIENT_VERTICAL },
  { eCSSKeyword_inline_axis, NS_STYLE_BOX_ORIENT_HORIZONTAL },
  { eCSSKeyword_block_axis, NS_STYLE_BOX_ORIENT_VERTICAL },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kBoxPackKTable[] = {
  { eCSSKeyword_start, NS_STYLE_BOX_PACK_START },
  { eCSSKeyword_center, NS_STYLE_BOX_PACK_CENTER },
  { eCSSKeyword_end, NS_STYLE_BOX_PACK_END },
  { eCSSKeyword_justify, NS_STYLE_BOX_PACK_JUSTIFY },
  { eCSSKeyword_UNKNOWN, -1 }
};

// keyword tables for SVG properties

const KTableEntry nsCSSProps::kDominantBaselineKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_DOMINANT_BASELINE_AUTO },
  { eCSSKeyword_use_script, NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT },
  { eCSSKeyword_no_change, NS_STYLE_DOMINANT_BASELINE_NO_CHANGE },
  { eCSSKeyword_reset_size, NS_STYLE_DOMINANT_BASELINE_RESET_SIZE },
  { eCSSKeyword_alphabetic, NS_STYLE_DOMINANT_BASELINE_ALPHABETIC },
  { eCSSKeyword_hanging, NS_STYLE_DOMINANT_BASELINE_HANGING },
  { eCSSKeyword_ideographic, NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC },
  { eCSSKeyword_mathematical, NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL },
  { eCSSKeyword_central, NS_STYLE_DOMINANT_BASELINE_CENTRAL },
  { eCSSKeyword_middle, NS_STYLE_DOMINANT_BASELINE_MIDDLE },
  { eCSSKeyword_text_after_edge, NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE },
  { eCSSKeyword_text_before_edge, NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFillRuleKTable[] = {
  { eCSSKeyword_nonzero, NS_STYLE_FILL_RULE_NONZERO },
  { eCSSKeyword_evenodd, NS_STYLE_FILL_RULE_EVENODD },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kClipShapeSizingKTable[] = {
  { eCSSKeyword_content_box,   NS_STYLE_CLIP_SHAPE_SIZING_CONTENT },
  { eCSSKeyword_padding_box,   NS_STYLE_CLIP_SHAPE_SIZING_PADDING },
  { eCSSKeyword_border_box,    NS_STYLE_CLIP_SHAPE_SIZING_BORDER },
  { eCSSKeyword_margin_box,    NS_STYLE_CLIP_SHAPE_SIZING_MARGIN },
  { eCSSKeyword_fill_box,      NS_STYLE_CLIP_SHAPE_SIZING_FILL },
  { eCSSKeyword_stroke_box,    NS_STYLE_CLIP_SHAPE_SIZING_STROKE },
  { eCSSKeyword_view_box,      NS_STYLE_CLIP_SHAPE_SIZING_VIEW },
  { eCSSKeyword_UNKNOWN,       -1 }
};

const KTableEntry nsCSSProps::kShapeRadiusKTable[] = {
  { eCSSKeyword_closest_side, NS_RADIUS_CLOSEST_SIDE },
  { eCSSKeyword_farthest_side, NS_RADIUS_FARTHEST_SIDE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kFilterFunctionKTable[] = {
  { eCSSKeyword_blur, NS_STYLE_FILTER_BLUR },
  { eCSSKeyword_brightness, NS_STYLE_FILTER_BRIGHTNESS },
  { eCSSKeyword_contrast, NS_STYLE_FILTER_CONTRAST },
  { eCSSKeyword_grayscale, NS_STYLE_FILTER_GRAYSCALE },
  { eCSSKeyword_invert, NS_STYLE_FILTER_INVERT },
  { eCSSKeyword_opacity, NS_STYLE_FILTER_OPACITY },
  { eCSSKeyword_saturate, NS_STYLE_FILTER_SATURATE },
  { eCSSKeyword_sepia, NS_STYLE_FILTER_SEPIA },
  { eCSSKeyword_hue_rotate, NS_STYLE_FILTER_HUE_ROTATE },
  { eCSSKeyword_drop_shadow, NS_STYLE_FILTER_DROP_SHADOW },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kImageRenderingKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_IMAGE_RENDERING_AUTO },
  { eCSSKeyword_optimizespeed, NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED },
  { eCSSKeyword_optimizequality, NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY },
  { eCSSKeyword__moz_crisp_edges, NS_STYLE_IMAGE_RENDERING_CRISPEDGES },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kMaskTypeKTable[] = {
  { eCSSKeyword_luminance, NS_STYLE_MASK_TYPE_LUMINANCE },
  { eCSSKeyword_alpha, NS_STYLE_MASK_TYPE_ALPHA },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kShapeRenderingKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_SHAPE_RENDERING_AUTO },
  { eCSSKeyword_optimizespeed, NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED },
  { eCSSKeyword_crispedges, NS_STYLE_SHAPE_RENDERING_CRISPEDGES },
  { eCSSKeyword_geometricprecision, NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kStrokeLinecapKTable[] = {
  { eCSSKeyword_butt, NS_STYLE_STROKE_LINECAP_BUTT },
  { eCSSKeyword_round, NS_STYLE_STROKE_LINECAP_ROUND },
  { eCSSKeyword_square, NS_STYLE_STROKE_LINECAP_SQUARE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kStrokeLinejoinKTable[] = {
  { eCSSKeyword_miter, NS_STYLE_STROKE_LINEJOIN_MITER },
  { eCSSKeyword_round, NS_STYLE_STROKE_LINEJOIN_ROUND },
  { eCSSKeyword_bevel, NS_STYLE_STROKE_LINEJOIN_BEVEL },
  { eCSSKeyword_UNKNOWN, -1 }
};

// Lookup table to store the sole objectValue keyword to let SVG glyphs inherit
// certain stroke-* properties from the outer text object
const KTableEntry nsCSSProps::kStrokeContextValueKTable[] = {
  { eCSSKeyword_context_value, NS_STYLE_STROKE_PROP_CONTEXT_VALUE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextAnchorKTable[] = {
  { eCSSKeyword_start, NS_STYLE_TEXT_ANCHOR_START },
  { eCSSKeyword_middle, NS_STYLE_TEXT_ANCHOR_MIDDLE },
  { eCSSKeyword_end, NS_STYLE_TEXT_ANCHOR_END },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kTextRenderingKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_TEXT_RENDERING_AUTO },
  { eCSSKeyword_optimizespeed, NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED },
  { eCSSKeyword_optimizelegibility, NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY },
  { eCSSKeyword_geometricprecision, NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kVectorEffectKTable[] = {
  { eCSSKeyword_none, NS_STYLE_VECTOR_EFFECT_NONE },
  { eCSSKeyword_non_scaling_stroke, NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kColorInterpolationKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_COLOR_INTERPOLATION_AUTO },
  { eCSSKeyword_srgb, NS_STYLE_COLOR_INTERPOLATION_SRGB },
  { eCSSKeyword_linearrgb, NS_STYLE_COLOR_INTERPOLATION_LINEARRGB },
  { eCSSKeyword_UNKNOWN, -1 }
};

const KTableEntry nsCSSProps::kColumnFillKTable[] = {
  { eCSSKeyword_auto, NS_STYLE_COLUMN_FILL_AUTO },
  { eCSSKeyword_balance, NS_STYLE_COLUMN_FILL_BALANCE },
  { eCSSKeyword_UNKNOWN, -1 }
};

static inline bool
IsKeyValSentinel(const KTableEntry& aTableEntry)
{
  return aTableEntry.mKeyword == eCSSKeyword_UNKNOWN &&
         aTableEntry.mValue == -1;
}

int32_t
nsCSSProps::FindIndexOfKeyword(nsCSSKeyword aKeyword,
                               const KTableEntry aTable[])
{
  if (eCSSKeyword_UNKNOWN == aKeyword) {
    // NOTE: we can have keyword tables where eCSSKeyword_UNKNOWN is used
    // not only for the sentinel, but also in the middle of the table to
    // knock out values that have been disabled by prefs, e.g. kDisplayKTable.
    // So we deal with eCSSKeyword_UNKNOWN up front to avoid returning a valid
    // index in the loop below.
    return -1;
  }
  for (int32_t i = 0; ; ++i) {
    const KTableEntry& entry = aTable[i];
    if (::IsKeyValSentinel(entry)) {
      break;
    }
    if (aKeyword == entry.mKeyword) {
      return i;
    }
  }
  return -1;
}

bool
nsCSSProps::FindKeyword(nsCSSKeyword aKeyword, const KTableEntry aTable[],
                        int32_t& aResult)
{
  int32_t index = FindIndexOfKeyword(aKeyword, aTable);
  if (index >= 0) {
    aResult = aTable[index].mValue;
    return true;
  }
  return false;
}

nsCSSKeyword
nsCSSProps::ValueToKeywordEnum(int32_t aValue, const KTableEntry aTable[])
{
#ifdef DEBUG
  typedef decltype(aTable[0].mValue) table_value_type;
  NS_ASSERTION(table_value_type(aValue) == aValue, "Value out of range");
#endif
  for (int32_t i = 0; ; ++i) {
    const KTableEntry& entry = aTable[i];
    if (::IsKeyValSentinel(entry)) {
      break;
    }
    if (aValue == entry.mValue) {
      return entry.mKeyword;
    }
  }
  return eCSSKeyword_UNKNOWN;
}

const nsAFlatCString&
nsCSSProps::ValueToKeyword(int32_t aValue, const KTableEntry aTable[])
{
  nsCSSKeyword keyword = ValueToKeywordEnum(aValue, aTable);
  if (keyword == eCSSKeyword_UNKNOWN) {
    static nsDependentCString sNullStr("");
    return sNullStr;
  } else {
    return nsCSSKeywords::GetStringValue(keyword);
  }
}

/* static */ const KTableEntry* const
nsCSSProps::kKeywordTableTable[eCSSProperty_COUNT_no_shorthands] = {
  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_) \
    kwtable_,
  #define CSS_PROP_LIST_INCLUDE_LOGICAL
  #include "nsCSSPropList.h"
  #undef CSS_PROP_LIST_INCLUDE_LOGICAL
  #undef CSS_PROP
};

const nsAFlatCString&
nsCSSProps::LookupPropertyValue(nsCSSProperty aProp, int32_t aValue)
{
  MOZ_ASSERT(aProp >= 0 && aProp < eCSSProperty_COUNT,
             "property out of range");
#ifdef DEBUG
  typedef decltype(KTableEntry::mValue) table_value_type;
  NS_ASSERTION(table_value_type(aValue) == aValue, "Value out of range");
#endif

  const KTableEntry* kwtable = nullptr;
  if (aProp < eCSSProperty_COUNT_no_shorthands)
    kwtable = kKeywordTableTable[aProp];

  if (kwtable)
    return ValueToKeyword(aValue, kwtable);

  static nsDependentCString sNullStr("");
  return sNullStr;
}

bool nsCSSProps::GetColorName(int32_t aPropValue, nsCString &aStr)
{
  bool rv = false;

  // first get the keyword corresponding to the property Value from the color table
  nsCSSKeyword keyword = ValueToKeywordEnum(aPropValue, kColorKTable);

  // next get the name as a string from the keywords table
  if (keyword != eCSSKeyword_UNKNOWN) {
    nsCSSKeywords::AddRefTable();
    aStr = nsCSSKeywords::GetStringValue(keyword);
    nsCSSKeywords::ReleaseTable();
    rv = true;
  }
  return rv;
}

const nsStyleStructID nsCSSProps::kSIDTable[eCSSProperty_COUNT_no_shorthands] = {
    #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                     kwtable_, stylestruct_, stylestructoffset_, animtype_) \
        eStyleStruct_##stylestruct_,
    #define CSS_PROP_LIST_INCLUDE_LOGICAL

    #include "nsCSSPropList.h"

    #undef CSS_PROP_LIST_INCLUDE_LOGICAL
    #undef CSS_PROP
};

const nsStyleAnimType
nsCSSProps::kAnimTypeTable[eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  animtype_,
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
};

const ptrdiff_t
nsCSSProps::kStyleStructOffsetTable[eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  stylestructoffset_,
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
};

const uint32_t nsCSSProps::kFlagsTable[eCSSProperty_COUNT] = {
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  flags_,
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#include "nsCSSPropList.h"
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) flags_,
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
};

static const nsCSSProperty gAllSubpropTable[] = {
#define CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#define CSS_PROP_LIST_INCLUDE_LOGICAL
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_, \
                 stylestruct_, stylestructoffset_, animtype_)                 \
  eCSSProperty_##id_,
#include "nsCSSPropList.h"
#undef CSS_PROP
#undef CSS_PROP_LIST_INCLUDE_LOGICAL
#undef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gAnimationSubpropTable[] = {
  eCSSProperty_animation_duration,
  eCSSProperty_animation_timing_function,
  eCSSProperty_animation_delay,
  eCSSProperty_animation_direction,
  eCSSProperty_animation_fill_mode,
  eCSSProperty_animation_iteration_count,
  eCSSProperty_animation_play_state,
  // List animation-name last so we serialize it last, in case it has
  // a value that conflicts with one of the other properties.  (See
  // how Declaration::GetValue serializes 'animation'.
  eCSSProperty_animation_name,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRadiusSubpropTable[] = {
  // Code relies on these being in topleft-topright-bottomright-bottomleft
  // order.
  eCSSProperty_border_top_left_radius,
  eCSSProperty_border_top_right_radius,
  eCSSProperty_border_bottom_right_radius,
  eCSSProperty_border_bottom_left_radius,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gOutlineRadiusSubpropTable[] = {
  // Code relies on these being in topleft-topright-bottomright-bottomleft
  // order.
  eCSSProperty__moz_outline_radius_topLeft,
  eCSSProperty__moz_outline_radius_topRight,
  eCSSProperty__moz_outline_radius_bottomRight,
  eCSSProperty__moz_outline_radius_bottomLeft,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBackgroundSubpropTable[] = {
  eCSSProperty_background_color,
  eCSSProperty_background_image,
  eCSSProperty_background_repeat,
  eCSSProperty_background_attachment,
  eCSSProperty_background_position,
  eCSSProperty_background_clip,
  eCSSProperty_background_origin,
  eCSSProperty_background_size,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderSubpropTable[] = {
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style,
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color,
  eCSSProperty_border_top_colors,
  eCSSProperty_border_right_colors,
  eCSSProperty_border_bottom_colors,
  eCSSProperty_border_left_colors,
  eCSSProperty_border_image_source,
  eCSSProperty_border_image_slice,
  eCSSProperty_border_image_width,
  eCSSProperty_border_image_outset,
  eCSSProperty_border_image_repeat,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderBlockEndSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_block_end_width,
  eCSSProperty_border_block_end_style,
  eCSSProperty_border_block_end_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderBlockStartSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_block_start_width,
  eCSSProperty_border_block_start_style,
  eCSSProperty_border_block_start_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderBottomSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_bottom_color,
  eCSSProperty_UNKNOWN
};

static_assert(NS_SIDE_TOP == 0 && NS_SIDE_RIGHT == 1 &&
              NS_SIDE_BOTTOM == 2 && NS_SIDE_LEFT == 3,
              "box side constants not top/right/bottom/left == 0/1/2/3");
static const nsCSSProperty gBorderColorSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  // Code relies on these matching the NS_SIDE_* constants.
  eCSSProperty_border_top_color,
  eCSSProperty_border_right_color,
  eCSSProperty_border_bottom_color,
  eCSSProperty_border_left_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderInlineEndSubpropTable[] = {
  // Declaration.cpp output the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_inline_end_width,
  eCSSProperty_border_inline_end_style,
  eCSSProperty_border_inline_end_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderLeftSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_left_width,
  eCSSProperty_border_left_style,
  eCSSProperty_border_left_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderRightSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_right_width,
  eCSSProperty_border_right_style,
  eCSSProperty_border_right_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderInlineStartSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_inline_start_width,
  eCSSProperty_border_inline_start_style,
  eCSSProperty_border_inline_start_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderStyleSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_style,
  eCSSProperty_border_right_style,
  eCSSProperty_border_bottom_style,
  eCSSProperty_border_left_style,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderTopSubpropTable[] = {
  // Declaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_border_top_width,
  eCSSProperty_border_top_style,
  eCSSProperty_border_top_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderWidthSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_border_top_width,
  eCSSProperty_border_right_width,
  eCSSProperty_border_bottom_width,
  eCSSProperty_border_left_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gFontSubpropTable[] = {
  eCSSProperty_font_family,
  eCSSProperty_font_style,
  eCSSProperty_font_weight,
  eCSSProperty_font_size,
  eCSSProperty_line_height,
  eCSSProperty_font_size_adjust,
  eCSSProperty_font_stretch,
  eCSSProperty__x_system_font,
  eCSSProperty_font_feature_settings,
  eCSSProperty_font_language_override,
  eCSSProperty_font_kerning,
  eCSSProperty_font_synthesis,
  eCSSProperty_font_variant_alternates,
  eCSSProperty_font_variant_caps,
  eCSSProperty_font_variant_east_asian,
  eCSSProperty_font_variant_ligatures,
  eCSSProperty_font_variant_numeric,
  eCSSProperty_font_variant_position,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gFontVariantSubpropTable[] = {
  eCSSProperty_font_variant_alternates,
  eCSSProperty_font_variant_caps,
  eCSSProperty_font_variant_east_asian,
  eCSSProperty_font_variant_ligatures,
  eCSSProperty_font_variant_numeric,
  eCSSProperty_font_variant_position,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gListStyleSubpropTable[] = {
  eCSSProperty_list_style_type,
  eCSSProperty_list_style_image,
  eCSSProperty_list_style_position,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarginSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_margin_top,
  eCSSProperty_margin_right,
  eCSSProperty_margin_bottom,
  eCSSProperty_margin_left,
  eCSSProperty_UNKNOWN
};


static const nsCSSProperty gOutlineSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty_outline_width,
  eCSSProperty_outline_style,
  eCSSProperty_outline_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gColumnsSubpropTable[] = {
  eCSSProperty__moz_column_count,
  eCSSProperty__moz_column_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gColumnRuleSubpropTable[] = {
  // nsCSSDeclaration.cpp outputs the subproperties in this order.
  // It also depends on the color being third.
  eCSSProperty__moz_column_rule_width,
  eCSSProperty__moz_column_rule_style,
  eCSSProperty__moz_column_rule_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gFlexSubpropTable[] = {
  eCSSProperty_flex_grow,
  eCSSProperty_flex_shrink,
  eCSSProperty_flex_basis,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gFlexFlowSubpropTable[] = {
  eCSSProperty_flex_direction,
  eCSSProperty_flex_wrap,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridTemplateSubpropTable[] = {
  eCSSProperty_grid_template_areas,
  eCSSProperty_grid_template_columns,
  eCSSProperty_grid_template_rows,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridSubpropTable[] = {
  eCSSProperty_grid_template_areas,
  eCSSProperty_grid_template_columns,
  eCSSProperty_grid_template_rows,
  eCSSProperty_grid_auto_flow,
  eCSSProperty_grid_auto_columns,
  eCSSProperty_grid_auto_rows,
  eCSSProperty_grid_column_gap, // can only be reset, not get/set
  eCSSProperty_grid_row_gap, // can only be reset, not get/set
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridColumnSubpropTable[] = {
  eCSSProperty_grid_column_start,
  eCSSProperty_grid_column_end,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridRowSubpropTable[] = {
  eCSSProperty_grid_row_start,
  eCSSProperty_grid_row_end,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridAreaSubpropTable[] = {
  eCSSProperty_grid_row_start,
  eCSSProperty_grid_column_start,
  eCSSProperty_grid_row_end,
  eCSSProperty_grid_column_end,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gGridGapSubpropTable[] = {
  eCSSProperty_grid_column_gap,
  eCSSProperty_grid_row_gap,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gOverflowSubpropTable[] = {
  eCSSProperty_overflow_x,
  eCSSProperty_overflow_y,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gPaddingSubpropTable[] = {
  // Code relies on these being in top-right-bottom-left order.
  eCSSProperty_padding_top,
  eCSSProperty_padding_right,
  eCSSProperty_padding_bottom,
  eCSSProperty_padding_left,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gTextDecorationSubpropTable[] = {
  eCSSProperty_text_decoration_color,
  eCSSProperty_text_decoration_line,
  eCSSProperty_text_decoration_style,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gTextEmphasisSubpropTable[] = {
  eCSSProperty_text_emphasis_style,
  eCSSProperty_text_emphasis_color,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gTransitionSubpropTable[] = {
  eCSSProperty_transition_property,
  eCSSProperty_transition_duration,
  eCSSProperty_transition_timing_function,
  eCSSProperty_transition_delay,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gBorderImageSubpropTable[] = {
  eCSSProperty_border_image_source,
  eCSSProperty_border_image_slice,
  eCSSProperty_border_image_width,
  eCSSProperty_border_image_outset,
  eCSSProperty_border_image_repeat,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMarkerSubpropTable[] = {
  eCSSProperty_marker_start,
  eCSSProperty_marker_mid,
  eCSSProperty_marker_end,
  eCSSProperty_UNKNOWN
};

// Subproperty tables for shorthands that are just aliases with
// different parsing rules.
static const nsCSSProperty gMozTransformSubpropTable[] = {
  eCSSProperty_transform,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gScrollSnapTypeSubpropTable[] = {
  eCSSProperty_scroll_snap_type_x,
  eCSSProperty_scroll_snap_type_y,
  eCSSProperty_UNKNOWN
};

const nsCSSProperty *const
nsCSSProps::kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands] = {
#define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
// Need an extra level of macro nesting to force expansion of method_
// params before they get pasted.
#define NSCSSPROPS_INNER_MACRO(method_) g##method_##SubpropTable,
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) \
  NSCSSPROPS_INNER_MACRO(method_)
#include "nsCSSPropList.h"
#undef CSS_PROP_SHORTHAND
#undef NSCSSPROPS_INNER_MACRO
#undef CSS_PROP_PUBLIC_OR_PRIVATE
};


static const nsCSSProperty gOffsetLogicalGroupTable[] = {
  eCSSProperty_top,
  eCSSProperty_right,
  eCSSProperty_bottom,
  eCSSProperty_left,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMaxSizeLogicalGroupTable[] = {
  eCSSProperty_max_height,
  eCSSProperty_max_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gMinSizeLogicalGroupTable[] = {
  eCSSProperty_min_height,
  eCSSProperty_min_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gSizeLogicalGroupTable[] = {
  eCSSProperty_height,
  eCSSProperty_width,
  eCSSProperty_UNKNOWN
};

static const nsCSSProperty gWebkitBoxOrientLogicalGroupTable[] = {
  eCSSProperty_flex_direction,
  eCSSProperty_UNKNOWN
};

const nsCSSProperty* const
nsCSSProps::kLogicalGroupTable[eCSSPropertyLogicalGroup_COUNT] = {
#define CSS_PROP_LOGICAL_GROUP_SHORTHAND(id_) g##id_##SubpropTable,
#define CSS_PROP_LOGICAL_GROUP_AXIS(name_) g##name_##LogicalGroupTable,
#define CSS_PROP_LOGICAL_GROUP_BOX(name_) g##name_##LogicalGroupTable,
#define CSS_PROP_LOGICAL_GROUP_SINGLE(name_) g##name_##LogicalGroupTable,
#include "nsCSSPropLogicalGroupList.h"
#undef CSS_PROP_LOGICAL_GROUP_SINGLE
#undef CSS_PROP_LOGICAL_GROUP_BOX
#undef CSS_PROP_LOGICAL_GROUP_AXIS
#undef CSS_PROP_LOGICAL_GROUP_SHORTHAND
};

// Mapping of logical longhand properties to their logical group (which
// represents the physical longhands the logical properties an correspond
// to).  The format is pairs of values, where the first is the logical
// longhand property (an nsCSSProperty) and the second is the logical group
// (an nsCSSPropertyLogicalGroup), stored in a flat array (like KTableEntry
// arrays).
static const int gLogicalGroupMappingTable[] = {
#define CSS_PROP_LOGICAL(name_, id_, method_, flags_, pref_, parsevariant_, \
                         kwtable_, group_, stylestruct_,                    \
                         stylestructoffset_, animtype_)                     \
  eCSSProperty_##id_, eCSSPropertyLogicalGroup_##group_,
#include "nsCSSPropList.h"
#undef CSS_PROP_LOGICAL
};

/* static */ const nsCSSProperty*
nsCSSProps::LogicalGroup(nsCSSProperty aProperty)
{
  MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
             "out of range");
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty, CSS_PROPERTY_LOGICAL),
             "aProperty must be a logical longhand property");

  for (size_t i = 0; i < ArrayLength(gLogicalGroupMappingTable); i += 2) {
    if (gLogicalGroupMappingTable[i] == aProperty) {
      return kLogicalGroupTable[gLogicalGroupMappingTable[i + 1]];
    }
  }

  MOZ_ASSERT(false, "missing gLogicalGroupMappingTable entry");
  return nullptr;
}


#define ENUM_DATA_FOR_PROPERTY(name_, id_, method_, flags_, pref_,          \
                               parsevariant_, kwtable_, stylestructoffset_, \
                               animtype_)                                   \
  ePropertyIndex_for_##id_,

// The order of these enums must match the g*Flags arrays in nsRuleNode.cpp.

enum FontCheckCounter {
  #define CSS_PROP_FONT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_FONT
  ePropertyCount_for_Font
};

enum DisplayCheckCounter {
  #define CSS_PROP_DISPLAY ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_DISPLAY
  ePropertyCount_for_Display
};

enum VisibilityCheckCounter {
  #define CSS_PROP_VISIBILITY ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_VISIBILITY
  ePropertyCount_for_Visibility
};

enum MarginCheckCounter {
  #define CSS_PROP_MARGIN ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_MARGIN
  ePropertyCount_for_Margin
};

enum BorderCheckCounter {
  #define CSS_PROP_BORDER ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_BORDER
  ePropertyCount_for_Border
};

enum PaddingCheckCounter {
  #define CSS_PROP_PADDING ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_PADDING
  ePropertyCount_for_Padding
};

enum OutlineCheckCounter {
  #define CSS_PROP_OUTLINE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_OUTLINE
  ePropertyCount_for_Outline
};

enum ListCheckCounter {
  #define CSS_PROP_LIST ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_LIST
  ePropertyCount_for_List
};

enum ColorCheckCounter {
  #define CSS_PROP_COLOR ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_COLOR
  ePropertyCount_for_Color
};

enum BackgroundCheckCounter {
  #define CSS_PROP_BACKGROUND ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_BACKGROUND
  ePropertyCount_for_Background
};

enum PositionCheckCounter {
  #define CSS_PROP_POSITION ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_POSITION
  ePropertyCount_for_Position
};

enum TableCheckCounter {
  #define CSS_PROP_TABLE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TABLE
  ePropertyCount_for_Table
};

enum TableBorderCheckCounter {
  #define CSS_PROP_TABLEBORDER ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TABLEBORDER
  ePropertyCount_for_TableBorder
};

enum ContentCheckCounter {
  #define CSS_PROP_CONTENT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_CONTENT
  ePropertyCount_for_Content
};

enum QuotesCheckCounter {
  #define CSS_PROP_QUOTES ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_QUOTES
  ePropertyCount_for_Quotes
};

enum TextCheckCounter {
  #define CSS_PROP_TEXT ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TEXT
  ePropertyCount_for_Text
};

enum TextResetCheckCounter {
  #define CSS_PROP_TEXTRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_TEXTRESET
  ePropertyCount_for_TextReset
};

enum UserInterfaceCheckCounter {
  #define CSS_PROP_USERINTERFACE ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_USERINTERFACE
  ePropertyCount_for_UserInterface
};

enum UIResetCheckCounter {
  #define CSS_PROP_UIRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_UIRESET
  ePropertyCount_for_UIReset
};

enum XULCheckCounter {
  #define CSS_PROP_XUL ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_XUL
  ePropertyCount_for_XUL
};

enum SVGCheckCounter {
  #define CSS_PROP_SVG ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SVG
  ePropertyCount_for_SVG
};

enum SVGResetCheckCounter {
  #define CSS_PROP_SVGRESET ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SVGRESET
  ePropertyCount_for_SVGReset
};

enum ColumnCheckCounter {
  #define CSS_PROP_COLUMN ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_COLUMN
  ePropertyCount_for_Column
};

enum VariablesCheckCounter {
  #define CSS_PROP_VARIABLES ENUM_DATA_FOR_PROPERTY
  #include "nsCSSPropList.h"
  #undef CSS_PROP_VARIABLES
  ePropertyCount_for_Variables
};

#undef ENUM_DATA_FOR_PROPERTY

/* static */ const size_t
nsCSSProps::gPropertyCountInStruct[nsStyleStructID_Length] = {
  #define STYLE_STRUCT(name, checkdata_cb) \
    ePropertyCount_for_##name,
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT
};

/* static */ const size_t
nsCSSProps::gPropertyIndexInStruct[eCSSProperty_COUNT_no_shorthands] = {

  #define CSS_PROP_LOGICAL(name_, id_, method_, flags_, pref_, parsevariant_, \
                           kwtable_, group_, stylestruct_,                    \
                           stylestructoffset_, animtype_)                     \
      size_t(-1),
  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_) \
    ePropertyIndex_for_##id_,
  #include "nsCSSPropList.h"
  #undef CSS_PROP
  #undef CSS_PROP_LOGICAL

};

/* static */ bool
nsCSSProps::gPropertyEnabled[eCSSProperty_COUNT_with_aliases] = {
  // If the property has any "ENABLED_IN" flag set, it is disabled by
  // default. Note that, if a property has pref, whatever its default
  // value is, it will later be changed in nsCSSProps::AddRefTable().
  // If the property has "ENABLED_IN" flags but doesn't have a pref,
  // it is an internal property which is disabled elsewhere.
  #define IS_ENABLED_BY_DEFAULT(flags_) \
    (!((flags_) & CSS_PROPERTY_ENABLED_MASK))

  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_) \
    IS_ENABLED_BY_DEFAULT(flags_),
  #define CSS_PROP_LIST_INCLUDE_LOGICAL
  #include "nsCSSPropList.h"
  #undef CSS_PROP_LIST_INCLUDE_LOGICAL
  #undef CSS_PROP

  #define  CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) \
    IS_ENABLED_BY_DEFAULT(flags_),
  #include "nsCSSPropList.h"
  #undef CSS_PROP_SHORTHAND

  #define CSS_PROP_ALIAS(aliasname_, propid_, aliasmethod_, pref_) \
    true,
  #include "nsCSSPropAliasList.h"
  #undef CSS_PROP_ALIAS

  #undef IS_ENABLED_BY_DEFAULT
};

#include "../../dom/base/PropertyUseCounterMap.inc"

/* static */ const UseCounter
nsCSSProps::gPropertyUseCounter[eCSSProperty_COUNT_no_shorthands] = {
  #define CSS_PROP_PUBLIC_OR_PRIVATE(publicname_, privatename_) privatename_
  #define CSS_PROP_LIST_INCLUDE_LOGICAL
  #define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,     \
                   kwtable_, stylestruct_, stylestructoffset_, animtype_) \
    static_cast<UseCounter>(USE_COUNTER_FOR_CSS_PROPERTY_##method_),
  #include "nsCSSPropList.h"
  #undef CSS_PROP
  #undef CSS_PROP_LIST_INCLUDE_LOGICAL
  #undef CSS_PROP_PUBLIC_OR_PRIVATE
};

// Check that all logical property flags are used appropriately.
#define CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_,         \
                 kwtable_, stylestruct_, stylestructoffset_, animtype_)     \
  static_assert(!((flags_) & CSS_PROPERTY_LOGICAL),                         \
                "only properties defined with CSS_PROP_LOGICAL can use "    \
                "the CSS_PROPERTY_LOGICAL flag");                           \
  static_assert(!((flags_) & CSS_PROPERTY_LOGICAL_AXIS),                    \
                "only properties defined with CSS_PROP_LOGICAL can use "    \
                "the CSS_PROPERTY_LOGICAL_AXIS flag");                      \
  static_assert(!((flags_) & CSS_PROPERTY_LOGICAL_BLOCK_AXIS),              \
                "only properties defined with CSS_PROP_LOGICAL can use "    \
                "the CSS_PROPERTY_LOGICAL_BLOCK_AXIS flag");                \
  static_assert(!((flags_) & CSS_PROPERTY_LOGICAL_END_EDGE),                \
                "only properties defined with CSS_PROP_LOGICAL can use "    \
                "the CSS_PROPERTY_LOGICAL_END_EDGE flag");                  \
  static_assert(!((flags_) & CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING),\
                "only properties defined with CSS_PROP_LOGICAL can use "    \
                "the CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING flag");
#define CSS_PROP_LOGICAL(name_, id_, method_, flags_, pref_, parsevariant_, \
                         kwtable_, group_, stylestruct_,                    \
                         stylestructoffset_, animtype_)                     \
  static_assert((flags_) & CSS_PROPERTY_LOGICAL,                            \
                "properties defined with CSS_PROP_LOGICAL must also use "   \
                "the CSS_PROPERTY_LOGICAL flag");                           \
  static_assert(!((flags_) & CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED),    \
                "CSS_PROPERTY_IGNORED_WHEN_COLORS_DISABLED has no effect "  \
                "on logical properties");                                   \
  static_assert(!(((flags_) & CSS_PROPERTY_LOGICAL_AXIS) &&                 \
                  ((flags_) & CSS_PROPERTY_LOGICAL_END_EDGE)),              \
                "CSS_PROPERTY_LOGICAL_END_EDGE makes no sense when used "   \
                "with CSS_PROPERTY_LOGICAL_AXIS");                          \
  /* Make sure CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING isn't used */  \
  /* with other mutually-exclusive flags: */                                \
  static_assert(!(((flags_) & CSS_PROPERTY_LOGICAL_AXIS) &&                 \
                  ((flags_) & CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING)),\
                "CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING makes no "   \
                "sense when used with CSS_PROPERTY_LOGICAL_AXIS");          \
  static_assert(!(((flags_) & CSS_PROPERTY_LOGICAL_BLOCK_AXIS) &&           \
                  ((flags_) & CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING)),\
                "CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING makes no "   \
                "sense when used with CSS_PROPERTY_LOGICAL_BLOCK_AXIS");    \
  static_assert(!(((flags_) & CSS_PROPERTY_LOGICAL_END_EDGE) &&             \
                  ((flags_) & CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING)),\
                "CSS_PROPERTY_LOGICAL_SINGLE_CUSTOM_VALMAPPING makes no "   \
                "sense when used with CSS_PROPERTY_LOGICAL_END_EDGE");
#include "nsCSSPropList.h"
#undef CSS_PROP_LOGICAL
#undef CSS_PROP

#include "nsCSSPropsGenerated.inc"
