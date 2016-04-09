/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 /**
 * A character set converter from Unicode to GBK.
 * 
 *
 * @created         08/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 * Revision History
 * 04/Oct/1999. Yueheng Xu: used table gUnicodeToGBKTable[0x5200] to make 
 *              Unicode to GB mapping fast 
 */

#include "nsUnicodeToGBK.h"
#include "gbku.h"
#include "uconvutil.h"
#include "nsCharTraits.h"
#include "nsUnicodeEncodeHelper.h"

//-------------------------------------------------------------
// Global table initialization function defined in gbku.h
//-------------------------------------------------------------

static const uint16_t g_uf_gb18030_2bytes[] = {
#include "gb18030uniq2b.uf"
};
static const uint16_t g_uf_gb18030_4bytes[] = {
#include "gb180304bytes.uf"
};
static const uint16_t g_uf_gbk[] = {
#include "gbkuniq.uf"
};
//-----------------------------------------------------------------------
//  nsUnicodeToGB18030
//-----------------------------------------------------------------------
nsresult nsUnicodeToGB18030::TryExtensionEncoder(char16_t aChar,
                                                 char* aOut,
                                                 int32_t *aOutLen)
{
  int32_t len = 1;
  return nsUnicodeEncodeHelper::ConvertByTable(
    &aChar, &len, aOut, aOutLen, u2BytesCharset, nullptr,
    (uMappingTable*) &g_uf_gb18030_2bytes);
}

nsresult nsUnicodeToGB18030::Try4BytesEncoder(char16_t aChar,
                                              char* aOut,
                                              int32_t *aOutLen)
{
  int32_t len = 1;
  nsresult res = nsUnicodeEncodeHelper::ConvertByTable(
    &aChar, &len, aOut, aOutLen, u4BytesGB18030Charset, nullptr,
    (uMappingTable*) &g_uf_gb18030_4bytes);
  MOZ_ASSERT((res != NS_OK) || ((1 == len) && (4 == *aOutLen)),
             "unexpect conversion length");
  return res;
}

nsresult nsUnicodeToGB18030::EncodeSurrogate(char16_t aSurrogateHigh,
                                             char16_t aSurrogateLow,
                                             char* aOut,
                                             int32_t aDestLength,
                                             int32_t aBufferLength)
{
  if( NS_IS_HIGH_SURROGATE(aSurrogateHigh) &&
      NS_IS_LOW_SURROGATE(aSurrogateLow) )
  {
    // notice that idx does not include the 0x10000
    uint32_t idx = ((aSurrogateHigh - (char16_t)0xD800) << 10 ) |
                   (aSurrogateLow - (char16_t) 0xDC00);

    if (aDestLength + 4 > aBufferLength) {
      return NS_OK_UENC_MOREOUTPUT;
    }

    unsigned char *out = (unsigned char*) aOut;
    // notice this is from 0x90 for supplementary planes
    out[0] = (idx / (10*126*10)) + 0x90;
    idx %= (10*126*10);
    out[1] = (idx / (10*126)) + 0x30;
    idx %= (10*126);
    out[2] = (idx / (10)) + 0x81;
    out[3] = (idx % 10) + 0x30;
    return NS_OK;
  }
  return NS_ERROR_UENC_NOMAPPING;
}

//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]

nsUnicodeToGBK::nsUnicodeToGBK(uint32_t aMaxLength) :
  nsEncoderSupport(aMaxLength), mSurrogateHigh(0)
{
}

nsresult nsUnicodeToGBK::TryExtensionEncoder(char16_t aChar,
                                             char* aOut,
                                             int32_t *aOutLen)
{
  int32_t len = 1;
  return nsUnicodeEncodeHelper::ConvertByTable(
    &aChar, &len, aOut, aOutLen, u1ByteCharset, nullptr,
    (uMappingTable*) &g_uf_gbk);
}

nsresult nsUnicodeToGBK::Try4BytesEncoder(char16_t aChar,
                                          char* aOut,
                                          int32_t *aOutLen)
{
  return NS_ERROR_UENC_NOMAPPING;
}

nsresult nsUnicodeToGBK::EncodeSurrogate(char16_t aSurrogateHigh,
                                         char16_t aSurrogateLow,
                                         char* aOut,
                                         int32_t aDestLength,
                                         int32_t aBufferLength)
{
  return NS_ERROR_UENC_NOMAPPING; // GBK cannot encode Surrogate, let the subclass encode it.
}

