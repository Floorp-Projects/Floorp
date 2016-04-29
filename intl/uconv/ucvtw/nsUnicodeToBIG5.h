/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUnicodeToBIG5_h_
#define nsUnicodeToBIG5_h_

#include "nsIUnicodeEncoder.h"

#define NS_UNICODETOBIG5_CID \
  { 0xefc323e2, 0xec62, 0x11d2, \
    { 0x8a, 0xac, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } }

class nsUnicodeToBIG5 : public nsIUnicodeEncoder
{
public:
  // Encoders probably shouldn't use the thread-safe variant, but we should
  // make a systematic change instead of making this class different.
  NS_DECL_THREADSAFE_ISUPPORTS

  nsUnicodeToBIG5();

  NS_IMETHOD Convert(const char16_t* aSrc,
                     int32_t* aSrcLength,
                     char* aDest,
                     int32_t * aDestLength) override;

  NS_IMETHOD Finish(char* aDest,
                    int32_t* aDestLength) override;

  MOZ_MUST_USE NS_IMETHOD GetMaxLength(const char16_t* aSrc,
                                       int32_t aSrcLength,
                                       int32_t* aDestLength) override;

  NS_IMETHOD Reset() override;

  NS_IMETHOD SetOutputErrorBehavior(int32_t aBehavior,
                                    nsIUnicharEncoder* aEncoder,
                                    char16_t aChar) override;

private:
  virtual ~nsUnicodeToBIG5(){};

  char16_t mUtf16Lead;
  uint8_t  mPendingTrail;
  bool     mSignal;
};

#endif /* nsUnicodeToBIG5_h_ */
