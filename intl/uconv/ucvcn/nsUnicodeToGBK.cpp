/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsICharRepresentable.h"
#include "nsUCvCnDll.h"
#include "gbku.h"
#include "uconvutil.h"
#include "nsUnicharUtils.h"

//-------------------------------------------------------------
// Global table initialization function defined in gbku.h
//-------------------------------------------------------------

//-----------------------------------------------------------------------
//  Private class used by nsUnicodeToGB18030 and nsUnicodeToGB18030Font0
//    nsUnicodeToGB18030Uniq2Bytes
//-----------------------------------------------------------------------
static const PRUint16 g_uf_gb18030_2bytes[] = {
#include "gb18030uniq2b.uf"
};
class nsUnicodeToGB18030Uniq2Bytes : public nsTableEncoderSupport
{
public: 
  nsUnicodeToGB18030Uniq2Bytes() 
    : nsTableEncoderSupport(u2BytesCharset,
                            (uMappingTable*) &g_uf_gb18030_2bytes, 2) {}
protected: 
};
//-----------------------------------------------------------------------
//  Private class used by nsUnicodeToGB18030
//    nsUnicodeTo4BytesGB18030
//-----------------------------------------------------------------------
static const PRUint16 g_uf_gb18030_4bytes[] = {
#include "gb180304bytes.uf"
};
class nsUnicodeTo4BytesGB18030 : public nsTableEncoderSupport
{
public: 
  nsUnicodeTo4BytesGB18030()
    : nsTableEncoderSupport(u4BytesGB18030Charset, 
                             (uMappingTable*) &g_uf_gb18030_4bytes, 4) {}
protected: 
};
//-----------------------------------------------------------------------
//  Private class used by nsUnicodeToGBK
//    nsUnicodeToGBKUniq2Bytes
//-----------------------------------------------------------------------
static const PRUint16 g_uf_gbk_2bytes[] = {
#include "gbkuniq2b.uf"
};
class nsUnicodeToGBKUniq2Bytes : public nsTableEncoderSupport
{
public: 
  nsUnicodeToGBKUniq2Bytes()
    : nsTableEncoderSupport(u2BytesCharset, 
                             (uMappingTable*) &g_uf_gbk_2bytes, 2) {}
protected: 
};
//-----------------------------------------------------------------------
//  nsUnicodeToGB18030
//-----------------------------------------------------------------------
void nsUnicodeToGB18030::CreateExtensionEncoder()
{
  mExtensionEncoder = new nsUnicodeToGB18030Uniq2Bytes();
}
void nsUnicodeToGB18030::Create4BytesEncoder()
{
  m4BytesEncoder = new nsUnicodeTo4BytesGB18030();
}

PRBool nsUnicodeToGB18030::EncodeSurrogate(
  PRUnichar aSurrogateHigh,
  PRUnichar aSurrogateLow,
  char* aOut)
{
  if( NS_IS_HIGH_SURROGATE(aSurrogateHigh) && 
      NS_IS_LOW_SURROGATE(aSurrogateLow) )
  {
    // notice that idx does not include the 0x10000 
    PRUint32 idx = ((aSurrogateHigh - (PRUnichar)0xD800) << 10 ) |
                   (aSurrogateLow - (PRUnichar) 0xDC00);

    unsigned char *out = (unsigned char*) aOut;
    // notice this is from 0x90 for supplment planes
    out[0] = (idx / (10*126*10)) + 0x90; 
    idx %= (10*126*10);
    out[1] = (idx / (10*126)) + 0x30;
    idx %= (10*126);
    out[2] = (idx / (10)) + 0x81;
    out[3] = (idx % 10) + 0x30;
    return PR_TRUE;
  } 
  return PR_FALSE; 
} 

#ifdef MOZ_EXTRA_X11CONVERTERS
//-----------------------------------------------------------------------
//  nsUnicodeToGB18030Font0
//-----------------------------------------------------------------------
NS_IMETHODIMP nsUnicodeToGB18030Font0::GetMaxLength(const PRUnichar * aSrc, 
                                              PRInt32 aSrcLength,
                                              PRInt32 * aDestLength)
{
  *aDestLength = 2 * aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH; // font encoding is only 2 bytes
}
void nsUnicodeToGB18030Font0::Create4BytesEncoder()
{
  m4BytesEncoder = nsnull;
}

