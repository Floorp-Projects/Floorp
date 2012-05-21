/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToISO2022JP_h___
#define nsUnicodeToISO2022JP_h___

#include "nsUCSupport.h"

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

protected:

  PRInt32                   mCharset;       // current character set

  nsresult ChangeCharset(PRInt32 aCharset, char * aDest, 
      PRInt32 * aDestLength);
  nsresult ConvertHankaku(const PRUnichar *aSrc, PRInt32 * aSrcLength,
                          char *aDest, PRInt32 * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD FinishNoBuff(char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Reset();
};

#endif /* nsUnicodeToISO2022JP_h___ */
