/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsUnicodeToUEscape.h"
#include "nsUCvLatinSupport.h"
#include <string.h>

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
           if(( PRUnichar('n') == *(1+src))||
              ( PRUnichar('r') == *(1+src))||
              ( PRUnichar('t') == *(1+src))) 
           {
             *dest++ = (char) *src;
           } else {
             if((dest +2 ) >= destEnd)
               goto error;
             *dest++ = (char) '\\';
             *dest++ = (char) '\\';
           }
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
