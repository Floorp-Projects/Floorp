/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsUnicodeToUCS2BE.h"
#include <string.h>

inline static void SwapBytes(char *aDest, const PRUnichar* aSrc, PRInt32 aLen);

NS_IMETHODIMP nsUnicodeToUTF16BE::Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength)
{
  PRInt32 srcInLen = *aSrcLength;
  PRInt32 destInLen = *aDestLength;
  PRInt32 srcOutLen = 0;
  PRInt32 destOutLen = 0;
  PRInt32 copyCharLen;
  PRUnichar *p = (PRUnichar*)aDest;
 
  // Handle BOM if necessary 
  if(0!=mBOM)
  {
     if(destInLen <2)
        goto needmoreoutput;
  
     *p++ = mBOM;
     mBOM = 0;
     destOutLen +=2;
  }
  // find out the length of copy 

  copyCharLen = srcInLen;
  if(copyCharLen > (destInLen - destOutLen) / 2) {
     copyCharLen = (destInLen - destOutLen) / 2;
  }

  // copy the data  by swaping 
  CopyData((char*)p , aSrc, copyCharLen );

  srcOutLen += copyCharLen;
  destOutLen += copyCharLen * 2;
  if(copyCharLen < srcInLen)
      goto needmoreoutput;
  
  *aSrcLength = srcOutLen;
  *aDestLength = destOutLen;
  return NS_OK;

needmoreoutput:
  *aSrcLength = srcOutLen;
  *aDestLength = destOutLen;
  return NS_OK_UENC_MOREOUTPUT;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength)
{
  if(0 != mBOM)
    *aDestLength = 2*(aSrcLength+1);
  else 
    *aDestLength = 2*aSrcLength;
  return NS_OK_UENC_EXACTLENGTH;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::Finish(char * aDest, PRInt32 * aDestLength)
{
  if(0 != mBOM)
  {
     if(*aDestLength >= 2)
     {
        *((PRUnichar*)aDest)= mBOM;
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

NS_IMETHODIMP nsUnicodeToUTF16BE::SetOutputErrorBehavior(PRInt32 aBehavior, 
      nsIUnicharEncoder * aEncoder, PRUnichar aChar)
{
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::FillInfo(PRUint32 *aInfo)
{
  ::memset(aInfo, 0xFF, (0x10000L >> 3));
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16BE::CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  )
{
#ifdef IS_BIG_ENDIAN
  //UnicodeToUTF16SameEndian
  ::memcpy(aDest, (void*) aSrc, aLen * 2);
#elif defined(IS_LITTLE_ENDIAN)
  //UnicodeToUTF16DiffEndian
  SwapBytes(aDest, aSrc, aLen);
#else
  #error "Unknown endianness"
#endif
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToUTF16LE::CopyData(char* aDest, const PRUnichar* aSrc, PRInt32 aLen  )
{
#ifdef IS_LITTLE_ENDIAN
  //UnicodeToUTF16SameEndian
  ::memcpy(aDest, (void*) aSrc, aLen * 2);
#elif defined(IS_BIG_ENDIAN)
  //UnicodeToUTF16DiffEndian
  SwapBytes(aDest, aSrc, aLen);
#else
  #error "Unknown endianness"
#endif
  return NS_OK;
}

inline void SwapBytes(char *aDest, const PRUnichar* aSrc, PRInt32 aLen)
{
  PRUnichar *p = (PRUnichar*) aDest;
  // copy the data  by swaping 
  for(PRInt32 i = 0; i < aLen; i++)
  {
    PRUnichar aChar = *aSrc++;
    *p++ = (0x00FF & (aChar >> 8)) | (0xFF00 & (aChar << 8));
  }
}

