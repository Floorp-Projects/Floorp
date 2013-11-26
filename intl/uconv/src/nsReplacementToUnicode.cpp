/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReplacementToUnicode.h"

nsReplacementToUnicode::nsReplacementToUnicode()
 : mSeenByte(false)
{
}

NS_IMETHODIMP
nsReplacementToUnicode::Convert(const char* aSrc,
                                int32_t* aSrcLength,
                                PRUnichar* aDest,
                                int32_t* aDestLength)
{
  if (mSeenByte || !(*aSrcLength)) {
    *aDestLength = 0;
    return NS_PARTIAL_MORE_INPUT;
  }
  if (mErrBehavior == kOnError_Signal) {
    mSeenByte = true;
    *aSrcLength = 0;
    *aDestLength = 0;
    return NS_ERROR_ILLEGAL_INPUT;
  }
  if (!(*aDestLength)) {
    *aSrcLength = -1;
    return NS_PARTIAL_MORE_OUTPUT;
  }
  mSeenByte = true;
  *aDest = 0xFFFD;
  *aDestLength = 1;
  return NS_PARTIAL_MORE_INPUT;
}

NS_IMETHODIMP
nsReplacementToUnicode::GetMaxLength(const char* aSrc,
                          int32_t aSrcLength,
                          int32_t* aDestLength)
{
  if (!mSeenByte && aSrcLength > 0) {
    *aDestLength = 1;
  } else {
    *aDestLength = 0;
  }
  return NS_EXACT_LENGTH;
}

NS_IMETHODIMP
nsReplacementToUnicode::Reset()
{
  mSeenByte = false;
  return NS_OK;
}
