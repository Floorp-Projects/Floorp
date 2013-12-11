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
#define CSS_ANON_BOX(_name, _value) \
  nsICSSAnonBoxPseudo* nsCSSAnonBoxes::_name;
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

#define CSS_ANON_BOX(name_, value_) \
  NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX

static const nsStaticAtom CSSAnonBoxes_info[] = {
#define CSS_ANON_BOX(name_, value_) \
  NS_STATIC_ATOM(name_##_buffer, (nsIAtom**)&nsCSSAnonBoxes::name_),
#include "nsCSSAnonBoxList.h"
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
  return StringBeginsWith(nsDependentAtomString(aPseudo),
                          NS_LITERAL_STRING(":-moz-tree-"));
}
#endif
