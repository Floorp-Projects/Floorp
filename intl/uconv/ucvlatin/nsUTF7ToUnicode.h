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

#ifndef nsUTF7ToUnicode_h___
#define nsUTF7ToUnicode_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsBasicUTF7Decoder [declaration]

/**
 * Basic class for a character set converter from UTF-7 to Unicode.
 *
 * @created         03/Jun/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsBasicUTF7Decoder : public nsBufferDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsBasicUTF7Decoder(char aLastChar, char aEscChar);

protected:

  PRInt32                   mEncoding;      // current encoding
  PRUint32                  mEncBits;
  PRInt32                   mEncStep;
  char                      mLastChar;
  char                      mEscChar;
  PRBool                    mFreshBase64;

  nsresult DecodeDirect(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
  nsresult DecodeBase64(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
  PRUint32 CharToValue(char aChar);

  //--------------------------------------------------------------------
  // Subclassing of nsBufferDecoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuff(const char * aSrc, PRInt32 * aSrcLength, 
      PRUnichar * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
};

//----------------------------------------------------------------------
// Class nsUTF7ToUnicode [declaration]

/**
 * A character set converter from Modified UTF7 to Unicode.
 *
 * @created         18/May/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUTF7ToUnicode : public nsBasicUTF7Decoder 
{
public:

  /**
   * Class constructor.
   */
  nsUTF7ToUnicode();

};

#endif /* nsUTF7ToUnicode_h___ */
