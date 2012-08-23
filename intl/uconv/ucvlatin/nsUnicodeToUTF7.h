/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToUTF7_h___
#define nsUnicodeToUTF7_h___

#include "nsUCSupport.h"

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

protected:

  int32_t                   mEncoding;      // current encoding
  uint32_t                  mEncBits;
  int32_t                   mEncStep;
  char                      mLastChar;
  char                      mEscChar;

  nsresult ShiftEncoding(int32_t aEncoding, char * aDest, 
      int32_t * aDestLength);
  nsresult EncodeDirect(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  nsresult EncodeBase64(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  char ValueToChar(uint32_t aValue);
  virtual bool DirectEncodable(PRUnichar aChar);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  NS_IMETHOD FinishNoBuff(char * aDest, int32_t * aDestLength);
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

  virtual bool DirectEncodable(PRUnichar aChar);
};

#endif /* nsUnicodeToUTF7_h___ */
