/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#include "mozilla/ArrayUtils.h"

#include "nsCSSAnonBoxes.h"
#include "nsAtomListUtils.h"
#include "nsStaticAtom.h"

using namespace mozilla;

// define storage for all atoms
#define CSS_ANON_BOX(name_, value_, skips_fixup_) \
  nsICSSAnonBoxPseudo* nsCSSAnonBoxes::name_;
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

#define CSS_ANON_BOX(name_, value_, skips_fixup_) \
  NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

static const nsStaticAtom CSSAnonBoxes_info[] = {
  // Put the non-inheriting anon boxes first, so we can index into them easily.
#define CSS_ANON_BOX(name_, value_, skips_fixup_) /* nothing */
#define CSS_NON_INHERITING_ANON_BOX(name_, value_) \
  NS_STATIC_ATOM(name_##_buffer, (nsIAtom**)&nsCSSAnonBoxes::name_),
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX

#define CSS_ANON_BOX(name_, value_, skips_fixup_) \
  NS_STATIC_ATOM(name_##_buffer, (nsIAtom**)&nsCSSAnonBoxes::name_),
#define CSS_NON_INHERITING_ANON_BOX(name_, value_) /* nothing */
#include "nsCSSAnonBoxList.h"
#undef CSS_NON_INHERITING_ANON_BOX
#undef CSS_ANON_BOX
};

void nsCSSAnonBoxes::AddRefAtoms()
{
  NS_RegisterStaticAtoms(CSSAnonBoxes_info);
}

bool nsCSSAnonBoxes::IsAnonBox(nsIAtom *aAtom)
{
  return nsAtomListUtils::IsMember(aAtom, CSSAnonBoxes_info,
                                   ArrayLength(CSSAnonBoxes_info));
}

#ifdef MOZ_XUL
/* static */ bool
nsCSSAnonBoxes::IsTreePseudoElement(nsIAtom* aPseudo)
{
  MOZ_ASSERT(nsCSSAnonBoxes::IsAnonBox(aPseudo));
  return StringBeginsWith(nsDependentAtomString(aPseudo),
                          NS_LITERAL_STRING(":-moz-tree-"));
}
#endif

/* static*/ nsCSSAnonBoxes::NonInheriting
nsCSSAnonBoxes::NonInheritingTypeForPseudoTag(nsIAtom* aPseudo)
{
  MOZ_ASSERT(IsNonInheritingAnonBox(aPseudo));
  for (NonInheritingBase i = 0;
       i < ArrayLength(CSSAnonBoxes_info);
       ++i) {
    if (*CSSAnonBoxes_info[i].mAtom == aPseudo) {
      return static_cast<NonInheriting>(i);
    }
  }

  MOZ_CRASH("Bogus pseudo passed to NonInheritingTypeForPseudoTag");
}

/* static */ nsIAtom*
nsCSSAnonBoxes::GetNonInheritingPseudoAtom(NonInheriting aBoxType)
{
  MOZ_ASSERT(aBoxType < NonInheriting::_Count);
  return *CSSAnonBoxes_info[static_cast<NonInheritingBase>(aBoxType)].mAtom;
}
