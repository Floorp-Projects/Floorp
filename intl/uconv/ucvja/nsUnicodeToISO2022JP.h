/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsUnicodeToISO2022JP_h___
#define nsUnicodeToISO2022JP_h___

#include "nsUCvJaSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToISO2022JP [declaration]

/**
 * A character set converter from Unicode to ISO2022JP.
 *
 * @created         17/Feb/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsUnicodeToISO2022JP : public nsEncoderSupport
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToISO2022JP();

  /**
   * Class destructor.
   */
  virtual ~nsUnicodeToISO2022JP();

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);

protected:

  PRInt32                   mCharset;       // current character set
  nsIUnicodeEncodeHelper    * mHelper;      // encoder helper object

  nsresult ChangeCharset(PRInt32 aCharset, char * aDest, 
      PRInt32 * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD FinishNoBuff(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength);
  NS_IMETHOD Reset();
  NS_IMETHOD FillInfo(PRUint32 *aInfo);
};

#endif /* nsUnicodeToISO2022JP_h___ */
