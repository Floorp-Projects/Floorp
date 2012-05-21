/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILineBreaker_h__
#define nsILineBreaker_h__

#include "nsISupports.h"

#include "nscore.h"

#define NS_LINEBREAKER_NEED_MORE_TEXT -1

// 	{0x4b0b9e04-6ffb-4647-aa5f-2fa2ebd883e8}
#define NS_ILINEBREAKER_IID \
{0x4b0b9e04, 0x6ffb, 0x4647, \
    {0xaa, 0x5f, 0x2f, 0xa2, 0xeb, 0xd8, 0x83, 0xe8}}

class nsILineBreaker : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILINEBREAKER_IID)

  enum {
    kWordBreak_Normal   = 0, // default
    kWordBreak_BreakAll = 1, // break all
    kWordBreak_KeepAll  = 2  // always keep 
  };

  virtual PRInt32 Next( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

  virtual PRInt32 Prev( const PRUnichar* aText, PRUint32 aLen, 
                        PRUint32 aPos) = 0;

  // Call this on a word with whitespace at either end. We will apply JISx4501
  // rules to find breaks inside the word. aBreakBefore is set to the break-
  // before status of each character; aBreakBefore[0] will always be false
  // because we never return a break before the first character.
  // aLength is the length of the aText array and also the length of the aBreakBefore
  // output array.
  virtual void GetJISx4051Breaks(const PRUnichar* aText, PRUint32 aLength,
                                 PRUint8 aWordBreak,
                                 PRUint8* aBreakBefore) = 0;
  virtual void GetJISx4051Breaks(const PRUint8* aText, PRUint32 aLength,
                                 PRUint8 aWordBreak,
                                 PRUint8* aBreakBefore) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILineBreaker, NS_ILINEBREAKER_IID)

static inline bool
NS_IsSpace(PRUnichar u)
{
  return u == 0x0020 ||                  // SPACE
         u == 0x0009 ||                  // CHARACTER TABULATION
         u == 0x000D ||                  // CARRIAGE RETURN
         (0x2000 <= u && u <= 0x2006) || // EN QUAD, EM QUAD, EN SPACE,
                                         // EM SPACE, THREE-PER-EM SPACE,
                                         // FOUR-PER-SPACE, SIX-PER-EM SPACE,
         (0x2008 <= u && u <= 0x200B) || // PUNCTUATION SPACE, THIN SPACE,
                                         // HAIR SPACE, ZERO WIDTH SPACE
         u == 0x3000;                    // IDEOGRAPHIC SPACE
}

static inline bool
NS_NeedsPlatformNativeHandling(PRUnichar aChar)
{
  return (0x0e01 <= aChar && aChar <= 0x0fff); // Thai, Lao, Tibetan
}

#endif  /* nsILineBreaker_h__ */
