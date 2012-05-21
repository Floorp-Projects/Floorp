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

#ifndef nsUnicodeToGBK_h___
#define nsUnicodeToGBK_h___

#include "nsUCSupport.h"
#include "nsCOMPtr.h"
#include "nsIUnicodeEncoder.h"
#include "gbku.h"
//----------------------------------------------------------------------
// Class nsUnicodeToGBK [declaration]

class nsUnicodeToGBK: public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToGBK(PRUint32 aMaxLengthFactor = 2);
  virtual ~nsUnicodeToGBK() {}

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, 
                            char * aDest, 
                            PRInt32 * aDestLength);

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
                                char * aDest, PRInt32 * aDestLength)
  {
    return NS_OK;
  }  // just make it not abstract;

  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();

  nsCOMPtr<nsIUnicodeEncoder> mExtensionEncoder;
  nsCOMPtr<nsIUnicodeEncoder> m4BytesEncoder;
protected:
  PRUnichar mSurrogateHigh;
  nsGBKConvUtil mUtil;
  bool TryExtensionEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
  bool Try4BytesEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
  virtual bool EncodeSurrogate(PRUnichar aSurrogateHigh, PRUnichar aSurrogateLow, char* aDest);
};

class nsUnicodeToGB18030: public nsUnicodeToGBK
{
public:
  nsUnicodeToGB18030() : nsUnicodeToGBK(4) {}
  virtual ~nsUnicodeToGB18030() {}
protected:
  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();
  virtual bool EncodeSurrogate(PRUnichar aSurrogateHigh, PRUnichar aSurrogateLow, char* aDest);
};

#endif /* nsUnicodeToGBK_h___ */

