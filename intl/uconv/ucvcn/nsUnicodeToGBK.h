/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  NS_IMETHOD FillInfo(PRUint32 *aInfo);

  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();

  nsCOMPtr<nsIUnicodeEncoder> mExtensionEncoder;
  nsCOMPtr<nsIUnicodeEncoder> m4BytesEncoder;
protected:
  PRUnichar mSurrogateHigh;
  nsGBKConvUtil mUtil;
  PRBool TryExtensionEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
  PRBool Try4BytesEncoder(PRUnichar aChar, char* aDest, PRInt32* aOutLen);
  virtual PRBool EncodeSurrogate(PRUnichar aSurrogateHigh, PRUnichar aSurrogateLow, char* aDest);
};

class nsUnicodeToGB18030: public nsUnicodeToGBK
{
public:
  nsUnicodeToGB18030() : nsUnicodeToGBK(4) {}
  virtual ~nsUnicodeToGB18030() {}
protected:
  virtual void CreateExtensionEncoder();
  virtual void Create4BytesEncoder();
  virtual PRBool EncodeSurrogate(PRUnichar aSurrogateHigh, PRUnichar aSurrogateLow, char* aDest);
};

#ifdef MOZ_EXTRA_X11CONVERTERS
class nsUnicodeToGB18030Font0: public nsUnicodeToGB18030
{
public:
  nsUnicodeToGB18030Font0() {}
  virtual ~nsUnicodeToGB18030Font0() {}
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
  virtual ~nsUnicodeToGB18030Font1() {}
protected: 
  NS_IMETHOD FillInfo(PRUint32 *aInfo);
};
#endif 

#endif /* nsUnicodeToGBK_h___ */

