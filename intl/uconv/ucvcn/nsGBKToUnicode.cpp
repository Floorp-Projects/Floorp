/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/**
 * A character set converter from GBK to Unicode.
 * 
 *
 * @created         07/Sept/1999
 * @author  Yueheng Xu, Yueheng.Xu@intel.com
 */

#include "nsGBKToUnicode.h"
#include "gbku.h"
#include "nsUnicodeDecodeHelper.h"

static const uint16_t g_utGB18030Unique2Bytes[] = {
#include "gb18030uniq2b.ut"
};

static const uint16_t g_utGB18030Unique4Bytes[] = {
#include "gb180304bytes.ut"
};

//----------------------------------------------------------------------
// Class nsGB18030ToUnicode [implementation]

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

#define LEGAL_GBK_MULTIBYTE_FIRST_BYTE(c)  \
      (UINT8_IN_RANGE(0x81, (c), 0xFE))
#define FIRST_BYTE_IS_SURROGATE(c)  \
      (UINT8_IN_RANGE(0x90, (c), 0xFE))
#define LEGAL_GBK_2BYTE_SECOND_BYTE(c) \
      (UINT8_IN_RANGE(0x40, (c), 0x7E)|| UINT8_IN_RANGE(0x80, (c), 0xFE))
#define LEGAL_GBK_4BYTE_SECOND_BYTE(c) \
      (UINT8_IN_RANGE(0x30, (c), 0x39))
#define LEGAL_GBK_4BYTE_THIRD_BYTE(c)  \
      (UINT8_IN_RANGE(0x81, (c), 0xFE))
#define LEGAL_GBK_4BYTE_FORTH_BYTE(c) \
      (UINT8_IN_RANGE(0x30, (c), 0x39))

NS_IMETHODIMP nsGB18030ToUnicode::ConvertNoBuff(const char* aSrc,
                                                int32_t * aSrcLength,
                                                char16_t *aDest,
                                                int32_t * aDestLength)
{
  int32_t i=0;
  int32_t iSrcLength = (*aSrcLength);
  int32_t iDestlen = 0;
  nsresult rv=NS_OK;
  *aSrcLength = 0;
  
  for (i=0;i<iSrcLength;i++)
  {
    if ( iDestlen >= (*aDestLength) )
    {
      rv = NS_OK_UDEC_MOREOUTPUT;
      break;
    }
    // The valid range for the 1st byte is [0x81,0xFE] 
    if(LEGAL_GBK_MULTIBYTE_FIRST_BYTE(*aSrc))
    {
      if(i+1 >= iSrcLength) 
      {
        rv = NS_OK_UDEC_MOREINPUT;
        break;
      }
      // To make sure, the second byte has to be checked as well.
      // In GBK, the second byte range is [0x40,0x7E] and [0x80,0XFE]
      if(LEGAL_GBK_2BYTE_SECOND_BYTE(aSrc[1]))
      {
        // Valid GBK code
        *aDest = mUtil.GBKCharToUnicode(aSrc[0], aSrc[1]);
        if(UCS2_NO_MAPPING == *aDest)
        { 
          // We cannot map in the common mapping, let's call the
          // delegate 2 byte decoder to decode the gbk or gb18030 unique 
          // 2 byte mapping
          if(! TryExtensionDecoder(aSrc, aDest))
          {
            *aDest = UCS2_NO_MAPPING;
          }
        }
        aSrc += 2;
        i++;
      }
      else if (LEGAL_GBK_4BYTE_SECOND_BYTE(aSrc[1]))
      {
        // from the first 2 bytes, it looks like a 4 byte GB18030
        if(i+3 >= iSrcLength)  // make sure we got 4 bytes
        {
          rv = NS_OK_UDEC_MOREINPUT;
          break;
        }
        // 4 bytes patten
        // [0x81-0xfe][0x30-0x39][0x81-0xfe][0x30-0x39]
        // preset the 
 
        if (LEGAL_GBK_4BYTE_THIRD_BYTE(aSrc[2]) &&
            LEGAL_GBK_4BYTE_FORTH_BYTE(aSrc[3]))
        {
           if ( ! FIRST_BYTE_IS_SURROGATE(aSrc[0])) 
           {
             // let's call the delegated 4 byte gb18030 converter to convert it
             if(! Try4BytesDecoder(aSrc, aDest))
               *aDest = UCS2_NO_MAPPING;
           } else {
              // let's try supplement mapping
             if ( (iDestlen+1) < (*aDestLength) )
             {
               if(DecodeToSurrogate(aSrc, aDest))
               {
                 // surrogte two char16_t
                 iDestlen++;
                 aDest++;
               }  else {
                 *aDest = UCS2_NO_MAPPING;
              }
             } else {
               if (*aDestLength < 2) {
                 NS_ERROR("insufficient space in output buffer");
                 *aDest = UCS2_NO_MAPPING;
               } else {
                 rv = NS_OK_UDEC_MOREOUTPUT;
                 break;
               }
             }
           }
           aSrc += 4;
           i += 3;
        } else {
          *aDest = UCS2_NO_MAPPING; 
          // If the third and fourth bytes are not in the legal ranges for
          // a four-byte sequnce, resynchronize on the second byte
          // (which we know is in the range of LEGAL_GBK_4BYTE_SECOND_BYTE,
          //  0x30-0x39)
          aSrc++;
        }
      }
      else if ((uint8_t) aSrc[0] == (uint8_t)0xA0 )
      {
        // stand-alone (not followed by a valid second byte) 0xA0 !
        // treat it as valid a la Netscape 4.x
        *aDest = CAST_CHAR_TO_UNICHAR(*aSrc);
        aSrc++;
      } else {
        // Invalid GBK code point (second byte should be 0x40 or higher)
        *aDest = UCS2_NO_MAPPING;
        aSrc++;
      }
    } else {
      if(IS_ASCII(*aSrc))
      {
        // The source is an ASCII
        *aDest = CAST_CHAR_TO_UNICHAR(*aSrc);
        aSrc++;
      } else {
        if(IS_GBK_EURO(*aSrc)) {
          *aDest = UCS2_EURO;
        } else {
          *aDest = UCS2_NO_MAPPING;
        }
        aSrc++;
      }
    }
    iDestlen++;
    aDest++;
    *aSrcLength = i+1;
  }
  *aDestLength = iDestlen;
  return rv;
}

