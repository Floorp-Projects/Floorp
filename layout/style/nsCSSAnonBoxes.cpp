/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#include "mozilla/ArrayUtils.h"

#include "nsCSSAnonBoxes.h"

using namespace mozilla;

namespace mozilla {
namespace detail {

MOZ_PUSH_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING
extern constexpr CSSAnonBoxAtoms gCSSAnonBoxAtoms = {
  #define CSS_ANON_BOX(name_, value_) \
    NS_STATIC_ATOM_INIT_STRING(value_)
  #include "nsCSSAnonBoxList.h"
  #undef CSS_ANON_BOX
  {
    #define CSS_ANON_BOX(name_, value_) \
      NS_STATIC_ATOM_INIT_ATOM( \
        nsICSSAnonBoxPseudo, CSSAnonBoxAtoms, name_, value_)
    #include "nsCSSAnonBoxList.h"
    #undef CSS_ANON_BOX
  }
};
MOZ_POP_DISABLE_INTEGRAL_CONSTANT_OVERFLOW_WARNING

} // namespace detail
} // namespace mozilla

// Non-inheriting boxes must come first in nsCSSAnonBoxList.h so that
// `NonInheriting` values can index into this array and other similar arrays.
const nsStaticAtom* const nsCSSAnonBoxes::sAtoms =
  mozilla::detail::gCSSAnonBoxAtoms.mAtoms;

#define CSS_ANON_BOX(name_, value_) \
  NS_STATIC_ATOM_DEFN_PTR( \
    nsICSSAnonBoxPseudo, mozilla::detail::CSSAnonBoxAtoms, \
    mozilla::detail::gCSSAnonBoxAtoms, nsCSSAnonBoxes, name_)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

void nsCSSAnonBoxes::RegisterStaticAtoms()
{
  NS_RegisterStaticAtoms(sAtoms, sAtomsLen);
}

bool nsCSSAnonBoxes::IsAnonBox(nsAtom *aAtom)
{
  return nsStaticAtomUtils::IsMember(aAtom, sAtoms, sAtomsLen);
}

#ifdef MOZ_XUL
/* static */ bool
nsCSSAnonBoxes::IsTreePseudoElement(nsAtom* aPseudo)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudo));
  return StringBeginsWith(nsDependentAtomString(aPseudo),
                          NS_LITERAL_STRING(":-moz-tree-"));
}
#endif

/* static*/ nsCSSAnonBoxes::NonInheriting
nsCSSAnonBoxes::NonInheritingTypeForPseudoTag(nsAtom* aPseudo)
{
  MOZ_ASSERT(IsNonInheritingAnonBox(aPseudo));
  Maybe<uint32_t> index = nsStaticAtomUtils::Lookup(aPseudo, sAtoms, sAtomsLen);
  MOZ_RELEASE_ASSERT(index.isSome());
  return static_cast<NonInheriting>(*index);
}
