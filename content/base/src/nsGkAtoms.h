/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This class wraps up the creation (and destruction) of the standard
 * set of atoms used by gklayout; the atoms are created when gklayout
 * is loaded and they are destroyed when gklayout is unloaded.
 */

#ifndef nsGkAtoms_h___
#define nsGkAtoms_h___

#include "nsIAtom.h"

class nsGkAtoms {
public:

  static void AddRefAtoms();

  /* Declare all atoms

     The atom names and values are stored in nsGkAtomList.h and
     are brought to you by the magic of C preprocessing

     Add new atoms to nsGkAtomList and all support logic will be auto-generated
   */
#define GK_ATOM(_name, _value) static nsIAtom* _name;
#include "nsGkAtomList.h"
#undef GK_ATOM
};

#endif /* nsGkAtoms_h___ */
