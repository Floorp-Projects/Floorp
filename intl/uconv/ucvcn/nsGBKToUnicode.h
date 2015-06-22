/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsGBKToUnicode_h___
#define nsGBKToUnicode_h___

#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#include "nsUCSupport.h"
#include "nsGBKConvUtil.h"

//----------------------------------------------------------------------
// Class nsGB18030ToUnicode [declaration]

/**
 * A character set converter from GB18030 to Unicode.
 * 
 *
 * @created         07/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */
class nsGB18030ToUnicode : public nsBufferDecoderSupport
{
public:
		  
  /**
   * Class constructor.
   */
  nsGB18030ToUnicode() : nsBufferDecoderSupport(1)
  {
  }

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsBufferDecoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const char* aSrc, int32_t * aSrcLength, char16_t *aDest, int32_t * aDestLength);

protected:
  nsGBKConvUtil mUtil;

  bool TryExtensionDecoder(const char* aSrc, char16_t* aDest);
  bool Try4BytesDecoder(const char* aSrc, char16_t* aDest);
  bool DecodeToSurrogate(const char* aSrc, char16_t* aDest);

};

#endif /* nsGBKToUnicode_h___ */

