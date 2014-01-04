/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHZToUnicode_h___
#define nsHZToUnicode_h___

#include "nsUCSupport.h"
#include "nsGBKConvUtil.h"

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
  NS_IMETHOD ConvertNoBuff(const char* aSrc, int32_t * aSrcLength, 
                           char16_t *aDest, int32_t * aDestLength); 
  nsGBKConvUtil mUtil;

private:
  int16_t mHZState;
  uint32_t mRunLength; // length of a run of 8-bit GB-encoded characters
  char mOddByte; // first byte of a multi-byte sequence from a previous buffer

};

#endif /* nsHZToUnicode_h___ */
