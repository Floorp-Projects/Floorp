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

#ifndef nsUCS2BEToUnicode_h___
#define nsUCS2BEToUnicode_h___

#include "nsISupports.h"
#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUCS2BEToUnicode [declaration]

/**
 * A character set converter from UCS2BE to Unicode.
 *
 * @created         18/Mar/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsUCS2BEToUnicode : public nsTableDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUCS2BEToUnicode();

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
};
nsresult NEW_UTF16BEToUnicode(nsISupports **Result);
nsresult NEW_UTF16LEToUnicode(nsISupports **Result);

//============== above code is obsolete ==============================

class nsUTF16BEToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUTF16BEToUnicode() { Reset();};

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength); 
  NS_IMETHOD Reset();

protected:
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
protected:
  PRUint8 mState;
  PRUint8 mData;
};

class nsUTF16LEToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUTF16LEToUnicode() { Reset();};

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength); 
  NS_IMETHOD Reset();

protected:
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
protected:
  PRUint8 mState;
  PRUint8 mData;
};

#endif /* nsUCS2BEToUnicode_h___ */
