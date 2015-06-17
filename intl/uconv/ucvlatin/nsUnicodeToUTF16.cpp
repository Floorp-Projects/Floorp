/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToUTF16.h"
#include "mozilla/CheckedInt.h"
#include <string.h>

NS_IMETHODIMP nsUnicodeToUTF16BE::Convert(const char16_t * aSrc, int32_t * aSrcLength, 
      char * aDest, int32_t * aDestLength)
{
  int32_t srcInLen = *aSrcLength;
  int32_t destInLen = *aDestLength;
  int32_t srcOutLen = 0;
  int32_t destOutLen = 0;
  int32_t copyCharLen;
  char16_t *p = (char16_t*)aDest;
 
  // Handle BOM if necessary 
  if (0!=mBOM) {
     if (destInLen < 2) {
        goto needmoreoutput;
     }
  
     *p++ = mBOM;
     mBOM = 0;
     destOutLen +=2;
  }
  // find out the length of copy 

  copyCharLen = srcInLen;
  if (copyCharLen > (destInLen - destOutLen) / 2) {
     copyCharLen = (destInLen - destOutLen) / 2;
  }

  // copy the data  by swaping 
  CopyData((char*)p , aSrc, copyCharLen );

  srcOutLen += copyCharLen;
  destOutLen += copyCharLen * 2;
  if (copyCharLen < srcInLen) {
      goto needmoreoutput;
  }
  
  *aSrcLength = srcOutLen;
  *aDestLength = destOutLen;
  return NS_OK;

needmoreoutput:
  *aSrcLength = srcOutLen;
  *aDestLength = destOutLen;
  return NS_OK_UENC_MOREOUTPUT;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::GetMaxLength(const char16_t * aSrc, int32_t aSrcLength, 
      int32_t * aDestLength)
{
  mozilla::CheckedInt32 length = aSrcLength;

  if (0 != mBOM) {
    length += 1;
  }

  length *= 2;

  if (!length.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aDestLength = length.value();
  return NS_OK_UENC_EXACTLENGTH;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::Finish(char * aDest, int32_t * aDestLength)
{
  if (0 != mBOM) {
     if (*aDestLength >= 2) {
        *((char16_t*)aDest)= mBOM;
        mBOM=0;
        *aDestLength = 2;
        return NS_OK;  
     } else {
        *aDestLength = 0;
        return NS_OK;  // xxx should be error
     }
  } else { 
     *aDestLength = 0;
     return NS_OK;
  } 
}

NS_IMETHODIMP nsUnicodeToUTF16BE::Reset()
{
  mBOM = 0;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::SetOutputErrorBehavior(int32_t aBehavior, 
      nsIUnicharEncoder * aEncoder, char16_t aChar)
{
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::CopyData(char* aDest, const char16_t* aSrc, int32_t aLen  )
{
  mozilla::NativeEndian::copyAndSwapToBigEndian(aDest, aSrc, aLen);
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16LE::CopyData(char* aDest, const char16_t* aSrc, int32_t aLen  )
{
  mozilla::NativeEndian::copyAndSwapToLittleEndian(aDest, aSrc, aLen);
  return NS_OK;
}
