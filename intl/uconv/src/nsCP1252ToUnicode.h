/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCP1252ToUnicode_h___
#define nsCP1252ToUnicode_h___

#include "nsISupports.h"

// Class ID for our CP1252ToUnicode charset converter
// {7C657D15-EC5E-11d2-8AAC-00600811A836}
#define NS_CP1252TOUNICODE_CID \
  { 0x7c657d15, 0xec5e, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_CP1252TOUNICODE_CONTRACTID "@mozilla.org/intl/unicode/decoder;1?charset=windows-1252"

/**
 * A character set converter from CP1252 to Unicode.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsCP1252ToUnicodeConstructor(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult);

#endif /* nsCP1252ToUnicode_h___ */
