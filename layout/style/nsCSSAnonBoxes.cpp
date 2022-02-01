/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#include "mozilla/ArrayUtils.h"

#include "nsCSSAnonBoxes.h"
#include "nsGkAtomConsts.h"
#include "nsStaticAtomUtils.h"

using namespace mozilla;

/* static */
bool nsCSSAnonBoxes::IsTreePseudoElement(nsAtom* aPseudo) {
  return StringBeginsWith(nsDependentAtomString(aPseudo), u":-moz-tree-"_ns);
}

#ifdef DEBUG
/* static */
nsStaticAtom* nsCSSAnonBoxes::GetAtomBase() {
  return const_cast<nsStaticAtom*>(
      nsGkAtoms::GetAtomByIndex(kAtomIndex_AnonBoxes));
}

/* static */
void nsCSSAnonBoxes::AssertAtoms() {
  nsStaticAtom* base = GetAtomBase();
  size_t index = 0;
#  define CSS_ANON_BOX(name_, value_)                                 \
    {                                                                 \
      RefPtr<nsAtom> atom = NS_Atomize(value_);                       \
      MOZ_ASSERT(atom == nsGkAtoms::AnonBox_##name_,                  \
                 "Static atom for " #name_ " has incorrect value");   \
      MOZ_ASSERT(atom == &base[index],                                \
                 "Static atom for " #name_ " not at expected index"); \
      ++index;                                                        \
    }
#  include "nsCSSAnonBoxList.h"
#  undef CSS_ANON_BOX
}
#endif
