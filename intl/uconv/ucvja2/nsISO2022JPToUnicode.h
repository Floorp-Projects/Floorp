/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsISO2022JPToUnicode_h___
#define nsISO2022JPToUnicode_h___

#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

#define BUFFSIZE 2      // the size of the buffer for partial conversions

//----------------------------------------------------------------------
// Class nsISO2022JPToUnicode [declaration]

/**
 * A character set converter from ISO-2022-JP to Unicode.
 *
 * The state machine is:
 * S0 + ESC -> S1
 * S0 + * -> S0; convert using the current mCharset
 * S1 + '(' -> S2
 * S1 + '$' -> S3
 * S1 + * -> ERR
 * S2 + 'B' -> S0; mCharset = kASCII
 * S2 + 'J' -> S0; mCharset = kJISX0201_1976
 * S2 + * -> ERR
 * S3 + '@' -> S0; mCharset = kJISX0208_1978
 * S3 + 'B' -> S0; mCharset = kJISX0208_1983
 * S3 + * -> ERR
 * ERR + * -> ERR
 *
 * @created         09/Feb/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsISO2022JPToUnicode : public nsIUnicodeDecoder
{
  NS_DECL_ISUPPORTS

public:

  /**
   * Class constructor.
   */
  nsISO2022JPToUnicode();

  /**
   * Class destructor.
   */
  ~nsISO2022JPToUnicode();

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);

  //--------------------------------------------------------------------
  // Interface nsIUnicodeDecoder [declaration]

  NS_IMETHOD Convert(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength,const char * aSrc, PRInt32 aSrcOffset, 
      PRInt32 * aSrcLength);
  NS_IMETHOD Finish(PRUnichar * aDest, PRInt32 aDestOffset, 
      PRInt32 * aDestLength);
  NS_IMETHOD Length(const char * aSrc, PRInt32 aSrcOffset, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD SetInputErrorBehavior(PRInt32 aBehavior);

private:

  enum {
    kASCII, 
    kJISX0201_1976,
    kJISX0208_1978,
    kJISX0208_1983
  };

  PRInt32   mState;             // current state of the state machine
  PRInt32   mCharset;           // current character set

  char      mBuff[BUFFSIZE];    // buffer for the partial conversions
  PRInt32   mBuffLen;

  nsIUnicodeDecodeUtil  * mHelper;  // decoder helper object

  nsresult ConvertBuffer(const char ** aSrc, const char * aSrcEnd, 
      PRUnichar ** aDest, PRUnichar * aDestEnd);
};

#endif /* nsISO2022JPToUnicode_h___ */
