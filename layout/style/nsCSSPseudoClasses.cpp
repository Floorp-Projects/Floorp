/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-classes */

#include "mozilla/ArrayUtils.h"

#include "nsCSSPseudoClasses.h"
#include "nsStaticAtom.h"
#include "mozilla/Preferences.h"
#include "nsString.h"

using namespace mozilla;

// define storage for all atoms
#define CSS_PSEUDO_CLASS(_name, _value, _flags, _pref) \
  static nsIAtom* sPseudoClass_##_name;
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  NS_STATIC_ATOM_BUFFER(name_##_pseudo_class_buffer, value_)
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS

// Array of nsStaticAtom for each of the pseudo-classes.
static const nsStaticAtom CSSPseudoClasses_info[] = {
#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  NS_STATIC_ATOM(name_##_pseudo_class_buffer, &sPseudoClass_##name_),
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
};

// Flags data for each of the pseudo-classes, which must be separate
// from the previous array since there's no place for it in
// nsStaticAtom.
static const uint32_t CSSPseudoClasses_flags[] = {
#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  flags_,
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
};

static bool sPseudoClassEnabled[] = {
#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_) \
  true,
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
};

void nsCSSPseudoClasses::AddRefAtoms()
{
  NS_RegisterStaticAtoms(CSSPseudoClasses_info);

#define CSS_PSEUDO_CLASS(name_, value_, flags_, pref_)                        \
  if (pref_[0]) {                                                             \
    auto idx = static_cast<CSSPseudoElementTypeBase>(Type::name_);            \
    Preferences::AddBoolVarCache(&sPseudoClassEnabled[idx], pref_);           \
  }
#include "nsCSSPseudoClassList.h"
#undef CSS_PSEUDO_CLASS
}

bool
nsCSSPseudoClasses::HasStringArg(Type aType)
{
  return aType == Type::lang ||
         aType == Type::mozEmptyExceptChildrenWithLocalname ||
         aType == Type::mozSystemMetric ||
         aType == Type::mozLocaleDir ||
         aType == Type::dir;
}

bool
nsCSSPseudoClasses::HasNthPairArg(Type aType)
{
  return aType == Type::nthChild ||
         aType == Type::nthLastChild ||
         aType == Type::nthOfType ||
         aType == Type::nthLastOfType;
}

void
nsCSSPseudoClasses::PseudoTypeToString(Type aType, nsAString& aString)
{
  MOZ_ASSERT(aType < Type::Count, "Unexpected type");
  auto idx = static_cast<CSSPseudoClassTypeBase>(aType);
  (*CSSPseudoClasses_info[idx].mAtom)->ToString(aString);
}

CSSPseudoClassType
nsCSSPseudoClasses::GetPseudoType(nsIAtom* aAtom)
{
  for (uint32_t i = 0; i < ArrayLength(CSSPseudoClasses_info); ++i) {
    if (*CSSPseudoClasses_info[i].mAtom == aAtom) {
      return sPseudoClassEnabled[i] ? Type(i) : Type::NotPseudo;
    }
  }

  return Type::NotPseudo;
}

/* static */ bool
nsCSSPseudoClasses::IsUserActionPseudoClass(Type aType)
{
  // See http://dev.w3.org/csswg/selectors4/#useraction-pseudos
  return aType == Type::hover ||
         aType == Type::active ||
         aType == Type::focus;
}

/* static */ uint32_t
nsCSSPseudoClasses::FlagsForPseudoClass(const Type aType)
{
  size_t index = static_cast<size_t>(aType);
  NS_ASSERTION(index < ArrayLength(CSSPseudoClasses_flags),
               "argument must be a pseudo-class");
  return CSSPseudoClasses_flags[index];
}

