/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsHZToUnicode_h___
#define nsHZToUnicode_h___

#include "nsUCvCnSupport.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsHZToUnicode [declaration]

/**
 * A character set converter from GBK to Unicode.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */
class nsHZToUnicode : public nsBufferDecoderSupport
{
public:
		
  /**
   * Class constructor.
   */
  nsHZToUnicode();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const char* aSrc, PRInt32 * aSrcLength, 
                           PRUnichar *aDest, PRInt32 * aDestLength); 
  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
                           PRInt32 * aDestLength);
  nsGBKConvUtil mUtil;

private:
  PRInt16 mHZState;

};

#endif /* nsHZToUnicode_h___ */
