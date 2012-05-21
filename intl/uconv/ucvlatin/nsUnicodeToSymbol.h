/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToSymbol_h___
#define nsUnicodeToSymbol_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to Symbol.
 *
 */
nsresult
nsUnicodeToSymbolConstructor(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult);

#endif /* nsUnicodeToSymbol_h___ */
