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
#include "nsUCvCnDll.h"
#include "gbku.h"


//------------------------------------------------------------
// nsGBKUnique2BytesToUnicode
//------------------------------------------------------------
class nsGBKUnique2BytesToUnicode : public nsTableDecoderSupport 
{
public:
  nsGBKUnique2BytesToUnicode();
  virtual ~nsGBKUnique2BytesToUnicode() 
    { }
protected:
};

static const PRUint16 g_utGBKUnique2Bytes[] = {
#include "gbkuniq2b.ut"
};
nsGBKUnique2BytesToUnicode::nsGBKUnique2BytesToUnicode() 
  : nsTableDecoderSupport(u2BytesCharset, nsnull,
        (uMappingTable*) &g_utGBKUnique2Bytes, 1) 
{
}

//------------------------------------------------------------
// nsGB18030Unique2BytesToUnicode
//------------------------------------------------------------
class nsGB18030Unique2BytesToUnicode : public nsTableDecoderSupport 
{
public:
  nsGB18030Unique2BytesToUnicode();
  virtual ~nsGB18030Unique2BytesToUnicode() 
    { }
protected:
};

static const PRUint16 g_utGB18030Unique2Bytes[] = {
#include "gb18030uniq2b.ut"
};
nsGB18030Unique2BytesToUnicode::nsGB18030Unique2BytesToUnicode() 
  : nsTableDecoderSupport(u2BytesCharset, nsnull,
        (uMappingTable*) &g_utGB18030Unique2Bytes, 1) 
{
}

//------------------------------------------------------------
// nsGB18030Unique4BytesToUnicode
//------------------------------------------------------------
class nsGB18030Unique4BytesToUnicode : public nsTableDecoderSupport 
{
public:
  nsGB18030Unique4BytesToUnicode();
  virtual ~nsGB18030Unique4BytesToUnicode() 
    { }
protected:
};

static const PRUint16 g_utGB18030Unique4Bytes[] = {
#include "gb180304bytes.ut"
};
nsGB18030Unique4BytesToUnicode::nsGB18030Unique4BytesToUnicode() 
  : nsTableDecoderSupport(u4BytesGB18030Charset, nsnull,
        (uMappingTable*) &g_utGB18030Unique4Bytes, 1) 
{
}


//----------------------------------------------------------------------
// Class nsGBKToUnicode [implementation]

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

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

NS_IMETHODIMP nsGBKToUnicode::ConvertNoBuff(const char* aSrc,
                                            PRInt32 * aSrcLength,
                                            PRUnichar *aDest,
                                            PRInt32 * aDestLength)
{
  PRInt32 i=0;
  PRInt32 iSrcLength = (*aSrcLength);
  PRInt32 iDestlen = 0;
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
                 // surrogte two PRUnichar
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
      else if ((PRUint8) aSrc[0] == (PRUint8)0xA0 )
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


void nsGBKToUnicode::CreateExtensionDecoder()
{
  mExtensionDecoder = new nsGBKUnique2BytesToUnicode();
}
void nsGBKToUnicode::Create4BytesDecoder()
{
  m4BytesDecoder =  nsnull;
}
void nsGB18030ToUnicode::CreateExtensionDecoder()
{
  mExtensionDecoder = new nsGB18030Unique2BytesToUnicode();
}
void nsGB18030ToUnicode::Create4BytesDecoder()
{
  m4BytesDecoder = new nsGB18030Unique4BytesToUnicode();
}
bool nsGB18030ToUnicode::DecodeToSurrogate(const char* aSrc, PRUnichar* aOut)
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

  PRUint8 a1 = (PRUint8) aSrc[0];
  PRUint8 a2 = (PRUint8) aSrc[1];
  PRUint8 a3 = (PRUint8) aSrc[2];
  PRUint8 a4 = (PRUint8) aSrc[3];
  a1 -= (PRUint8)0x90;
  a2 -= (PRUint8)0x30;
  a3 -= (PRUint8)0x81;
  a4 -= (PRUint8)0x30;
  PRUint32 idx = (((a1 * 10 + a2 ) * 126 + a3) * 10) + a4;
  // idx == ucs4Codepoint - 0x10000
  if (idx > 0x000FFFFF)
    return false;

  *aOut++ = 0xD800 | (idx >> 10);
  *aOut = 0xDC00 | (0x000003FF & idx);

  return true;
}
bool nsGBKToUnicode::TryExtensionDecoder(const char* aSrc, PRUnichar* aOut)
{
  if(!mExtensionDecoder)
    CreateExtensionDecoder();
  NS_ASSERTION(mExtensionDecoder, "cannot creqte 2 bytes unique converter");
  if(mExtensionDecoder)
  {
    nsresult res = mExtensionDecoder->Reset();
    NS_ASSERTION(NS_SUCCEEDED(res), "2 bytes unique conversoin reset failed");
    PRInt32 len = 2;
    PRInt32 dstlen = 1;
    res = mExtensionDecoder->Convert(aSrc,&len, aOut, &dstlen); 
    NS_ASSERTION(NS_FAILED(res) || ((len==2) && (dstlen == 1)), 
       "some strange conversion result");
     // if we failed, we then just use the 0xfffd 
     // therefore, we ignore the res here. 
    if(NS_SUCCEEDED(res)) 
      return true;
  }
  return  false;
}
bool nsGBKToUnicode::DecodeToSurrogate(const char* aSrc, PRUnichar* aOut)
{
  return false;
}
bool nsGBKToUnicode::Try4BytesDecoder(const char* aSrc, PRUnichar* aOut)
{
  if(!m4BytesDecoder)
    Create4BytesDecoder();
  if(m4BytesDecoder)
  {
    nsresult res = m4BytesDecoder->Reset();
    NS_ASSERTION(NS_SUCCEEDED(res), "4 bytes unique conversoin reset failed");
    PRInt32 len = 4;
    PRInt32 dstlen = 1;
    res = m4BytesDecoder->Convert(aSrc,&len, aOut, &dstlen); 
    NS_ASSERTION(NS_FAILED(res) || ((len==4) && (dstlen == 1)), 
       "some strange conversion result");
     // if we failed, we then just use the 0xfffd 
     // therefore, we ignore the res here. 
    if(NS_SUCCEEDED(res)) 
      return true;
  }
  return  false;
}
