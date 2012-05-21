/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEUCTWToUnicode_h___
#define nsEUCTWToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from EUCTW to Unicode.
 *
 * @created         06/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsEUCTWToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);

#endif /* nsEUCTWToUnicode_h___ */
