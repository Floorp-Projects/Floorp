/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToUTF16_h_
#define nsUnicodeToUTF16_h_

#include "nsUCSupport.h"

class nsUnicodeToUTF16BE: public nsBasicEncoder
{
public:
  nsUnicodeToUTF16BE() { mBOM = 0;}

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncoder [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength);
  NS_IMETHOD Finish(char * aDest, int32_t * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetOutputErrorBehavior(int32_t aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);

protected:
  PRUnichar mBOM;
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, int32_t aLen  );
};

class nsUnicodeToUTF16LE: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16LE() { mBOM = 0;}

protected:
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, int32_t aLen  );
};

// XXX In theory, we have to check the endianness at run-time because some
// modern RISC processors can be run at both LE and BE. 
#ifdef IS_LITTLE_ENDIAN
class nsUnicodeToUTF16: public nsUnicodeToUTF16LE
#elif defined(IS_BIG_ENDIAN)
class nsUnicodeToUTF16: public nsUnicodeToUTF16BE
#else
#error "Unknown endianness"
#endif
{
public:
  nsUnicodeToUTF16() { mBOM = 0xFEFF;}
};

#endif /* nsUnicodeToUTF16_h_ */
