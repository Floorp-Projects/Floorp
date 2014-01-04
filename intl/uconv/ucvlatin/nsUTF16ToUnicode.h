/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUTF16ToUnicode_h_
#define nsUTF16ToUnicode_h_

#include "nsISupports.h"
#include "nsUCSupport.h"

// internal base class
class nsUTF16ToUnicodeBase : public nsBasicDecoderSupport
{
protected:
  // ctor accessible only by child classes
  nsUTF16ToUnicodeBase() { Reset();}

  nsresult UTF16ConvertToUnicode(const char * aSrc,
                                 int32_t * aSrcLength, char16_t * aDest,
                                 int32_t * aDestLength, bool aSwapBytes);

public: 
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength);
  NS_IMETHOD Reset();

protected:
  uint8_t mState;
  // to store an odd byte left over between runs
  uint8_t mOddByte;
  // to store an odd high surrogate left over between runs
  char16_t mOddHighSurrogate;
  // to store an odd low surrogate left over between runs
  char16_t mOddLowSurrogate;
};

// UTF-16 big endian
class nsUTF16BEToUnicode : public nsUTF16ToUnicodeBase
{
public:

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength,
      char16_t * aDest, int32_t * aDestLength); 
};

// UTF-16 little endian
class nsUTF16LEToUnicode : public nsUTF16ToUnicodeBase
{
public:

  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength,
      char16_t * aDest, int32_t * aDestLength); 
};

// UTF-16 with BOM
class nsUTF16ToUnicode : public nsUTF16ToUnicodeBase
{
public:

  nsUTF16ToUnicode() { Reset();}
  NS_IMETHOD Convert(const char * aSrc, int32_t * aSrcLength,
      char16_t * aDest, int32_t * aDestLength); 

  NS_IMETHOD Reset();

private:

  enum Endian {kUnknown, kBigEndian, kLittleEndian};
  Endian  mEndian; 
  bool    mFoundBOM;
};

#endif /* nsUTF16ToUnicode_h_ */
