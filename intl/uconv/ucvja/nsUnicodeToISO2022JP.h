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

  int32_t                   mCharset;       // current character set

  nsresult ChangeCharset(int32_t aCharset, char * aDest, 
      int32_t * aDestLength);
  nsresult ConvertHankaku(const char16_t *aSrc, int32_t * aSrcLength,
                          char *aDest, int32_t * aDestLength);

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD ConvertNoBuffNoErr(const char16_t * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength);
  NS_IMETHOD FinishNoBuff(char * aDest, int32_t * aDestLength);
  NS_IMETHOD Reset();
};

#endif /* nsUnicodeToISO2022JP_h___ */
