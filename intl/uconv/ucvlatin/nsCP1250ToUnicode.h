/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCP1250ToUnicode_h___
#define nsCP1250ToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from CP1250 to Unicode.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsCP1250ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);
#endif /* nsCP1250ToUnicode_h___ */
