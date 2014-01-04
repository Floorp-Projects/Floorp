/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsReplacementToUnicode_h_
#define nsReplacementToUnicode_h_

#include "nsUCSupport.h"

#define NS_REPLACEMENTTOUNICODE_CID \
  { 0xd24b24da, 0xc607, 0x489a, \
    { 0xb5, 0xf0, 0x67, 0x91, 0xf4, 0x45, 0x45, 0x6d } }

#define NS_REPLACEMENTTOUNICODE_CONTRACTID \
  "@mozilla.org/intl/unicode/decoder;1?charset=replacement"

class nsReplacementToUnicode : public nsBasicDecoderSupport
{
public:
  nsReplacementToUnicode();

  NS_IMETHOD Convert(const char* aSrc,
                     int32_t* aSrcLength,
                     char16_t* aDest,
                     int32_t* aDestLength);

  NS_IMETHOD GetMaxLength(const char* aSrc,
                          int32_t aSrcLength,
                          int32_t* aDestLength);

  NS_IMETHOD Reset();

private:
  bool mSeenByte;
};

#endif // nsReplacementToUnicode_h_
