/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "nsUnicodeToUEscape.h"
#include "nsUCvLatinSupport.h"

// XXX should we inherited from nsEncoderSupprt ? We don't want the buffer stuff there
class nsUnicodeToUEscape : public nsEncoderSupport 
{
public:

  /**
   * Class constructor.
   */
  nsUnicodeToUEscape() {};

  /**
   * Static class constructor.
   */
  static nsresult CreateInstance(nsISupports **aResult);
  
  NS_IMETHOD FillInfo(PRUint32* aInfo)
  {
    memset(aInfo, 0xFF, (0x10000L >> 3));
    return NS_OK;
  }

  NS_IMETHOD Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength);
  NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength) 
   {
      return NS_OK;
   }
  NS_IMETHOD Reset()
   {
      return NS_OK;
   }

protected:

  //--------------------------------------------------------------------
  // Subclassing of nsEncoderSupport class [declaration]

  NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength, 
      PRInt32 * aDestLength)
  {
    *aDestLength = 6*aSrcLength;
    return NS_OK;
  };

  NS_IMETHOD ConvertNoBuffNoErr(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength) 
  {
      NS_ASSERTION(PR_FALSE, "should never call this");
      return NS_ERROR_NOT_IMPLEMENTED;
  };

};



nsresult nsUnicodeToUEscape::CreateInstance(nsISupports ** aResult) 
{
  *aResult = (nsIUnicodeEncoder*) new nsUnicodeToUEscape();
  return (*aResult == NULL)? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

char hexarray[]="0123456789ABCDEF";
NS_IMETHODIMP nsUnicodeToUEscape::Convert(const PRUnichar * aSrc, PRInt32 * aSrcLength, 
      char * aDest, PRInt32 * aDestLength)
{
  const PRUnichar *src = aSrc;
  const PRUnichar *srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  const char * destEnd = aDest + *aDestLength;
 
  while(( src < srcEnd) && (dest<destEnd))
  {
     if(0x0000 == ( 0xFF80 & (*src)) )
     {
       if( PRUnichar('\\') == *src)
       {
           if((dest +2 ) >= destEnd)
             goto error;
           *dest++ = (char) '\\';
           *dest++ = (char) '\\';
       } else {
           *dest++ = (char) *src;
       }
     } else {
       if((dest +6 ) >= destEnd)
         goto error;
       *dest++ = (char) '\\';
       *dest++ = (char) 'u';
       *dest++ = (char) hexarray[0x0f & (*src >> 12)];
       *dest++ = (char) hexarray[0x0f & (*src >> 8)];
       *dest++ = (char) hexarray[0x0f & (*src >> 4)];
       *dest++ = (char) hexarray[0x0f & (*src)];
     }
     src++;
  }

  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return NS_OK;
error:
  *aSrcLength = src - aSrc;
  *aDestLength = dest - aDest;
  return NS_OK_UDEC_MOREOUTPUT;
}
nsresult NEW_UnicodeToUEscape(nsISupports **aResult)
{
   *aResult = (nsIUnicodeEncoder*)new nsUnicodeToUEscape();
   return (NULL == *aResult) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}    
