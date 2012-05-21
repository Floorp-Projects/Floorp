/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsT61ToUnicode_h___
#define nsT61ToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from T61 to Unicode.
 *
 * @created         18/Mar/1998
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsT61ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                          void **aResult);

#endif /* nsT61ToUnicode_h___ */
