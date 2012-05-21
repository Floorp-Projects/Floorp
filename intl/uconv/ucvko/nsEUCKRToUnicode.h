/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEUCKRToUnicode_h___
#define nsEUCKRToUnicode_h___

#include "nsCP949ToUnicode.h"

/**
 * A character set converter from EUCKR to Unicode.
 *
 * @created         06/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
// Just make it an alias to CP949 decoder. bug 131388
nsresult
nsEUCKRToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                            void **aResult);

#endif /* nsEUCKRToUnicode_h___ */
