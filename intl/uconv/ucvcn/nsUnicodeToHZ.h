/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#ifndef nsUnicodeToHZ_h___
#define nsUnicodeToHZ_h___

#include "nsUCSupport.h"
#include "gbku.h"
//----------------------------------------------------------------------
// Class nsUnicodeToHZ [declaration]

class nsUnicodeToHZ: public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToHZ();

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, 
                            char * aDest, 
                            PRInt32 * aDestLength);

  NS_IMETHOD FinishNoBuff(char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
                                char * aDest, PRInt32 * aDestLength)
  {
    return NS_OK;
  }  // just make it not abstract;

  PRUint16 mHZState;
protected:
  nsGBKConvUtil mUtil;


};

#endif /* nsUnicodeToHZ_h___ */
