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

#ifndef nsUEscapeToUnicode_h___
#define nsUEscapeToUnicode_h___
#include "nsISupports.h"

#include "nsUEscapeToUnicode.h"
#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUEscapeToUnicode [declaration]

class nsUEscapeToUnicode : public nsBasicDecoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUEscapeToUnicode() { Reset();};

  NS_IMETHOD Convert(const char * aSrc, PRInt32 * aSrcLength,
      PRUnichar * aDest, PRInt32 * aDestLength); 
  NS_IMETHOD Reset();
 
protected:
  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
private:
  PRUint8 mState;
  PRUint16 mData;
  PRUnichar mBuffer[2];
  PRUint32  mBufferLen;
};

nsresult NEW_UEscapeToUnicode(nsISupports **Result);


#endif /* nsUEscapeToUnicode_h___ */
