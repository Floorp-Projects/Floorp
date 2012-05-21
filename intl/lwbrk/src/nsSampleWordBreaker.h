/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSampleWordBreaker_h__
#define nsSampleWordBreaker_h__


#include "nsIWordBreaker.h"

typedef enum {
  kWbClassSpace = 0,
  kWbClassAlphaLetter,
  kWbClassPunct,
  kWbClassHanLetter,
  kWbClassKatakanaLetter,
  kWbClassHiraganaLetter,
  kWbClassHWKatakanaLetter,
  kWbClassThaiLetter
} wb_class;

class nsSampleWordBreaker : public nsIWordBreaker
{
  NS_DECL_ISUPPORTS
public:

  nsSampleWordBreaker() ;
  virtual ~nsSampleWordBreaker() ;

  bool BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                        const PRUnichar* aText2 , PRUint32 aTextLen2);
  nsWordRange FindWord(const PRUnichar* aText1 , PRUint32 aTextLen1,
                       PRUint32 aOffset);

  PRInt32 NextWord(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos);

protected:
  PRUint8  GetClass(PRUnichar aChar);
};

#endif  /* nsSampleWordBreaker_h__ */
