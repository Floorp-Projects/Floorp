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

#ifndef nsHZToUnicode_h___
#define nsHZToUnicode_h___

#include "nsUCvCnSupport.h"

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
  nsHZToUnicode() {};
  virtual ~nsHZToUnicode() {};

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);


protected:

  //--------------------------------------------------------------------
  // Subclassing of nsDecoderSupport class [declaration]
  NS_IMETHOD ConvertNoBuff(const char* aSrc,
											   PRInt32 * aSrcLength,
											   PRUnichar *aDest,
										       PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(const char * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);

private:

typedef struct
{
    char leftbyte;
    char rightbyte;

} DByte;
  void HZToUnicode(DByte *pGBCode, PRUnichar * pUnicode);
  void GBToUnicode(DByte *pGBCode, PRUnichar * pUnicode);

};

#endif /* nsHZToUnicode_h___ */
