/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* atom list for CSS anonymous boxes */

#ifndef nsCSSAnonBoxes_h___
#define nsCSSAnonBoxes_h___

#include "nsIAtom.h"

// Empty class derived from nsIAtom so that function signatures can
// require an atom from this atom list.
class nsICSSAnonBoxPseudo : public nsIAtom {};

class nsCSSAnonBoxes {
public:

  static void AddRefAtoms();

  static bool IsAnonBox(nsIAtom *aAtom);
#ifdef MOZ_XUL
  static bool IsTreePseudoElement(nsIAtom* aPseudo);
#endif
  static bool IsNonElement(nsIAtom* aPseudo)
    { return aPseudo == mozText || aPseudo == mozOtherNonElement; }

#define CSS_ANON_BOX(_name, _value) static nsICSSAnonBoxPseudo* _name;
#include "nsCSSAnonBoxList.h"
#undef CSS_ANON_BOX
};

#endif /* nsCSSAnonBoxes_h___ */
