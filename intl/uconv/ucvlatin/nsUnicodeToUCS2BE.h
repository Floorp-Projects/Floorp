/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToUCS2BE_h___
#define nsUnicodeToUCS2BE_h___

#include "nsUCSupport.h"

class nsUnicodeToUTF16BE: public nsBasicEncoder
{
public:
  nsUnicodeToUTF16BE() { mBOM = 0;}

  //--------------------------------------------------------------------
  // Interface nsIUnicodeEncoder [declaration]

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);

protected:
  PRUnichar mBOM;
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
};

class nsUnicodeToUTF16LE: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16LE() { mBOM = 0;}

protected:
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
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

#endif /* nsUnicodeToUCS2BE_h___ */
