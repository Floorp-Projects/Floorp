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

bool nsUnicodeToGB18030::EncodeSurrogate(
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
bool nsUnicodeToGBK::TryExtensionEncoder(
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

bool nsUnicodeToGBK::Try4BytesEncoder(
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
bool nsUnicodeToGBK::EncodeSurrogate(
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
