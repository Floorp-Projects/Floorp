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

#ifndef nsUnicodeToUTF7_h___
#define nsUnicodeToUTF7_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsBasicUTF7Encoder [declaration]

/**
 * Basic class for a character set converter from Unicode to UTF-7.
 *
 * @created         03/Jun/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsBasicUTF7Encoder : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsBasicUTF7Encoder(char aLastChar, char aEscChar);

  NS_IMETHOD FillInfo(PRUint32 *aInfo);

protected:

  PRInt32                   mEncoding;      // current encoding
  PRUint32                  mEncBits;
  PRInt32                   mEncStep;
  char                      mLastChar;
  char                      mEscChar;

  nsresult ShiftEncoding(PRInt32 aEncoding, char * aDest, 
      PRInt32 * aDestLength);
  nsresult EncodeDirect(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  nsresult EncodeBase64(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  char ValueToChar(PRUint32 aValue);
  virtual PRBool DirectEncodable(PRUnichar aChar);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD FinishNoBuff(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
};

//----------------------------------------------------------------------
// Class nsUnicodeToUTF7 [declaration]

/**
 * A character set converter from Unicode to UTF-7.
 *
 * @created         03/Jun/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToUTF7 : public nsBasicUTF7Encoder
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToUTF7();

protected:

  virtual PRBool DirectEncodable(PRUnichar aChar);
};

#endif /* nsUnicodeToUTF7_h___ */
