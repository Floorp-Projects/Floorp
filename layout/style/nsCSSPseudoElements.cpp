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

using namespace mozilla;

namespace mozilla {
namespace detail {

MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
extern constexpr CSSPseudoElementAtoms gCSSPseudoElementAtoms = {
  #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
    NS_STATIC_ATOM_INIT_STRING(value_)
  #include "nsCSSPseudoElementList.h"
  #undef CSS_PSEUDO_ELEMENT
  {
    #define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
      NS_STATIC_ATOM_INIT_ATOM( \
        nsICSSPseudoElement, CSSPseudoElementAtoms, name_, value_)
    #include "nsCSSPseudoElementList.h"
    #undef CSS_PSEUDO_ELEMENT
  }
};
MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING

} // namespace detail
} // namespace mozilla

const nsStaticAtom* const nsCSSPseudoElements::sAtoms =
  mozilla::detail::gCSSPseudoElementAtoms.mAtoms;

#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  NS_STATIC_ATOM_DEFN_PTR( \
    nsICSSPseudoElement, mozilla::detail::CSSPseudoElementAtoms, \
    mozilla::detail::gCSSPseudoElementAtoms, nsCSSPseudoElements, name_);
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

// Flags data for each of the pseudo-elements.
/* static */ const uint32_t
nsCSSPseudoElements::kPseudoElementFlags[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  flags_,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

void nsCSSPseudoElements::RegisterStaticAtoms()
{
  NS_RegisterStaticAtoms(sAtoms, sAtomsLen);
}

bool nsCSSPseudoElements::IsPseudoElement(nsAtom *aAtom)
{
  return nsStaticAtomUtils::IsMember(aAtom, sAtoms, sAtomsLen);
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
nsCSSPseudoElements::GetPseudoType(nsAtom* aAtom, EnabledState aEnabledState)
{
  Maybe<uint32_t> index = nsStaticAtomUtils::Lookup(aAtom, sAtoms, sAtomsLen);
  if (index.isSome()) {
    auto type = static_cast<Type>(*index);
    return IsEnabled(type, aEnabledState) ? type : Type::NotPseudo;
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
  MOZ_ASSERT(aType < Type::Count, "Unexpected type");
  return const_cast<nsStaticAtom*>(
    &sAtoms[static_cast<CSSPseudoElementTypeBase>(aType)]);
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

/* static */ nsString
nsCSSPseudoElements::PseudoTypeAsString(Type aPseudoType)
{
  switch (aPseudoType) {
    case CSSPseudoElementType::before:
      return NS_LITERAL_STRING("::before");
    case CSSPseudoElementType::after:
      return NS_LITERAL_STRING("::after");
    default:
      MOZ_ASSERT(aPseudoType == CSSPseudoElementType::NotPseudo,
                 "Unexpected pseudo type");
      return EmptyString();
  }
}

