/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsGBK2312ToUnicode_h___
#define nsGBK2312ToUnicode_h___

#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#include "nsUCSupport.h"
#include "gbku.h"

//----------------------------------------------------------------------
// Class nsGBKToUnicode [declaration]

/**
 * A character set converter from GBK to Unicode.
 * 
 *
 * @created         07/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */
class nsGBKToUnicode : public nsBufferDecoderSupport
{
public:
		  
  /**
   * Class constructor.
   */
  nsGBKToUnicode() : nsBufferDecoderSupport(1)
  {
    mExtensionDecoder = nullptr;
    m4BytesDecoder = nullptr;
  }

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const char* aSrc, PRInt32 * aSrcLength, PRUnichar *aDest, PRInt32 * aDestLength);

protected:
  nsGBKConvUtil mUtil;
  nsCOMPtr<nsIUnicodeDecoder> mExtensionDecoder;
  nsCOMPtr<nsIUnicodeDecoder> m4BytesDecoder;

  virtual void CreateExtensionDecoder();
  virtual void Create4BytesDecoder();
  bool TryExtensionDecoder(const char* aSrc, PRUnichar* aDest);
  bool Try4BytesDecoder(const char* aSrc, PRUnichar* aDest);
  virtual bool DecodeToSurrogate(const char* aSrc, PRUnichar* aDest);

};


class nsGB18030ToUnicode : public nsGBKToUnicode
{
public:
  nsGB18030ToUnicode() {}
  virtual ~nsGB18030ToUnicode() {}
protected:
  virtual void CreateExtensionDecoder();
  virtual void Create4BytesDecoder();
  virtual bool DecodeToSurrogate(const char* aSrc, PRUnichar* aDest);
};

#endif /* nsGBK2312ToUnicode_h___ */

