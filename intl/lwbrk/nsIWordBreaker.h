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
  uint32_t mBegin;
  uint32_t mEnd;
} nsWordRange;

enum nsWordBreakClass : uint8_t {
  kWbClassSpace = 0,
  kWbClassAlphaLetter,
  kWbClassPunct,
  kWbClassHanLetter,
  kWbClassKatakanaLetter,
  kWbClassHiraganaLetter,
  kWbClassHWKatakanaLetter,
  kWbClassThaiLetter
};

class nsIWordBreaker : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWORDBREAKER_IID)

  virtual bool BreakInBetween(const char16_t* aText1 , uint32_t aTextLen1,
                                const char16_t* aText2 ,
                                uint32_t aTextLen2) = 0;
  virtual nsWordRange FindWord(const char16_t* aText1 , uint32_t aTextLen1,
                               uint32_t aOffset) = 0;
  virtual int32_t NextWord(const char16_t* aText, uint32_t aLen,
                           uint32_t aPos) = 0;

  static nsWordBreakClass GetClass(char16_t aChar);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIWordBreaker, NS_IWORDBREAKER_IID)

#endif  /* nsIWordBreaker_h__ */
