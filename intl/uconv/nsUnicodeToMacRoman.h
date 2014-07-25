/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToMacRoman_h___
#define nsUnicodeToMacRoman_h___

#include "nsID.h"

class nsISupports;

// Class ID for our UnicodeToMacRoman charset converter
// {7B8556AF-EC79-11d2-8AAC-00600811A836}
#define NS_UNICODETOMACROMAN_CID \
  { 0x7b8556af, 0xec79, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODETOMACROMAN_CONTRACTID "@mozilla.org/intl/unicode/encoder;1?charset=macintosh"

/**
 * A character set converter from Unicode to MacRoman.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUnicodeToMacRomanConstructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult);

#endif /* nsUnicodeToMacRoman_h___ */
