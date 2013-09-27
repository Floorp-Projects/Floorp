/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToEUCJP_h___
#define nsUnicodeToEUCJP_h___

#include "nsID.h"

class nsISupports;

/**
 * A character set converter from Unicode to EUCJP.
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUnicodeToEUCJPConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);

#endif /* nsUnicodeToEUCJP_h___ */
