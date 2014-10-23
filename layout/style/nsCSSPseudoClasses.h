/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

#ifndef nsCSSPseudoClasses_h___
#define nsCSSPseudoClasses_h___

#include "nsStringFwd.h"

// This pseudo-element is accepted only in UA style sheets.
#define CSS_PSEUDO_CLASS_UA_SHEET_ONLY                 (1<<0)

class nsIAtom;

class nsCSSPseudoClasses {
public:

  static void AddRefAtoms();

  enum Type {
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
    ePseudoClass_##_name,
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
    ePseudoClass_Count,
    ePseudoClass_NotPseudoClass /* This value MUST be last!  SelectorMatches
                                   depends on it. */
  };

  static Type GetPseudoType(nsIAtom* aAtom);
  static bool HasStringArg(Type aType);
  static bool HasNthPairArg(Type aType);
  static bool HasSelectorListArg(Type aType) {
    return aType == ePseudoClass_any;
  }
  static bool IsUserActionPseudoClass(Type aType);

  static bool PseudoClassIsUASheetOnly(Type aType) {
    return PseudoClassHasFlags(aType, CSS_PSEUDO_CLASS_UA_SHEET_ONLY);
  }

  // Should only be used on types other than Count and NotPseudoClass
  static void PseudoTypeToString(Type aType, nsAString& aString);

private:
  static uint32_t FlagsForPseudoClass(const Type aType);

  // Does the given pseudo-class have all of the flags given?
  static bool PseudoClassHasFlags(const Type aType, uint32_t aFlags)
  {
    return (FlagsForPseudoClass(aType) & aFlags) == aFlags;
  }
};

#endif /* nsCSSPseudoClasses_h___ */
