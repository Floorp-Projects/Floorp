/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIWordBreaker_h__
#define nsIWordBreaker_h__

#include "nsISupports.h"

#include "nscore.h"

#define NS_WORDBREAKER_NEED_MORE_TEXT -1

// {E86B3379-BF89-11d2-B3AF-00805F8A6670}
#define NS_IWORDBREAKER_IID \
{ 0xe86b3379, 0xbf89, 0x11d2, \
   { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }

typedef struct {
  PRUint32 mBegin;
  PRUint32 mEnd;
} nsWordRange;

class nsIWordBreaker : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWORDBREAKER_IID)

  virtual bool BreakInBetween(const PRUnichar* aText1 , PRUint32 aTextLen1,
                                const PRUnichar* aText2 ,
                                PRUint32 aTextLen2) = 0;
  virtual nsWordRange FindWord(const PRUnichar* aText1 , PRUint32 aTextLen1,
                               PRUint32 aOffset) = 0;
  virtual PRInt32 NextWord(const PRUnichar* aText, PRUint32 aLen, 
                           PRUint32 aPos) = 0;
                           
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWordBreaker, NS_IWORDBREAKER_IID)

#endif  /* nsIWordBreaker_h__ */
