/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToGB2312V2_h___
#define nsUnicodeToGB2312V2_h___

#include "nsUCSupport.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsUnicodeToGB2312V2 [declaration]

/**
 * A character set converter from Unicode to GB2312.
 *
 * @created         06/Apr/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToGB2312V2 : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToGB2312V2();

protected:

  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, 
                            int32_t * aSrcLength, 
                            char * aDest, 
                            int32_t * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, int32_t * aSrcLength, 
                                char * aDest, int32_t * aDestLength)
  {
    return NS_OK;
  }   // just make it not abstract;

protected:
  nsGBKConvUtil mUtil;
};

#endif /* nsUnicodeToGB2312V2_h___ */
