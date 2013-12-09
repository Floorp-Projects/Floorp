/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This class wraps up the creation (and destruction) of the standard
 * set of atoms used by gklayout; the atoms are created when gklayout
 * is loaded and they are destroyed when gklayout is unloaded.
 */

#include "nsGkAtoms.h"
#include "nsStaticAtom.h"

using namespace mozilla;

// define storage for all atoms
#define GK_ATOM(name_, value_) nsIAtom* nsGkAtoms::name_;
#include "nsGkAtomList.h"
#undef GK_ATOM

#define GK_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsGkAtomList.h"
#undef GK_ATOM

static const nsStaticAtom GkAtoms_info[] = {
#define GK_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &nsGkAtoms::name_),
#include "nsGkAtomList.h"
#undef GK_ATOM
};

void nsGkAtoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(GkAtoms_info);
}

