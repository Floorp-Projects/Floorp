/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Static helper class for implementing atom lists.
 */

#include "nsAtomListUtils.h"
#include "nsAtom.h"
#include "nsStaticAtom.h"

/* static */ bool
nsAtomListUtils::IsMember(nsAtom *aAtom,
                          const nsStaticAtomSetup* aSetup,
                          uint32_t aCount)
{
    for (const nsStaticAtomSetup *setup = aSetup, *setup_end = aSetup + aCount;
         setup != setup_end; ++setup) {
        if (aAtom == *(setup->mAtom))
            return true;
    }
    return false;
}
