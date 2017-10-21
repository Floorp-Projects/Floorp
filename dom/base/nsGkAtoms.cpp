/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsStaticAtom.h"

using namespace mozilla;

#define GK_ATOM(name_, value_) NS_STATIC_ATOM_DEFN(nsGkAtoms, name_)
#include "nsGkAtomList.h"
#undef GK_ATOM

#define GK_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_, value_)
#include "nsGkAtomList.h"
#undef GK_ATOM

static const nsStaticAtomSetup sGkAtomSetup[] = {
  #define GK_ATOM(name_, value_) NS_STATIC_ATOM_SETUP(nsGkAtoms, name_)
  #include "nsGkAtomList.h"
  #undef GK_ATOM
};

void nsGkAtoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(sGkAtomSetup);
}

