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

 /**
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#ifndef nsUnicodeToGBK_h___
#define nsUnicodeToGBK_h___

#include "nsUCvCnSupport.h"
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
  nsUnicodeToGBK();
  virtual ~nsUnicodeToGBK() {};

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, 
                            char * aDest, 
                            PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
                                char * aDest, PRInt32 * aDestLength)
  {
    return NS_OK;
  };  // just make it not abstract;

  NS_IMETHOD FillInfo(PRUint32 *aInfo);

  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();

  nsCOMPtr<nsIUnicodeEncoder> mExtensionEncoder;
  nsCOMPtr<nsIUnicodeEncoder> m4BytesEncoder;
protected:
  nsGBKConvUtil mUtil;
  PRBool TryExtensionEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
  PRBool Try4BytesEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
};

class nsUnicodeToGB18030: public nsUnicodeToGBK
{
public:
  nsUnicodeToGB18030() {};
  virtual ~nsUnicodeToGB18030() {};
protected:
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();
};

class nsUnicodeToGB18030Font0: public nsUnicodeToGB18030
{
public:
  nsUnicodeToGB18030Font0() {};
  virtual ~nsUnicodeToGB18030Font0() {};
protected:
  virtual void Create4BytesEncoder();
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD FillInfo(PRUint32 *aInfo);
};
class nsUnicodeToGB18030Font1 : public nsTableEncoderSupport
{
public: 
  nsUnicodeToGB18030Font1();
  virtual ~nsUnicodeToGB18030Font1() {};
protected: 
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, 
                                PRInt32 aSrcLength,
                                PRInt32 * aDestLength);
  NS_IMETHOD FillInfo(PRUint32 *aInfo);
};

#endif /* nsUnicodeToGBK_h___ */

