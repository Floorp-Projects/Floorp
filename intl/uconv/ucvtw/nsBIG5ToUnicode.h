/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBIG5ToUnicode_h___
#define nsBIG5ToUnicode_h___

#include "nsUCSupport.h"

#define NS_BIG5TOUNICODE_CID \
  { 0xefc323e1, 0xec62, 0x11d2, \
    { 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } }

#define NS_BIG5TOUNICODE_CONTRACTID \
  "@mozilla.org/intl/unicode/decoder;1?charset=big5"

class nsBIG5ToUnicode : public nsBasicDecoderSupport
{
public:
  nsBIG5ToUnicode();

  NS_IMETHOD Convert(const char* aSrc,
                     int32_t* aSrcLength,
                     char16_t* aDest,
                     int32_t* aDestLength);

  NS_IMETHOD GetMaxLength(const char* aSrc,
                          int32_t aSrcLength,
                          int32_t* aDestLength);

  NS_IMETHOD Reset();

private:
  char16_t mPendingTrail;
  uint8_t  mBig5Lead;
};

#endif /* nsBIG5ToUnicode_h___ */
