/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#include "nsCSSPseudoElements.h"

#include "mozilla/ArrayUtils.h"

#include "nsCSSAnonBoxes.h"
#include "nsDOMString.h"
#include "nsGkAtomConsts.h"
#include "nsStaticAtomUtils.h"

using namespace mozilla;

// Flags data for each of the pseudo-elements.
/* static */ const uint32_t nsCSSPseudoElements::kPseudoElementFlags[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) flags_,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

/* static */
nsStaticAtom* nsCSSPseudoElements::GetAtomBase() {
  return const_cast<nsStaticAtom*>(
      nsGkAtoms::GetAtomByIndex(kAtomIndex_PseudoElements));
}

/* static */
nsAtom* nsCSSPseudoElements::GetPseudoAtom(Type aType) {
  MOZ_ASSERT(PseudoStyle::IsPseudoElement(aType), "Unexpected type");
  size_t index = kAtomIndex_PseudoElements + static_cast<size_t>(aType);
  return nsGkAtoms::GetAtomByIndex(index);
}

/* static */
Maybe<PseudoStyleType> nsCSSPseudoElements::GetPseudoType(
    const nsAString& aPseudoElement, CSSEnabledState aEnabledState) {
  if (DOMStringIsNull(aPseudoElement) || aPseudoElement.IsEmpty()) {
    return Some(PseudoStyleType::NotPseudo);
  }

  if (aPseudoElement.First() != char16_t(':')) {
    return Nothing();
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

  Maybe<uint32_t> index = nsStaticAtomUtils::Lookup(pseudo, GetAtomBase(),
                                                    kAtomCount_PseudoElements);
  if (index.isNothing()) {
    return Nothing();
  }
  auto type = static_cast<Type>(*index);
  if (!haveTwoColons &&
      !PseudoElementHasFlags(type, CSS_PSEUDO_ELEMENT_IS_CSS2)) {
    return Nothing();
  }
  return IsEnabled(type, aEnabledState) ? Some(type) : Nothing();
}

/* static */
bool nsCSSPseudoElements::PseudoElementSupportsUserActionState(
    const Type aType) {
  return PseudoElementHasFlags(aType,
                               CSS_PSEUDO_ELEMENT_SUPPORTS_USER_ACTION_STATE);
}

/* static */
nsString nsCSSPseudoElements::PseudoTypeAsString(Type aPseudoType) {
  switch (aPseudoType) {
    case PseudoStyleType::before:
      return u"::before"_ns;
    case PseudoStyleType::after:
      return u"::after"_ns;
    case PseudoStyleType::marker:
      return u"::marker"_ns;
    default:
      MOZ_ASSERT(aPseudoType == PseudoStyleType::NotPseudo,
                 "Unexpected pseudo type");
      return u""_ns;
  }
}

#ifdef DEBUG
/* static */
void nsCSSPseudoElements::AssertAtoms() {
  nsStaticAtom* base = GetAtomBase();
#  define CSS_PSEUDO_ELEMENT(name_, value_, flags_)                   \
    {                                                                 \
      RefPtr<nsAtom> atom = NS_Atomize(value_);                       \
      size_t index = static_cast<size_t>(PseudoStyleType::name_);     \
      MOZ_ASSERT(atom == nsGkAtoms::PseudoElement_##name_,            \
                 "Static atom for " #name_ " has incorrect value");   \
      MOZ_ASSERT(atom == &base[index],                                \
                 "Static atom for " #name_ " not at expected index"); \
    }
#  include "nsCSSPseudoElementList.h"
#  undef CSS_PSEUDO_ELEMENT
}
#endif
