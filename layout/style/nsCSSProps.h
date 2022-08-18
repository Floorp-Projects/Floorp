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
#include <ostream>
#include <type_traits>

#include "nsString.h"
#include "nsCSSPropertyID.h"
#include "nsStyleStructFwd.h"
#include "mozilla/UseCounter.h"
#include "mozilla/CSSEnabledState.h"
#include "mozilla/CSSPropFlags.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/Preferences.h"
#include "mozilla/gfx/gfxVarReceiver.h"
#include "nsXULAppAPI.h"

// Length of the "--" prefix on custom names (such as custom property names,
// and, in the future, custom media query names).
#define CSS_CUSTOM_NAME_PREFIX_LENGTH 2

namespace mozilla {
class ComputedStyle;
}

extern "C" {
nsCSSPropertyID Servo_ResolveLogicalProperty(nsCSSPropertyID,
                                             const mozilla::ComputedStyle*);
nsCSSPropertyID Servo_Property_LookupEnabledForAllContent(const nsACString*);
const uint8_t* Servo_Property_GetName(nsCSSPropertyID, uint32_t* aLength);
}

class nsCSSProps {
 public:
  typedef mozilla::CSSEnabledState EnabledState;
  typedef mozilla::CSSPropFlags Flags;

  static void AddRefTable(void);
  static void ReleaseTable(void);

  // Looks up the property with name aProperty and returns its corresponding
  // nsCSSPropertyID value.  If aProperty is the name of a custom property,
  // then eCSSPropertyExtra_variable will be returned.
  //
  // This only returns properties enabled for all content, and resolves aliases
  // to return the aliased property.
  static nsCSSPropertyID LookupProperty(const nsACString& aProperty) {
    return Servo_Property_LookupEnabledForAllContent(&aProperty);
  }

  // As above, but looked up using a property's IDL name.
  // eCSSPropertyExtra_variable won't be returned from this method.
  static nsCSSPropertyID LookupPropertyByIDLName(
      const nsACString& aPropertyIDLName, EnabledState aEnabled);

  // Returns whether aProperty is a custom property name, i.e. begins with
  // "--".  This assumes that the CSS Variables pref has been enabled.
  static bool IsCustomPropertyName(const nsACString& aProperty);

  static bool IsShorthand(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return (aProperty >= eCSSProperty_COUNT_no_shorthands);
  }

  // Same but for @font-face descriptors
  static nsCSSFontDesc LookupFontDesc(const nsACString& aProperty);

  // The relevant invariants are asserted in Document.cpp
  static mozilla::UseCounter UseCounterFor(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_with_aliases,
               "out of range");
    return mozilla::UseCounter(size_t(mozilla::eUseCounter_FirstCSSProperty) +
                               size_t(aProperty));
  }

  // Given a property enum, get the string value
  //
  // This string is static.
  static const nsDependentCSubstring GetStringValue(nsCSSPropertyID aProperty) {
    uint32_t len;
    const uint8_t* chars = Servo_Property_GetName(aProperty, &len);
    return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
  }

  static const nsCString& GetStringValue(nsCSSFontDesc aFontDesc);
  static const nsCString& GetStringValue(nsCSSCounterDesc aCounterDesc);

  static bool PropHasFlags(nsCSSPropertyID aProperty, Flags aFlags);

  static nsCSSPropertyID Physicalize(nsCSSPropertyID aProperty,
                                     const mozilla::ComputedStyle& aStyle) {
    MOZ_ASSERT(!IsShorthand(aProperty));
    if (PropHasFlags(aProperty, Flags::IsLogical)) {
      return Servo_ResolveLogicalProperty(aProperty, &aStyle);
    }
    return aProperty;
  }

 private:
  // A table for shorthand properties.  The appropriate index is the
  // property ID minus eCSSProperty_COUNT_no_shorthands.
  static const nsCSSPropertyID* const
      kSubpropertyTable[eCSSProperty_COUNT - eCSSProperty_COUNT_no_shorthands];

 public:
  /**
   * Returns true if the backdrop-filter pref and the gfx blocklist are enabled.
   */
  static bool IsBackdropFilterAvailable(JSContext*, JSObject*) {
    return IsEnabled(eCSSProperty_backdrop_filter, EnabledState::ForAllContent);
  }

  /**
   * Recoumputes the enabled state of a pref. If aPrefName is nullptr,
   * recomputes the state of all prefs in gPropertyEnabled.
   * aClosure is the pref callback closure data, which is not used.
   */
  static void RecomputeEnabledState(const char* aPrefName,
                                    void* aClosure = nullptr);

  /**
   * Retrieve a singleton receiver to register with gfxVars
   */
  static mozilla::gfx::gfxVarReceiver& GfxVarReceiver();

  static const nsCSSPropertyID* SubpropertyEntryFor(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(eCSSProperty_COUNT_no_shorthands <= aProperty &&
                   aProperty < eCSSProperty_COUNT,
               "out of range");
    return kSubpropertyTable[aProperty - eCSSProperty_COUNT_no_shorthands];
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
  static const char* PropertyIDLName(nsCSSPropertyID aProperty) {
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
  static int32_t PropertyIDLNameSortPosition(nsCSSPropertyID aProperty) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT,
               "out of range");
    return kIDLNameSortPositionTable[aProperty];
  }

 public:
  static bool IsEnabled(nsCSSPropertyID aProperty, EnabledState aEnabled) {
    MOZ_ASSERT(0 <= aProperty && aProperty < eCSSProperty_COUNT_with_aliases,
               "out of range");
    // In the child process, assert that we're not trying to parse stylesheets
    // before we've gotten all our prefs.
    MOZ_ASSERT_IF(!XRE_IsParentProcess(),
                  mozilla::Preferences::ArePrefsInitedInContentProcess());
    if (gPropertyEnabled[aProperty]) {
      return true;
    }
    if (aEnabled == EnabledState::IgnoreEnabledState) {
      return true;
    }
    if ((aEnabled & EnabledState::InUASheets) &&
        PropHasFlags(aProperty, Flags::EnabledInUASheets)) {
      return true;
    }
    if ((aEnabled & EnabledState::InChrome) &&
        PropHasFlags(aProperty, Flags::EnabledInChrome)) {
      return true;
    }
    return false;
  }

 public:
  struct PropertyPref {
    nsCSSPropertyID mPropID;
    const char* mPref;
  };
  static const PropertyPref kPropertyPrefTable[];

// Storing the enabledstate_ value in an nsCSSPropertyID variable is a small
// hack to avoid needing a separate variable declaration for its real type
// (CSSEnabledState), which would then require using a block and
// therefore a pair of macros by consumers for the start and end of the loop.
#define CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(it_, prop_, enabledstate_)  \
  for (const nsCSSPropertyID *                                           \
           it_ = nsCSSProps::SubpropertyEntryFor(prop_),                 \
          es_ = (nsCSSPropertyID)((enabledstate_) | CSSEnabledState(0)); \
       *it_ != eCSSProperty_UNKNOWN; ++it_)                              \
    if (nsCSSProps::IsEnabled(*it_, (mozilla::CSSEnabledState)es_))
};

// MOZ_DBG support for nsCSSPropertyID

inline std::ostream& operator<<(std::ostream& aOut, nsCSSPropertyID aProperty) {
  return aOut << nsCSSProps::GetStringValue(aProperty);
}

#endif /* nsCSSProps_h___ */
