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

#ifndef nsMacRomanToUnicode_h___
#define nsMacRomanToUnicode_h___

// Class ID for our MacRomanToUnicode charset converter
// {7B8556A1-EC79-11d2-8AAC-00600811A836}
#define NS_MACROMANTOUNICODE_CID \
  { 0x7b8556a1, 0xec79, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_MACROMANTOUNICODE_CONTRACTID "@mozilla.org/intl/unicode/decoder;1?charset=x-mac-roman"

//#define NS_ERROR_UCONV_NOMACROMANTOUNICODE   
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

//----------------------------------------------------------------------
// Class nsMacRomanToUnicode [declaration]

/**
 * A character set converter from MacRoman to Unicode.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsMacRomanToUnicode : public nsTableDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsMacRomanToUnicode();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

#endif /* nsMacRomanToUnicode_h___ */