bool nsGB18030ToUnicode::DecodeToSurrogate(const char* aSrc, char16_t* aOut)
{
  NS_ASSERTION(FIRST_BYTE_IS_SURROGATE(aSrc[0]),       "illegal first byte");
  NS_ASSERTION(LEGAL_GBK_4BYTE_SECOND_BYTE(aSrc[1]),   "illegal second byte");
  NS_ASSERTION(LEGAL_GBK_4BYTE_THIRD_BYTE(aSrc[2]),    "illegal third byte");
  NS_ASSERTION(LEGAL_GBK_4BYTE_FORTH_BYTE(aSrc[3]),    "illegal forth byte");
  if(! FIRST_BYTE_IS_SURROGATE(aSrc[0]))
    return false;
  if(! LEGAL_GBK_4BYTE_SECOND_BYTE(aSrc[1]))
    return false;
  if(! LEGAL_GBK_4BYTE_THIRD_BYTE(aSrc[2]))
    return false;
  if(! LEGAL_GBK_4BYTE_FORTH_BYTE(aSrc[3]))
    return false;

  uint8_t a1 = (uint8_t) aSrc[0];
  uint8_t a2 = (uint8_t) aSrc[1];
  uint8_t a3 = (uint8_t) aSrc[2];
  uint8_t a4 = (uint8_t) aSrc[3];
  a1 -= (uint8_t)0x90;
  a2 -= (uint8_t)0x30;
  a3 -= (uint8_t)0x81;
  a4 -= (uint8_t)0x30;
  uint32_t idx = (((a1 * 10 + a2 ) * 126 + a3) * 10) + a4;
  // idx == ucs4Codepoint - 0x10000
  if (idx > 0x000FFFFF)
    return false;

  *aOut++ = 0xD800 | (idx >> 10);
  *aOut = 0xDC00 | (0x000003FF & idx);

  return true;
}
bool nsGB18030ToUnicode::TryExtensionDecoder(const char* aSrc, char16_t* aOut)
{
  int32_t len = 2;
  int32_t dstlen = 1;
  nsresult res =
    nsUnicodeDecodeHelper::ConvertByTable(aSrc, &len, aOut, &dstlen,
                                          u2BytesCharset, nullptr,
                                          (uMappingTable*) &g_utGB18030Unique2Bytes,
                                          false);
  NS_ASSERTION(NS_FAILED(res) || ((len==2) && (dstlen == 1)),
               "some strange conversion result");
  // if we failed, we then just use the 0xfffd
  // therefore, we ignore the res here.
  return NS_SUCCEEDED(res);
}

bool nsGB18030ToUnicode::Try4BytesDecoder(const char* aSrc, char16_t* aOut)
{
  int32_t len = 4;
  int32_t dstlen = 1;
  nsresult res =
    nsUnicodeDecodeHelper::ConvertByTable(aSrc, &len, aOut, &dstlen,
                                          u4BytesGB18030Charset, nullptr,
                                          (uMappingTable*) &g_utGB18030Unique4Bytes,
                                          false); 
  NS_ASSERTION(NS_FAILED(res) || ((len==4) && (dstlen == 1)),
               "some strange conversion result");
  // if we failed, we then just use the 0xfffd
  // therefore, we ignore the res here.
  return NS_SUCCEEDED(res);
}
