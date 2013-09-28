/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToISO88591_h___
#define nsUnicodeToISO88591_h___

#include "nsID.h"

class nsISupports;

// {920307B0-C6E8-11d2-8AA8-00600811A836}
#define NS_UNICODETOISO88591_CID \
  { 0x920307b0, 0xc6e8, 0x11d2, {0x8a, 0xa8, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODETOISO88591_CONTRACTID "@mozilla.org/intl/unicode/encoder;1?charset=ISO-8859-1"

//#define NS_ERROR_UCONV_NOUNICODETOISO88591  
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)


/**
 * A character set converter from Unicode to ISO88591.
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
nsresult
nsUnicodeToISO88591Constructor(nsISupports *aOuter, REFNSIID aIID,
                               void **aResult);

#endif /* nsUnicodeToISO88591_h___ */
