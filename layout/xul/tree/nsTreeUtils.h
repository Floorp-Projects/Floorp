/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeUtils_h__
#define nsTreeUtils_h__

#include "nsString.h"
#include "nsTreeStyleCache.h"
#include "nsIContent.h"

class nsTreeUtils
{
  public:
    /**
     * Parse a whitespace separated list of properties into an array
     * of atoms.
     */
    static nsresult
    TokenizeProperties(const nsAString& aProperties, AtomArray & aPropertiesArray);

    static nsIContent*
    GetImmediateChild(nsIContent* aContainer, nsIAtom* aTag);

    static nsIContent*
    GetDescendantChild(nsIContent* aContainer, nsIAtom* aTag);

    static nsresult
    UpdateSortIndicators(nsIContent* aColumn, const nsAString& aDirection);

    static nsresult
    GetColumnIndex(nsIContent* aColumn, int32_t* aResult);
};

#endif // nsTreeUtils_h__
