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


#include "nsUnicodeToLangBoxArabic8.h"

#include "nsUCvLatinSupport.h"

//----------------------------------------------------------------------
// Class nsUnicodeToLangBoxArabic8 [declaration]

class nsUnicodeToLangBoxArabic8 : public nsBasicEncoder
{

public:

  /**
   * Class constructor.
   */
  nsUnicodeToLangBoxArabic8() {};
  virtual ~nsUnicodeToLangBoxArabic8() {};

  NS_IMETHOD Convert(
      const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD Finish(
      char * aDest, PRInt32 * aDestLength);

  NS_IMETHOD GetMaxLength(
      const PRUnichar * aSrc, PRInt32 aSrcLength,
      PRInt32 * aDestLength);

  NS_IMETHOD Reset();

  NS_IMETHOD SetOutputErrorBehavior(
      PRInt32 aBehavior,
      nsIUnicharEncoder * aEncoder, PRUnichar aChar);

  NS_IMETHOD FillInfo(PRUint32* aInfo);
};


enum {
 eZWNJ = 0x200C,
 eZWJ = 0x200D,
 eLRM = 0x200E,
 eRLM = 0x200F,
 eLRE = 0x202A,
 eRLE = 0x202B,
 ePDF = 0x202C,
 eLRO = 0x202D,
 eRLO = 0x202E
};
// I currently only put in hack to show what this function will do in the
// future after implement by LangBox
// Done:
// 1. Logical order to Visual order
// 2. Arabic char to Arabic Isolated form
// 
// To Do:
// 1. Arabic char to presentation form (shaping)
// 2. Lam Alef ligature
// 
NS_IMETHODIMP nsUnicodeToLangBoxArabic8::Convert(
      const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength)
{
   unsigned char* dest = (unsigned char*)aDest;
   PRInt32 outlen = 0;
   if(*aSrc<=0) {
      *aDestLength = 0;
      return NS_OK;
   }
   const PRUnichar *in = aSrc + *aSrcLength-1;
   // scanning from logical end to start, which mean visually left to right
   while((in >= aSrc) && (outlen < *aDestLength))
   {
      // begin of hack
      // replace the following code with real arabic shaping
      PRUnichar aChar = *in;
      if((0x0600 >= aChar) && (aChar >=0x066F)) {
        
        if((0x061F >= aChar) && (aChar >=0x064A)) {
            *dest++ = (unsigned char)(aChar - 0x061F + 0xBF);
            outlen++;
        } else if((0x064B >= aChar) && (aChar >=0x0652)) {
            *dest++ = (unsigned char)(aChar - 0x064B + 0xA8);
            outlen++;
        } else if((0x0660 >= aChar) && (aChar >=0x0669)) {
            *dest++ = (unsigned char)(aChar - 0x0660 + 0xB0);
            outlen++;
        } else if(0x060C == aChar) {
            *dest++ = (unsigned char)0xBA;
            outlen++;
        } else if(0x061B == aChar) {
            *dest++ = (unsigned char)0xBB;
            outlen++;
        } else { 
         // do nothing
        }
      } else  {
         // do nothing
      }
      in--;
      // end of hack
   }
   *aDestLength = outlen;
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::Finish(
      char * aDest, PRInt32 * aDestLength)
{
   *aDestLength=0;
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::GetMaxLength(
      const PRUnichar * aSrc, PRInt32 aSrcLength,
      PRInt32 * aDestLength)
{
   *aDestLength = aSrcLength;
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::Reset()
{
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::SetOutputErrorBehavior(
      PRInt32 aBehavior,
      nsIUnicharEncoder * aEncoder, PRUnichar aChar)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::FillInfo(PRUint32* aInfo)
{
   PRUnichar i;
   // I think we should also add Presentation Form B here 
   // Bi-di format char
   for(i=eZWNJ;i<=eRLM;i++)
      SET_REPRESENTABLE(aInfo, i);      
   for(i=eLRE;i<=eRLO;i++)
      SET_REPRESENTABLE(aInfo, i);      

   SET_REPRESENTABLE(aInfo, 0x060c);      
   SET_REPRESENTABLE(aInfo, 0x061b);      
   SET_REPRESENTABLE(aInfo, 0x061f);      
   for(i=0x0621;i<=0x063a;i++)
      SET_REPRESENTABLE(aInfo, i);      
   for(i=0x0640;i<=0x0652;i++)
      SET_REPRESENTABLE(aInfo, i);      
   for(i=0x0660;i<=0x0669;i++)
      SET_REPRESENTABLE(aInfo, i);      
   return NS_OK;
}
nsresult NEW_UnicodeToLangBoxArabic8(nsISupports **aResult)
{
  nsIUnicodeEncoder *p = new nsUnicodeToLangBoxArabic8();
  if(p) {
   *aResult = p;
   return NS_OK;
  }
  return NS_ERROR_OUT_OF_MEMORY; 
}
