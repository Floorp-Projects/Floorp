/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUTF8ToUnicode_h___
#define nsUTF8ToUnicode_h___

#include "nsUCSupport.h"

// Class ID for our UTF8ToUnicode charset converter
// {5534DDC0-DD96-11d2-8AAC-00600811A836}
#define NS_UTF8TOUNICODE_CID \
  { 0x5534ddc0, 0xdd96, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UTF8TOUNICODE_CONTRACTID "@mozilla.org/intl/unicode/decoder;1?charset=UTF-8"

//#define NS_ERROR_UCONV_NOUTF8TOUNICODE  
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

//----------------------------------------------------------------------
// Class nsUTF8ToUnicode [declaration]


/**
 * A character set converter from UTF8 to Unicode.
 *
 * @created         18/Mar/1998
 * @modified        04/Feb/2000
 * @author  Catalin Rotaru [CATA]
 */

class nsUTF8ToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUTF8ToUnicode();

protected:

  uint32_t mUcs4; // cached Unicode character
  uint8_t mState; // cached expected number of bytes per UTF8 character sequence
  uint8_t mBytes;
  bool mFirst;

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength, 
      char16_t * aDest, int32_t * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Reset();

};

#endif /* nsUTF8ToUnicode_h___ */

