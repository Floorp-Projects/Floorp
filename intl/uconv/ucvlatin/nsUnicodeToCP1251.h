/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToCP1251_h___
#define nsUnicodeToCP1251_h___

#include "nsISupports.h"

/**
 * A character set converter from Unicode to CP1251.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUnicodeToCP1251Constructor(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult);

#endif /* nsUnicodeToCP1251_h___ */
