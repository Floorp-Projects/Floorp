/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeUtils_h__
#define nsTreeUtils_h__

#include "mozilla/AtomArray.h"
#include "nsError.h"
#include "nsString.h"
#include "nsTreeStyleCache.h"

class nsAtom;
class nsIContent;

class nsTreeUtils
{
  public:
    /**
     * Parse a whitespace separated list of properties into an array
     * of atoms.
     */
    static nsresult
    TokenizeProperties(const nsAString& aProperties,
                       mozilla::AtomArray& aPropertiesArray);

    static nsIContent*
    GetImmediateChild(nsIContent* aContainer, nsAtom* aTag);

    static nsIContent*
    GetDescendantChild(nsIContent* aContainer, nsAtom* aTag);

    static nsresult
    UpdateSortIndicators(nsIContent* aColumn, const nsAString& aDirection);

    static nsresult
    GetColumnIndex(nsIContent* aColumn, int32_t* aResult);
};

#endif // nsTreeUtils_h__
