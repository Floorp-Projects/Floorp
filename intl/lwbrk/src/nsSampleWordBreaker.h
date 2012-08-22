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

  bool BreakInBetween(const PRUnichar* aText1 , uint32_t aTextLen1,
                        const PRUnichar* aText2 , uint32_t aTextLen2);
  nsWordRange FindWord(const PRUnichar* aText1 , uint32_t aTextLen1,
                       uint32_t aOffset);

  int32_t NextWord(const PRUnichar* aText, uint32_t aLen, uint32_t aPos);

protected:
  uint8_t  GetClass(PRUnichar aChar);
};

#endif  /* nsSampleWordBreaker_h__ */
