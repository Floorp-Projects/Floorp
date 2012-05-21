/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUserDefinedToUnicode_h___
#define nsUserDefinedToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from UserDefined to Unicode.
 *
 * @created         23/Nov/1998
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUserDefinedToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                  void **aResult);

#endif /* nsUserDefinedToUnicode_h___ */
