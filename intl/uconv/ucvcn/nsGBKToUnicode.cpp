/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
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


static PRInt16 g_2BytesShiftTable[] = {
 0, u2BytesCharset,
 ShiftCell(0,0,0,0,0,0,0,0)
};
//------------------------------------------------------------
// nsGBKUnique2BytesToUnicode
//------------------------------------------------------------
class nsGBKUnique2BytesToUnicode : public nsTableDecoderSupport 
{
public:
  nsGBKUnique2BytesToUnicode();
  virtual ~nsGBKUnique2BytesToUnicode() 
    { };
protected:
  NS_IMETHOD GetMaxLength(const char* aSrc, PRInt32 aSrcLength,
                          PRInt32 * aDestLength)
  {
     *aDestLength = aSrcLength;
     return NS_OK;
  };
};

static PRUint16 g_utGBKUnique2Bytes[] = {
#include "gbkuniq2b.ut"
};
nsGBKUnique2BytesToUnicode::nsGBKUnique2BytesToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_2BytesShiftTable,
        (uMappingTable*) &g_utGBKUnique2Bytes) 
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
    { };
protected:
  NS_IMETHOD GetMaxLength(const char* aSrc, PRInt32 aSrcLength,
                          PRInt32 * aDestLength)
  {
     *aDestLength = aSrcLength;
     return NS_OK;
  };
};

static PRUint16 g_utGB18030Unique2Bytes[] = {
#include "gb18030uniq2b.ut"
};
nsGB18030Unique2BytesToUnicode::nsGB18030Unique2BytesToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_2BytesShiftTable,
        (uMappingTable*) &g_utGB18030Unique2Bytes) 
{
}

//------------------------------------------------------------
// nsGB18030Unique4BytesToUnicode
//------------------------------------------------------------
static PRInt16 g_GB18030_4BytesShiftTable[] = {
 0, u4BytesGB18030Charset,
 ShiftCell(0,0,0,0,0,0,0,0)
};
class nsGB18030Unique4BytesToUnicode : public nsTableDecoderSupport 
{
public:
  nsGB18030Unique4BytesToUnicode();
  virtual ~nsGB18030Unique4BytesToUnicode() 
    { };
protected:
  NS_IMETHOD GetMaxLength(const char* aSrc, PRInt32 aSrcLength,
                          PRInt32 * aDestLength)
  {
     *aDestLength = aSrcLength;
     return NS_OK;
  };
};

static PRUint16 g_utGB18030Unique4Bytes[] = {
#include "gb180304bytes.ut"
};
nsGB18030Unique4BytesToUnicode::nsGB18030Unique4BytesToUnicode() 
: nsTableDecoderSupport((uShiftTable*) &g_GB18030_4BytesShiftTable,
        (uMappingTable*) &g_utGB18030Unique4Bytes) 
{
}


//----------------------------------------------------------------------
// Class nsGBKToUnicode [implementation]

//----------------------------------------------------------------------
// Subclassing of nsTablesDecoderSupport class [implementation]

NS_IMETHODIMP nsGBKToUnicode::GetMaxLength(const char * aSrc, 
                                              PRInt32 aSrcLength, 
                                              PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

#define LEGAL_GBK_MULTIBYTE_FIRST_BYTE(c)  \
      (UINT8_IN_RANGE(0x81, (c), 0xFE))
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
        *aDest = UCS2_NO_MAPPING; 

        if (LEGAL_GBK_4BYTE_THIRD_BYTE(aSrc[2]) &&
            LEGAL_GBK_4BYTE_FORTH_BYTE(aSrc[3]))
        {
           // let's call the delegated 4 byte gb18030 converter to convert it
           if(! Try4BytesDecoder(aSrc, aDest))
           {
             *aDest = UCS2_NO_MAPPING;
           }
        }
        aSrc += 4;
        i+=3;
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
PRBool nsGBKToUnicode::TryExtensionDecoder(const char* aSrc, PRUnichar* aOut)
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
      return PR_TRUE;
  }
  return  PR_FALSE;
}
PRBool nsGBKToUnicode::Try4BytesDecoder(const char* aSrc, PRUnichar* aOut)
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
      return PR_TRUE;
  }
  return  PR_FALSE;
}
