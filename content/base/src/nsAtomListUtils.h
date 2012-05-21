/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Static helper class for implementing atom lists.
 */

#ifndef nsAtomListUtils_h__
#define nsAtomListUtils_h__

#include "prtypes.h"

class nsIAtom;
struct nsStaticAtom;

class nsAtomListUtils {
public:
    static bool IsMember(nsIAtom *aAtom,
                           const nsStaticAtom* aInfo,
                           PRUint32 aInfoCount);
};

#endif /* !defined(nsAtomListUtils_h__) */
