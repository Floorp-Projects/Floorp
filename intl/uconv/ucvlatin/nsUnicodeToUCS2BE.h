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

#ifndef nsUnicodeToUCS2BE_h___
#define nsUnicodeToUCS2BE_h___

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToUCS2BE [declaration]

/**
 * A character set converter from Unicode to UCS2BE.
 *
 * @created         05/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToUCS2BE : public nsTableEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToUCS2BE();
  
  NS_IMETHOD FillInfo(PRUint32* aInfo);

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};

nsresult NEW_UnicodeToUTF16BE(nsISupports **Result);
nsresult NEW_UnicodeToUTF16LE(nsISupports **Result);
nsresult NEW_UnicodeToUTF16(nsISupports **Result);

//============== above code is obsolete ==============================

class nsUnicodeToUTF16BE: public nsBasicEncoder
{
public:
  nsUnicodeToUTF16BE() { mBOM = 0;};

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

  //--------------------------------------------------------------------
  // Interface nsICharRepresentable [declaration]
  NS_IMETHOD FillInfo(PRUint32 *aInfo);

protected:
  PRUnichar mBOM;
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
};

class nsUnicodeToUTF16LE: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16LE() { mBOM = 0;};

protected:
  NS_IMETHOD CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  );
};

class nsUnicodeToUTF16: public nsUnicodeToUTF16BE
{
public:
  nsUnicodeToUTF16() { mBOM = 0xFEFF;};
};

#endif /* nsUnicodeToUCS2BE_h___ */
