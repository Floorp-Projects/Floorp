/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToKOI8U_h___
#define nsUnicodeToKOI8U_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to KOI8U.
 *
 * @created         4/26/1999
 * @author  Frank Tang [ftang]
 */
nsresult
nsUnicodeToKOI8UConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);

#endif /* nsUnicodeToKOI8U_h___ */
