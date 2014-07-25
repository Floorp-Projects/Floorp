/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToCP1252_h___
#define nsUnicodeToCP1252_h___

#include "nsID.h"

class nsISupports;

// Class ID for our UnicodeToCP1252 charset converter
// {7B8556AC-EC79-11d2-8AAC-00600811A836}
#define NS_UNICODETOCP1252_CID \
  { 0x7b8556ac, 0xec79, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODETOCP1252_CONTRACTID "@mozilla.org/intl/unicode/encoder;1?charset=windows-1252"

//#define NS_ERROR_UCONV_NOUNICODETOCP1252  
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

/**
 * A character set converter from Unicode to CP1252.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUnicodeToCP1252Constructor(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult);

#endif /* nsUnicodeToCP1252_h___ */
