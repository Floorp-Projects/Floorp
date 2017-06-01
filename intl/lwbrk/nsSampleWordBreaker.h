/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsSampleWordBreaker_h__
#define nsSampleWordBreaker_h__


#include "nsIWordBreaker.h"

class nsSampleWordBreaker : public nsIWordBreaker
{
  NS_DECL_ISUPPORTS
public:

  nsSampleWordBreaker() ;

  bool BreakInBetween(const char16_t* aText1 , uint32_t aTextLen1,
                        const char16_t* aText2 , uint32_t aTextLen2) override;
  nsWordRange FindWord(const char16_t* aText1 , uint32_t aTextLen1,
                       uint32_t aOffset) override;

  int32_t NextWord(const char16_t* aText, uint32_t aLen, uint32_t aPos) override;

protected:
  virtual ~nsSampleWordBreaker();
};

#endif  /* nsSampleWordBreaker_h__ */
