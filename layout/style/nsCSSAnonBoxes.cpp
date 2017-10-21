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

#define CSS_ANON_BOX(name_, value_) \
  NS_STATIC_ATOM_SUBCLASS_DEFN(nsICSSAnonBoxPseudo, nsCSSAnonBoxes, name_)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

#define CSS_ANON_BOX(name_, value_) NS_STATIC_ATOM_BUFFER(name_, value_)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

static const nsStaticAtomSetup sCSSAnonBoxAtomSetup[] = {
  // Put the non-inheriting anon boxes first, so we can index into them easily.
  #define CSS_ANON_BOX(name_, value_) /* nothing */
  #define CSS_NON_INHERITING_ANON_BOX(name_, value_) \
    NS_STATIC_ATOM_SUBCLASS_SETUP(nsCSSAnonBoxes, name_)
  #include "nsCSSAnonBoxList.h"
  #undef CSS_NON_INHERITING_ANON_BOX
  #undef CSS_ANON_BOX

  #define CSS_ANON_BOX(name_, value_) \
    NS_STATIC_ATOM_SUBCLASS_SETUP(nsCSSAnonBoxes, name_)
  #define CSS_NON_INHERITING_ANON_BOX(name_, value_) /* nothing */
  #include "nsCSSAnonBoxList.h"
  #undef CSS_NON_INHERITING_ANON_BOX
  #undef CSS_ANON_BOX
};

void nsCSSAnonBoxes::AddRefAtoms()
{
  NS_RegisterStaticAtoms(sCSSAnonBoxAtomSetup);
}

bool nsCSSAnonBoxes::IsAnonBox(nsAtom *aAtom)
{
  return nsAtomListUtils::IsMember(aAtom, sCSSAnonBoxAtomSetup,
                                   ArrayLength(sCSSAnonBoxAtomSetup));
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
  for (NonInheritingBase i = 0; i < ArrayLength(sCSSAnonBoxAtomSetup); ++i) {
    if (*sCSSAnonBoxAtomSetup[i].mAtom == aPseudo) {
      return static_cast<NonInheriting>(i);
    }
  }

  MOZ_CRASH("Bogus pseudo passed to NonInheritingTypeForPseudoTag");
}

/* static */ nsAtom*
nsCSSAnonBoxes::GetNonInheritingPseudoAtom(NonInheriting aBoxType)
{
  MOZ_ASSERT(aBoxType < NonInheriting::_Count);
  return *sCSSAnonBoxAtomSetup[static_cast<NonInheritingBase>(aBoxType)].mAtom;
}
