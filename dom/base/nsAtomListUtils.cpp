/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Static helper class for implementing atom lists.
 */

#include "nsAtomListUtils.h"
#include "nsIAtom.h"
#include "nsStaticAtom.h"

/* static */ bool
nsAtomListUtils::IsMember(nsIAtom *aAtom,
                          const nsStaticAtom* aInfo,
                          uint32_t aInfoCount)
{
    for (const nsStaticAtom *info = aInfo, *info_end = aInfo + aInfoCount;
         info != info_end; ++info) {
        if (aAtom == *(info->mAtom))
            return true;
    }
    return false;
}