NS_IMETHODIMP nsUnicodeToGBK::ConvertNoBuffNoErr(const char16_t * aSrc,
                                                 int32_t * aSrcLength,
                                                 char * aDest,
                                                 int32_t * aDestLength)
{
  int32_t iSrcLength = 0;
  int32_t iDestLength = 0;
  char16_t unicode;
  nsresult res = NS_OK;
  while (iSrcLength < *aSrcLength )
  {
    unicode = *aSrc;
    //if unicode's hi byte has something, it is not ASCII, must be a GB
    if (IS_ASCII(unicode)) {
      // make sure we still have 1 byte for output first
      if (iDestLength >= *aDestLength) {
        res = NS_OK_UENC_MOREOUTPUT;
        break;
      }
      // this is an ASCII
      *aDest = CAST_UNICHAR_TO_CHAR(*aSrc);
      aDest++; // increment 1 byte
      iDestLength +=1;
    } else {
      char byte1, byte2;
      if(mUtil.UnicodeToGBKChar( unicode, false, &byte1, &byte2))
      {
        // make sure we still have 2 bytes for output first
        if(iDestLength+2 > *aDestLength)
        {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        aDest[0] = byte1;
        aDest[1] = byte2;
        aDest += 2;	// increment 2 bytes
        iDestLength +=2;
      } else {
        // Swapped character in GB18030-2005
        if (unicode == 0xE7C7) {
          unicode = 0x1E3F;
        }
        // we cannot map in the common mapping. Let's try to
        // call the delegated 2 byte converter for the gbk or gb18030
        // unique 2 byte mapping
        int32_t outLen = *aDestLength - iDestLength;
        if (NS_IS_HIGH_SURROGATE(unicode) ||
            NS_IS_LOW_SURROGATE(unicode)) {
          // performance tune for surrogate characters
          res = NS_ERROR_UENC_NOMAPPING;
        } else {
          res = TryExtensionEncoder(unicode, aDest, &outLen);
        }
        if (res == NS_OK) {
          iDestLength += outLen;
          aDest += outLen;
        } else if (res == NS_OK_UENC_MOREOUTPUT) {
          break;
        } else {
          // we still cannot map. Let's try to
          // call the delegated GB18030 4 byte converter
          if( NS_IS_HIGH_SURROGATE(unicode) )
          {
            if((iSrcLength+1) < *aSrcLength ) {
              res = EncodeSurrogate(aSrc[0],aSrc[1], aDest,
                                    iDestLength, *aDestLength);
              if (res == NS_OK) {
                // since we got a surrogate pair, we need to increment src.
                iSrcLength++ ;
                aSrc++;
                iDestLength += 4;
                aDest += 4;
              } else {
                if (res == NS_ERROR_UENC_NOMAPPING) {
                  // only get a high surrogate, but not a low surrogate
                  iSrcLength++;   // include length of the unmapped character
                }
                break;
              }
            } else {
              mSurrogateHigh = aSrc[0];
              res = NS_OK;
              break; // this will go to afterwhileloop
            }
          } else {
            if( NS_IS_LOW_SURROGATE(unicode) )
            {
              if(NS_IS_HIGH_SURROGATE(mSurrogateHigh)) {
                res = EncodeSurrogate(mSurrogateHigh, aSrc[0], aDest,
                                      iDestLength, *aDestLength);
                if (res == NS_OK) {
                  iDestLength += 4;
                  aDest += 4;
                } else {
                  if (res == NS_ERROR_UENC_NOMAPPING) {
                    // only get a high surrogate, but not a low surrogate
                    iSrcLength++;   // include length of the unmapped character
                  }
                  break;
                }
              } else {
                // only get a low surrogate, but not a low surrogate
                res = NS_ERROR_UENC_NOMAPPING;
                iSrcLength++;   // include length of the unmapped character
                break;
              }
            } else {
              outLen = *aDestLength - iDestLength;
              res = Try4BytesEncoder(unicode, aDest, &outLen);
              if (res == NS_OK) {
                iDestLength += outLen;
                aDest += outLen;
              } else {
                if (res == NS_ERROR_UENC_NOMAPPING) {
                  iSrcLength++;   // include length of the unmapped character
                }
                break;
              }
            }
          }
        }
      }
    }
    iSrcLength++ ; // Each unicode char just count as one in char16_t string;
    mSurrogateHigh = 0;
    aSrc++;
  }
//afterwhileloop:
  *aDestLength = iDestLength;
  *aSrcLength = iSrcLength;
  return res;
}
