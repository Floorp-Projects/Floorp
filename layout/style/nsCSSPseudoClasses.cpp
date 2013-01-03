/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

#include "mozilla/Util.h"

#include "nsCSSPseudoClasses.h"
#include "nsAtomListUtils.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// define storage for all atoms
#define CSS_PSEUDO_CLASS(_name, _value, _pref) \
  static nsIAtom* sPseudoClass_##_name;
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

#define CSS_PSEUDO_CLASS(name_, value_, pref_)  \
  NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

static const nsStaticAtom CSSPseudoClasses_info[] = {
#define CSS_PSEUDO_CLASS(name_, value_, pref_)            \
  NS_STATIC_ATOM(name_##_buffer, &sPseudoClass_##name_),
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
};

static bool sPseudoClassEnabled[] = {
#define CSS_PSEUDO_CLASS(name_, value_, pref_)            \
  true,
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
};  

void nsCSSPseudoClasses::AddRefAtoms()
{
  NS_RegisterStaticAtoms(CSSPseudoClasses_info);
  
#define CSS_PSEUDO_CLASS(name_, value_, pref_)                               \
  if (pref_[0]) {                                                            \
    Preferences::AddBoolVarCache(&sPseudoClassEnabled[ePseudoClass_##name_], \
                                 pref_);                                     \
  }
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
}

bool
nsCSSPseudoClasses::HasStringArg(Type aType)
{
  return aType == ePseudoClass_lang ||
         aType == ePseudoClass_mozEmptyExceptChildrenWithLocalname ||
         aType == ePseudoClass_mozSystemMetric ||
         aType == ePseudoClass_mozLocaleDir ||
         aType == ePseudoClass_dir;
}

bool
nsCSSPseudoClasses::HasNthPairArg(Type aType)
{
  return aType == ePseudoClass_nthChild ||
         aType == ePseudoClass_nthLastChild ||
         aType == ePseudoClass_nthOfType ||
         aType == ePseudoClass_nthLastOfType;
}

void
nsCSSPseudoClasses::PseudoTypeToString(Type aType, nsAString& aString)
{
  NS_ABORT_IF_FALSE(aType < ePseudoClass_Count, "Unexpected type");
  NS_ABORT_IF_FALSE(aType >= 0, "Very unexpected type");
  (*CSSPseudoClasses_info[aType].mAtom)->ToString(aString);
}

nsCSSPseudoClasses::Type
nsCSSPseudoClasses::GetPseudoType(nsIAtom* aAtom)
{
  for (uint32_t i = 0; i < ArrayLength(CSSPseudoClasses_info); ++i) {
    if (*CSSPseudoClasses_info[i].mAtom == aAtom) {
      return sPseudoClassEnabled[i] ? Type(i) : ePseudoClass_NotPseudoClass;
    }
  }

  return nsCSSPseudoClasses::ePseudoClass_NotPseudoClass;
}
