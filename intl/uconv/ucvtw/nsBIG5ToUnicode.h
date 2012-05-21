/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBIG5ToUnicode_h___
#define nsBIG5ToUnicode_h___

#include "nsISupports.h"

/**
 * A character set converter from BIG5 to Unicode.
 *
 * @created         06/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsBIG5ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                                void **aResult);

#endif /* nsBIG5ToUnicode_h___ */
