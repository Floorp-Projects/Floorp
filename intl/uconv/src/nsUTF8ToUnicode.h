/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsUTF8ToUnicode_h___
#define nsUTF8ToUnicode_h___

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

   PRUint32 mState;	// cached expected number of bytes per UTF8 character sequence
   PRUint32 mUcs4;	// cached Unicode character
   PRUint32 mBytes;

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsBasicDecoderSupport class [declaration]

  NS_IMETHOD Reset();

};

#endif /* nsUTF8ToUnicode_h___ */

