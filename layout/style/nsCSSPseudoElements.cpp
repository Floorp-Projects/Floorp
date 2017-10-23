/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#include "nsCSSPseudoElements.h"

#include "mozilla/ArrayUtils.h"

#include "nsAtomListUtils.h"
#include "nsStaticAtom.h"
#include "nsCSSAnonBoxes.h"
#include "nsDOMString.h"

using namespace mozilla;

#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  NS_STATIC_ATOM_BUFFER(name_, value_)
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  NS_STATIC_ATOM_SUBCLASS_DEFN(nsICSSPseudoElement, nsCSSPseudoElements, name_)
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

// Array of nsStaticAtomSetup for each of the pseudo-elements.
static const nsStaticAtomSetup sCSSPseudoElementAtomSetup[] = {
  #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
    NS_STATIC_ATOM_SUBCLASS_SETUP(nsCSSPseudoElements, name_)
  #include "nsCSSPseudoElementList.h"
  #undef CSS_PSEUDO_ELEMENT
};

// Flags data for each of the pseudo-elements, which must be separate
// from the previous array since there's no place for it in
// nsStaticAtomSetup.
/* static */ const uint32_t
nsCSSPseudoElements::kPseudoElementFlags[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  flags_,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

void nsCSSPseudoElements::AddRefAtoms()
{
  NS_RegisterStaticAtoms(sCSSPseudoElementAtomSetup);
}

bool nsCSSPseudoElements::IsPseudoElement(nsAtom *aAtom)
{
  return nsAtomListUtils::IsMember(aAtom, sCSSPseudoElementAtomSetup,
                                   ArrayLength(sCSSPseudoElementAtomSetup));
}

/* static */ bool
nsCSSPseudoElements::IsCSS2PseudoElement(nsAtom *aAtom)
{
  // We don't implement this using PseudoElementHasFlags because callers
  // want to pass things that could be anon boxes.
  NS_ASSERTION(nsCSSPseudoElements::IsPseudoElement(aAtom) ||
               nsCSSAnonBoxes::IsAnonBox(aAtom),
               "must be pseudo element or anon box");
  bool result = aAtom == nsCSSPseudoElements::after ||
                  aAtom == nsCSSPseudoElements::before ||
                  aAtom == nsCSSPseudoElements::firstLetter ||
                  aAtom == nsCSSPseudoElements::firstLine;
  NS_ASSERTION(nsCSSAnonBoxes::IsAnonBox(aAtom) ||
               result == PseudoElementHasFlags(
                   GetPseudoType(aAtom, EnabledState::eIgnoreEnabledState),
                   CSS_PSEUDO_ELEMENT_IS_CSS2),
               "result doesn't match flags");
  return result;
}

/* static */ CSSPseudoElementType
nsCSSPseudoElements::GetPseudoType(nsAtom *aAtom, EnabledState aEnabledState)
{
  for (CSSPseudoElementTypeBase i = 0;
       i < ArrayLength(sCSSPseudoElementAtomSetup);
       ++i) {
    if (*sCSSPseudoElementAtomSetup[i].mAtom == aAtom) {
      auto type = static_cast<Type>(i);
      // ::moz-placeholder is an alias for ::placeholder
      if (type == CSSPseudoElementType::mozPlaceholder) {
        type = CSSPseudoElementType::placeholder;
      }
      return IsEnabled(type, aEnabledState) ? type : Type::NotPseudo;
    }
  }

  if (nsCSSAnonBoxes::IsAnonBox(aAtom)) {
#ifdef MOZ_XUL
    if (nsCSSAnonBoxes::IsTreePseudoElement(aAtom)) {
      return Type::XULTree;
    }
#endif

    if (nsCSSAnonBoxes::IsNonInheritingAnonBox(aAtom)) {
      return Type::NonInheritingAnonBox;
    }

    return Type::InheritingAnonBox;
  }

  return Type::NotPseudo;
}

/* static */ nsAtom*
nsCSSPseudoElements::GetPseudoAtom(Type aType)
{
  NS_ASSERTION(aType < Type::Count, "Unexpected type");
  return *sCSSPseudoElementAtomSetup[
    static_cast<CSSPseudoElementTypeBase>(aType)].mAtom;
}

/* static */ already_AddRefed<nsAtom>
nsCSSPseudoElements::GetPseudoAtom(const nsAString& aPseudoElement)
{
  if (DOMStringIsNull(aPseudoElement) || aPseudoElement.IsEmpty() ||
      aPseudoElement.First() != char16_t(':')) {
    return nullptr;
  }

  // deal with two-colon forms of aPseudoElt
  nsAString::const_iterator start, end;
  aPseudoElement.BeginReading(start);
  aPseudoElement.EndReading(end);
  NS_ASSERTION(start != end, "aPseudoElement is not empty!");
  ++start;
  bool haveTwoColons = true;
  if (start == end || *start != char16_t(':')) {
    --start;
    haveTwoColons = false;
  }
  RefPtr<nsAtom> pseudo = NS_Atomize(Substring(start, end));
  MOZ_ASSERT(pseudo);

  // There aren't any non-CSS2 pseudo-elements with a single ':'
  if (!haveTwoColons &&
      (!IsPseudoElement(pseudo) || !IsCSS2PseudoElement(pseudo))) {
    // XXXbz I'd really rather we threw an exception or something, but
    // the DOM spec sucks.
    return nullptr;
  }

  return pseudo.forget();
}

/* static */ bool
nsCSSPseudoElements::PseudoElementSupportsUserActionState(const Type aType)
{
  return PseudoElementHasFlags(aType,
                               CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE);
}
