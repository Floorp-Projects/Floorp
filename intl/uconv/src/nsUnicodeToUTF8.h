/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToUTF8_h___
#define nsUnicodeToUTF8_h___

#include "mozilla/Attributes.h"

// Class ID for our UnicodeToUTF8 charset converter
// {7C657D18-EC5E-11d2-8AAC-00600811A836}
#define NS_UNICODETOUTF8_CID \
  { 0x7c657d18, 0xec5e, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODETOUTF8_CONTRACTID "@mozilla.org/intl/unicode/encoder;1?charset=UTF-8"

//#define NS_ERROR_UCONV_NOUNICODETOUTF8  
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

//----------------------------------------------------------------------
// Class nsUnicodeToUTF8 [declaration]

/**
 * A character set converter from Unicode to UTF8.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToUTF8 MOZ_FINAL : public nsIUnicodeEncoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsUnicodeToUTF8() {mHighSurrogate = 0;}

  NS_IMETHOD Convert(const PRUnichar * aSrc, 
                     PRInt32 * aSrcLength, 
                     char * aDest, 
                     PRInt32 * aDestLength);

  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);

  NS_IMETHOD Reset() {mHighSurrogate = 0; return NS_OK;}

  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
    nsIUnicharEncoder * aEncoder, PRUnichar aChar) {return NS_OK;}

protected:
  PRUnichar mHighSurrogate;

};

#endif /* nsUnicodeToUTF8_h___ */
