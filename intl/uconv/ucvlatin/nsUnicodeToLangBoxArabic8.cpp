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


#include "nsUCConstructors.h"
#include "nsUnicodeToLangBoxArabic8.h"

#include "nsISupports.h"

static const unsigned char uni2lbox [] = 
 {
  0xC1,   /* FE80 */
  0xC2 , 
  0xC2 , 
  0xC3 , 
  0xC3 , 
  0xC4 , 
  0xC4 , 
  0xC5 , 
  0xC5 , 
  0x9F , 
  0xC6 , 
  0xC0 , 
  0xC0 , 
  0xC7 ,
  0xC7 , 
  0xC8 , 
  0xC8 , /* FE90 */
  0xEB , 
  0xEB , 
  0xC9 , 
  0x8E , /* START TAA MARBUTA FINAL */
  0xCA , 
  0xCA , 
  0xEC , 
  0xEC , 
  0xCB , 
  0xCB , 
  0xED , 
  0xED , 
  0xCC , 
  0xCC , 
  0xEE , 
  0xEE , 
  0xCD , 
  0xCD , 
  0xEF , 
  0xEF , 
  0xCE ,
  0xCE , 
  0xF0 , 
  0xF0 , 
  0xCF , 
  0xCF , 
  0xD0 , 
  0xD0 , 
  0xD1 ,
  0xD1 , 
  0xD2 , 
  0xD2 , 
  0xD3 , 
  0x8F , 
  0xF1 , 
  0xF1 , 
  0xD4 , 
  0x90 , 
  0xF2 , 
  0xF2 , 
  0xD5 , 
  0x91 , 
  0xF3 , 
  0xF3 , 
  0xD6 ,
  0x92 , 
  0xF4 , 
  0xF4 , 
  0xD7 ,
  0xD7 , 
  0x93 , 
  0x93 , 
  0xD8 , 
  0xD8 , 
  0x94 , 
  0x94 , 
  0xD9 , 
  0x96 , 
  0xF5 , 
  0x95 , 
  0xDA ,
  0x98 , 
  0xF6 , 
  0x97 , 
  0xE1 , 
  0xE1 , 
  0xF7 , 
  0x99 , 
  0xE2 , 
  0xE2 , 
  0xF8 , 
  0x9A , 
  0xE3 , 
  0xE3 , 
  0xF9 , 
  0x9B , 
  0xE4 , 
  0xE4 , 
  0xFA , 
  0xFA , 
  0xE5 , 
  0xE5 , 
  0xFB , 
  0xFB , 
  0xE6 , 
  0xE6 , 
  0xFC , 
  0xFC , 
  0xE7 , 
  0x9D , 
  0xFD , 
  0x9C , 
  0xE8 , 
  0xE8 , 
  0x8D , 
  0xE9 , 
  0x9E , 
  0xEA , 
  0xFE , 
  0xFE  /* FEF4 */
 };

/**
 * The following are the Unicode Lam-Alef ligatures:
 * 
 * FEF5;ARABIC LIGATURE LAM WITH ALEF WITH MADDA ABOVE ISOLATED FORM
 * FEF6;ARABIC LIGATURE LAM WITH ALEF WITH MADDA ABOVE FINAL FORM
 * FEF7;ARABIC LIGATURE LAM WITH ALEF WITH HAMZA ABOVE ISOLATED FORM
 * FEF8;ARABIC LIGATURE LAM WITH ALEF WITH HAMZA ABOVE FINAL FORM
 * FEF9;ARABIC LIGATURE LAM WITH ALEF WITH HAMZA BELOW ISOLATED FORM
 * FEFA;ARABIC LIGATURE LAM WITH ALEF WITH HAMZA BELOW FINAL FORM
 * FEFB;ARABIC LIGATURE LAM WITH ALEF ISOLATED FORM
 * FEFC;ARABIC LIGATURE LAM WITH ALEF FINAL FORM
 *
 * In the Langbox 8x encoding, they have to be split into separate glyphs:
 *
 * 0xA1 ARABIC LIGATURE ALEF OF LAM ALEF
 * 0xA2 ARABIC LIGATURE MADDA ON ALEF OF LAM ALEF
 * 0xA3 ARABIC LIGATURE HAMZA ON ALEF OF LAM ALEF
 * 0xA4 ARABIC LIGATURE HAMZA UNDER ALEF OF LAM ALEF
 * 0xA5 ARABIC LIGATURE LAM OF LAM ALEF INITIAL FORM
 * 0xA6 ARABIC LIGATURE LAM OF LAM ALEF MEDIAL FORM
 */

