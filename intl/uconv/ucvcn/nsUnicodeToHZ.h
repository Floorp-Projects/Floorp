/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

 /**
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#ifndef nsUnicodeToHZ_h___
#define nsUnicodeToHZ_h___

#include "nsUCvCnSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToHZ [declaration]

class nsUnicodeToHZ: public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToHZ(){};
  virtual ~nsUnicodeToHZ(){};

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);


protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, 
                            char * aDest, 
                            PRInt32 * aDestLength);

  NS_IMETHOD ConvertNoBuffNoErr( const PRUnichar * aSrc, 
                                 PRInt32 * aSrcLength, 
                                 char * aDest, 
                                 PRInt32 * aDestLength);

  NS_IMETHOD FillInfo(PRUint32 *aInfo);

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);

private:

typedef struct
{
    char leftbyte;
    char rightbyte;

} DByte;
  void UnicodeToHZ(PRUnichar SrcUnicode, DByte *pGBCode);

};

#endif /* nsUnicodeToHZ_h___ */
