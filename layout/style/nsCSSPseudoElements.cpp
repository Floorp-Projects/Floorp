/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS pseudo-elements */

#include "mozilla/Util.h"

#include "nsCSSPseudoElements.h"
#include "nsAtomListUtils.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"
#include "nsCSSAnonBoxes.h"

using namespace mozilla;

// define storage for all atoms
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  nsICSSPseudoElement* nsCSSPseudoElements::name_;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

static const nsStaticAtom CSSPseudoElements_info[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  NS_STATIC_ATOM(name_##_buffer, (nsIAtom**)&nsCSSPseudoElements::name_),
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

// Separate from the array above so that we can have an array of
// nsStaticAtom (to pass to NS_RegisterStaticAtoms and
// nsAtomListUtils::IsMember), but with corresponding indices (so the
// i-th element of this array is the flags for the i-th pseudo-element
// in the previous array).
static const PRUint32 CSSPseudoElements_flags[] = {
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  flags_,
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT
};

void nsCSSPseudoElements::AddRefAtoms()
{
  NS_RegisterStaticAtoms(CSSPseudoElements_info);
}

bool nsCSSPseudoElements::IsPseudoElement(nsIAtom *aAtom)
{
  return nsAtomListUtils::IsMember(aAtom, CSSPseudoElements_info,
                                   ArrayLength(CSSPseudoElements_info));
}

/* static */ bool
nsCSSPseudoElements::IsCSS2PseudoElement(nsIAtom *aAtom)
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
               result ==
                 PseudoElementHasFlags(aAtom, CSS_PSEUDO_ELEMENT_IS_CSS2),
               "result doesn't match flags");
  return result;
}

/* static */ nsCSSPseudoElements::Type
nsCSSPseudoElements::GetPseudoType(nsIAtom *aAtom)
{
  for (PRUint32 i = 0; i < ArrayLength(CSSPseudoElements_info); ++i) {
    if (*CSSPseudoElements_info[i].mAtom == aAtom) {
      return Type(i);
    }
  }

  if (nsCSSAnonBoxes::IsAnonBox(aAtom)) {
#ifdef MOZ_XUL
    if (nsCSSAnonBoxes::IsTreePseudoElement(aAtom)) {
      return ePseudo_XULTree;
    }
#endif

    return ePseudo_AnonBox;
  }

  return ePseudo_NotPseudoElement;
}

/* static */ nsIAtom*
nsCSSPseudoElements::GetPseudoAtom(Type aType)
{
  NS_ASSERTION(aType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
               "Unexpected type");
  return *CSSPseudoElements_info[aType].mAtom;
}

/* static */ PRUint32
nsCSSPseudoElements::FlagsForPseudoElement(nsIAtom *aAtom)
{
  PRUint32 i;
  for (i = 0; i < ArrayLength(CSSPseudoElements_info); ++i) {
    if (*CSSPseudoElements_info[i].mAtom == aAtom) {
      break;
    }
  }
  NS_ASSERTION(i < ArrayLength(CSSPseudoElements_info),
               "argument must be a pseudo-element");
  return CSSPseudoElements_flags[i];
}
