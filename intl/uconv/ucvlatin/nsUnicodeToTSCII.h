/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToTSCII_h___
#define nsUnicodeToTSCII_h___

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIUnicodeEncoder.h"

//----------------------------------------------------------------------
// Class nsUnicodeToTSCII [declaration]

class nsUnicodeToTSCII : public nsIUnicodeEncoder
{

NS_DECL_ISUPPORTS

public:
  nsUnicodeToTSCII() { mBuffer = 0; }
  virtual ~nsUnicodeToTSCII() {}

  NS_IMETHOD Convert(const PRUnichar * aSrc, int32_t * aSrcLength,
                     char * aDest, int32_t * aDestLength);

  NS_IMETHOD Finish(char * aDest, int32_t * aDestLength);

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, int32_t aSrcLength,
                          int32_t * aDestLength);

  NS_IMETHOD Reset();

  NS_IMETHOD SetOutputErrorBehavior(int32_t aBehavior,
                                    nsIUnicharEncoder * aEncoder, 
                                    PRUnichar aChar);

private:
  uint32_t mBuffer; // buffer for character(s) to be combined with the following
                    // character. Up to 4 single byte characters can be 
                    // stored. 
};

#define CHAR_BUFFER_SIZE 2048

//----------------------------------------------------------------------
// Class nsUnicodeToTamilTTF [declaration]

class nsUnicodeToTamilTTF : public nsUnicodeToTSCII
{
  NS_DECL_ISUPPORTS_INHERITED

public:
  nsUnicodeToTamilTTF() : nsUnicodeToTSCII() {}
  virtual ~nsUnicodeToTamilTTF() {}

  NS_IMETHOD Convert      (const PRUnichar * aSrc, int32_t * aSrcLength,
                           char * aDest, int32_t * aDestLength);
  NS_IMETHOD GetMaxLength (const PRUnichar * aSrc, int32_t  aSrcLength,
                           int32_t * aDestLength);

  NS_IMETHOD SetOutputErrorBehavior (int32_t aBehavior, 
                                     nsIUnicharEncoder *aEncoder, 
                                     PRUnichar aChar);

private:
  char mStaticBuffer[CHAR_BUFFER_SIZE];
  int32_t mErrBehavior;
  PRUnichar mErrChar;
  nsCOMPtr<nsIUnicharEncoder> mErrEncoder;
};

#endif /* nsUnicodeToTSCII_h___ */
