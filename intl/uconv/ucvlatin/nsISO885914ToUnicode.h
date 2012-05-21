/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISO885914ToUnicode_h___
#define nsISO885914ToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from ISO885914 to Unicode.
 *
 * @created         4/26/1999
 * @author  Frank Tang [ftang]
 */
nsresult
nsISO885914ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsISO885914ToUnicode_h___ */
