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

#ifndef nsUnicodeToCP1252_h___
#define nsUnicodeToCP1252_h___

// Class ID for our UnicodeToCP1252 charset converter
// {7B8556AC-EC79-11d2-8AAC-00600811A836}
#define NS_UNICODETOCP1252_CID \
  { 0x7b8556ac, 0xec79, 0x11d2, {0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36}}

#define NS_UNICODETOCP1252_CONTRACTID "@mozilla.org/intl/unicode/encoder;1?charset=windows-1252"

//#define NS_ERROR_UCONV_NOUNICODETOCP1252  
//  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x31)

//----------------------------------------------------------------------
// Class nsUnicodeToCP1252 [declaration]

/**
 * A character set converter from Unicode to CP1252.
 *
 * @created         20/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToCP1252 : public nsTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToCP1252();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

#endif /* nsUnicodeToCP1252_h___ */