static const unsigned char lboxAlefs[] =
{
  0xA2,
  0xA3,
  0xA4,
  0xA1
};

static const unsigned char lboxLams[] = 
{
  0xA5,
  0xA6
};

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::Convert(
      const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength)
{
   char* dest = aDest;
   PRInt32 inlen = 0;

   if(*aSrc<=0) {
      *aDestLength = 0;
      return NS_OK;
   }

   while (inlen < *aSrcLength) {
     PRUnichar aChar = aSrc[inlen];

     if((aChar >= 0x0660) && (aChar <=0x0669)) { /* Hindu Numerals */
       *dest++ = (char)(aChar - 0x0660 + 0xB0);
     } else if ((aChar >= 0x064B) && (aChar <= 0x0652)) {
       *dest++ = (char)(aChar - 0x64B + 0xA8);
     } else if(0x060C == aChar) {
       *dest++ = (char)0xBA;
     } else if(0x061B == aChar) {
       *dest++ = (char)0xBB;
     } else if(0x061F == aChar) {
       *dest++ = (char)0xBF;
     } else if(0x0640 == aChar) {
       *dest++ = (char)0xE0;
     } else if ((aChar>=0xFE80) && (aChar <= 0xFEF4)) {
       *dest++ = uni2lbox[aChar-0xFE80];
     } else if ((aChar >=0xFEF5) && (aChar <= 0xFEFC)) {
       PRUint8 lamAlefType = aChar - 0xFEF5;       // first map to 0-7 range,
       PRUint8 alefType = (lamAlefType & 6) >> 1;  // then the high 2 bits give us the type of alef
       PRUint8 lamType = lamAlefType & 1;          // and the low bits give us the type of lam

       *dest++ = lboxAlefs[alefType];
       *dest++ = lboxLams[lamType];
     } else if ((aChar >=0x0001) && (aChar <= 0x007F)) {
       *dest++ = (char) (aChar & 0x7F);
     } else {
       // do nothing
     }
     inlen++;
   }

    *aDestLength = dest - aDest;
    return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::GetMaxLength(
const PRUnichar * aSrc, PRInt32 aSrcLength, 
                           PRInt32 * aDestLength) 
{
  *aDestLength = 2*aSrcLength;
  return NS_OK;
};

NS_IMETHODIMP nsUnicodeToLangBoxArabic8::Finish(
      char * aDest, PRInt32 * aDestLength)
{
   *aDestLength=0;
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

   SET_REPRESENTABLE(aInfo, 0x060c);      
   SET_REPRESENTABLE(aInfo, 0x061b);      
   SET_REPRESENTABLE(aInfo, 0x061f);      
   for(i=0x0621;i<=0x063a;i++)
      SET_REPRESENTABLE(aInfo, i);      
   for(i=0x0640;i<=0x0652;i++)
      SET_REPRESENTABLE(aInfo, i);      
   for(i=0x0660;i<=0x0669;i++)
      SET_REPRESENTABLE(aInfo, i);      

   // Arabic Pres Form-B
   for(i=0xFE80; i < 0xFEFD;i++)
     SET_REPRESENTABLE(aInfo, i);

   // ASCII range
   for(i=0x0000; i < 0x007f;i++)
     SET_REPRESENTABLE(aInfo, i);

   return NS_OK;
}