NS_IMETHODIMP nsUnicodeToGB18030Font0::FillInfo(PRUint32 *aInfo)
{
  nsresult rv = nsUnicodeToGB18030::FillInfo(aInfo); // call the super class
  if(NS_SUCCEEDED(rv))
  {
    // mark the first 128 bits as 0. 4 x 32 bits = 128 bits
    aInfo[0] = aInfo[1] = aInfo[2] = aInfo[3] = 0;
  }
  return rv;
}

//-----------------------------------------------------------------------
//  nsUnicodeToGB18030Font1
//-----------------------------------------------------------------------
nsUnicodeToGB18030Font1::nsUnicodeToGB18030Font1()
  : nsTableEncoderSupport(u2BytesCharset,
                             (uMappingTable*) &g_uf_gb18030_4bytes, 4)
{

}

NS_IMETHODIMP nsUnicodeToGB18030Font1::FillInfo(PRUint32 *aInfo)
{
  nsresult res = nsTableEncoderSupport::FillInfo(aInfo);
  PRUint32 i;

  for(i = (0x0000 >> 5); i<(0x0600 >> 5); i++)
    aInfo[i] = 0;

  // handle Arabic script
  for(i = (0x0600 >> 5); i<(0x06e0 >> 5); i++)
    aInfo[i] = 0;
  SET_REPRESENTABLE(aInfo, 0x060c);
  SET_REPRESENTABLE(aInfo, 0x061b);
  SET_REPRESENTABLE(aInfo, 0x061f);
  for(i = 0x0626;i <=0x0628;i++)
    SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0x062a);
  for(i = 0x062c;i <=0x062f;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0x0631;i <=0x0634;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0x0639;i <=0x063a;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0x0640;i <=0x064a;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0x0674;i <=0x0678;i++)
    SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0x067e);
  SET_REPRESENTABLE(aInfo, 0x0686);
  SET_REPRESENTABLE(aInfo, 0x0698);
  SET_REPRESENTABLE(aInfo, 0x06a9);
  SET_REPRESENTABLE(aInfo, 0x06ad);
  SET_REPRESENTABLE(aInfo, 0x06af);
  SET_REPRESENTABLE(aInfo, 0x06be);
  for(i = 0x06c5;i <=0x06c9;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0x06cb;i <=0x06cc;i++)
    SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0x06d0);
  SET_REPRESENTABLE(aInfo, 0x06d5);
  // end of Arabic script

  for(i = (0x06e0 >> 5); i<(0x0f00 >> 5); i++)
    aInfo[i] = 0;

  // handle Tibetan script
  CLEAR_REPRESENTABLE(aInfo, 0x0f48);
  for(i = 0x0f6b;i <0x0f71;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  for(i = 0x0f8c;i <0x0f90;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  CLEAR_REPRESENTABLE(aInfo, 0x0f98);
  CLEAR_REPRESENTABLE(aInfo, 0x0fbd);
  CLEAR_REPRESENTABLE(aInfo, 0x0fcd);
  CLEAR_REPRESENTABLE(aInfo, 0x0fce);
  for(i = 0x0fd0;i <0x0fe0;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  // end of Tibetan script

  for(i = (0x0fe0 >> 5); i<(0x1800 >> 5); i++)
    aInfo[i] = 0;

  // handle Mongolian script
  CLEAR_REPRESENTABLE(aInfo, 0x180f);
  for(i = 0x181a;i <0x1820;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  for(i = 0x1878;i <0x1880;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  for(i = 0x18aa;i <0x18c0;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  // end of Mongolian script

  for(i = (0x18c0 >> 5); i<(0x3400 >> 5); i++)
    aInfo[i] = 0;

  // handle Han Ideograph
  // remove above U+4db5
  //   remove U+4db6 to U+4dbf one by one
  for(i = 0x4db6;i <0x4dc0;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  // end of Han Ideograph

  //   remove  U+4dc0 to 0xa000
  for(i = (0x4dc0 >> 5);i < (0xa000>>5) ; i++)
    aInfo[i] = 0;

  // handle Yi script
  for(i = 0xa48d;i <0xa490;i++)
    CLEAR_REPRESENTABLE(aInfo, i);

  CLEAR_REPRESENTABLE(aInfo, 0xa4a2);
  CLEAR_REPRESENTABLE(aInfo, 0xa4a3);
  CLEAR_REPRESENTABLE(aInfo, 0xa4b4);
  CLEAR_REPRESENTABLE(aInfo, 0xa4c1);
  CLEAR_REPRESENTABLE(aInfo, 0xa4c5);

  for(i = 0xa4c7;i <0xa4e0;i++)
    CLEAR_REPRESENTABLE(aInfo, i);
  // end of Yi script

  for(i = (0xa4e0 >> 5);i < (0xfb00>>5) ; i++)
    aInfo[i] = 0;

  // handle Arabic script
  for(i = (0xfb00 >> 5);i < (0xfc00>>5) ; i++)
    aInfo[i] = 0;
  for(i = 0xfb56;i <=0xfb59;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0xfb7a;i <=0xfb95;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0xfbaa;i <=0xfbad;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0xfbd3;i <=0xfbff;i++)
    SET_REPRESENTABLE(aInfo, i);
  // end of Arabic script

  for(i = (0xfc00 >> 5);i < (0xfe80>>5) ; i++)
    aInfo[i] = 0;

  // handle Arabic script
  for(i = (0xfe80 >> 5);i < (0x10000>>5) ; i++)
    aInfo[i] = 0;
  for(i = 0xfe89;i <=0xfe98;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0xfe9d;i <=0xfeaa;i++)
    SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0xfead);
  for(i = 0xfeae;i <=0xfeb8;i++)
    SET_REPRESENTABLE(aInfo, i);
  for(i = 0xfec9;i <=0xfef4;i++)
    SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0xfefb);
  SET_REPRESENTABLE(aInfo, 0xfefc);
  // end of Arabic script
  return res;
}
#endif
//----------------------------------------------------------------------
// Class nsUnicodeToGBK [implementation]

nsUnicodeToGBK::nsUnicodeToGBK(PRUint32 aMaxLength) :
  nsEncoderSupport(aMaxLength)
{
  mExtensionEncoder = nsnull;
  m4BytesEncoder = nsnull;
  mUtil.InitToGBKTable();
  mSurrogateHigh = 0;
}
void nsUnicodeToGBK::CreateExtensionEncoder()
{
  mExtensionEncoder = new nsUnicodeToGBKUniq2Bytes();
}
void nsUnicodeToGBK::Create4BytesEncoder()
{
  m4BytesEncoder = nsnull;
}
PRBool nsUnicodeToGBK::TryExtensionEncoder(
  PRUnichar aChar,
  char* aOut,
  PRInt32 *aOutLen
)
{
  if( NS_IS_HIGH_SURROGATE(aChar) || 
      NS_IS_LOW_SURROGATE(aChar) )
  {
    // performance tune for surrogate characters
    return PR_FALSE;
  }
  if(! mExtensionEncoder )
    CreateExtensionEncoder();
  if(mExtensionEncoder) 
  {
    PRInt32 len = 1;
    nsresult res = NS_OK;
    res = mExtensionEncoder->Convert(&aChar, &len, aOut, aOutLen);
    if(NS_SUCCEEDED(res) && (*aOutLen > 0))
      return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool nsUnicodeToGBK::Try4BytesEncoder(
  PRUnichar aChar,
  char* aOut,
  PRInt32 *aOutLen
)
{
  if( NS_IS_HIGH_SURROGATE(aChar) || 
      NS_IS_LOW_SURROGATE(aChar) )
  {
    // performance tune for surrogate characters
    return PR_FALSE;
  }
  if(! m4BytesEncoder )
    Create4BytesEncoder();
  if(m4BytesEncoder) 
  {
    PRInt32 len = 1;
    nsresult res = NS_OK;
    res = m4BytesEncoder->Convert(&aChar, &len, aOut, aOutLen);
    NS_ASSERTION(NS_FAILED(res) || ((1 == len) && (4 == *aOutLen)),
      "unexpect conversion length");
    if(NS_SUCCEEDED(res) && (*aOutLen > 0))
      return PR_TRUE;
  }
  return PR_FALSE;
}
PRBool nsUnicodeToGBK::EncodeSurrogate(
  PRUnichar aSurrogateHigh,
  PRUnichar aSurrogateLow,
  char* aOut)
{
  return PR_FALSE; // GBK cannot encode Surrogate, let the subclass encode it.
} 

NS_IMETHODIMP nsUnicodeToGBK::ConvertNoBuff(
  const PRUnichar * aSrc, 
  PRInt32 * aSrcLength, 
  char * aDest, 
  PRInt32 * aDestLength)
{
  PRInt32 iSrcLength = 0;
  PRInt32 iDestLength = 0;
  PRUnichar unicode;
  nsresult res = NS_OK;
  while (iSrcLength < *aSrcLength )
  {
    unicode = *aSrc;
    //if unicode's hi byte has something, it is not ASCII, must be a GB
    if(IS_ASCII(unicode))
    {
      // this is an ASCII
      *aDest = CAST_UNICHAR_TO_CHAR(*aSrc);
      aDest++; // increment 1 byte
      iDestLength +=1;
    } else {
      char byte1, byte2;
      if(mUtil.UnicodeToGBKChar( unicode, PR_FALSE, &byte1, &byte2))
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
        PRInt32 aOutLen = 2;
        // make sure we still have 2 bytes for output first
        if(iDestLength+2 > *aDestLength)
        {
          res = NS_OK_UENC_MOREOUTPUT;
          break;
        }
        // we cannot map in the common mapping. Let's try to
        // call the delegated 2 byte converter for the gbk or gb18030
        // unique 2 byte mapping
        if(TryExtensionEncoder(unicode, aDest, &aOutLen))
        {
          iDestLength += aOutLen;
          aDest += aOutLen;
        } else {
          // make sure we still have 4 bytes for output first
          if(iDestLength+4 > *aDestLength)
          {
            res = NS_OK_UENC_MOREOUTPUT;
            break;
          }
          // we still cannot map. Let's try to
          // call the delegated GB18030 4 byte converter 
          aOutLen = 4;
          if( NS_IS_HIGH_SURROGATE(unicode) )
          {
            if((iSrcLength+1) < *aSrcLength ) {
              if(EncodeSurrogate(aSrc[0],aSrc[1], aDest)) {
                // since we got a surrogate pair, we need to increment src.
                iSrcLength++ ; 
                aSrc++;
                iDestLength += aOutLen;
                aDest += aOutLen;
              } else {
                // only get a high surrogate, but not a low surrogate
                res = NS_ERROR_UENC_NOMAPPING;
                iSrcLength++;   // include length of the unmapped character
                break;
              }
            } else {
              mSurrogateHigh = aSrc[0];
              break; // this will go to afterwhileloop
            }
          } else {
            if( NS_IS_LOW_SURROGATE(unicode) )
            {
              if(NS_IS_HIGH_SURROGATE(mSurrogateHigh)) {
                if(EncodeSurrogate(mSurrogateHigh, aSrc[0], aDest)) {
                  iDestLength += aOutLen;
                  aDest += aOutLen;
                } else {
                  // only get a high surrogate, but not a low surrogate
                  res = NS_ERROR_UENC_NOMAPPING;
                  iSrcLength++;   // include length of the unmapped character
                  break;
                }
              } else {
                // only get a low surrogate, but not a low surrogate
                res = NS_ERROR_UENC_NOMAPPING;
                iSrcLength++;   // include length of the unmapped character
                break;
              }
            } else {
              if(Try4BytesEncoder(unicode, aDest, &aOutLen))
              {
                NS_ASSERTION((aOutLen == 4), "we should always generate 4 bytes here");
                iDestLength += aOutLen;
                aDest += aOutLen;
              } else {
                res = NS_ERROR_UENC_NOMAPPING;
                iSrcLength++;   // include length of the unmapped character
                break;
              }
            }
          }
        }
      } 
    }
    iSrcLength++ ; // Each unicode char just count as one in PRUnichar string;  	  
    mSurrogateHigh = 0;
    aSrc++;
    if ( iDestLength >= (*aDestLength) && (iSrcLength < *aSrcLength) )
    {
      res = NS_OK_UENC_MOREOUTPUT;
      break;
    }
  }
//afterwhileloop:
  *aDestLength = iDestLength;
  *aSrcLength = iSrcLength;
  return res;
}

//----------------------------------------------------------------------
// Subclassing of nsTableEncoderSupport class [implementation]

NS_IMETHODIMP nsUnicodeToGBK::FillInfo(PRUint32 *aInfo)
{
  mUtil.FillInfo(aInfo, 0x81, 0xFE, 0x40, 0xFE);
  if(! mExtensionEncoder )
    CreateExtensionEncoder();
  if(mExtensionEncoder) 
  {
    nsCOMPtr<nsICharRepresentable> aRep = do_QueryInterface(mExtensionEncoder);
    aRep->FillInfo(aInfo);
  }
  
  if(! m4BytesEncoder )
    Create4BytesEncoder();
  if(m4BytesEncoder) 
  {
    nsCOMPtr<nsICharRepresentable> aRep = do_QueryInterface(m4BytesEncoder);
    aRep->FillInfo(aInfo);
  }

  //GBK font lib also have single byte ASCII characters, set them here
  for (PRUint16 SrcUnicode = 0x0000; SrcUnicode <= 0x007F; SrcUnicode++)
    SET_REPRESENTABLE(aInfo, SrcUnicode);
  SET_REPRESENTABLE(aInfo, 0x20ac); // euro
  return NS_OK;
}
