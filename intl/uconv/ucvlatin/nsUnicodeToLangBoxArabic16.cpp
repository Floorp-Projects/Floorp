/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Montagu <smontagu@netscape.com>
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


#include "nsUCConstructors.h"
#include "nsUnicodeToLangBoxArabic16.h"

#include "nsISupports.h"

// This table is based on http://www.langbox.com/arabic/FontSet_ISO8859-6-16.html
// Codepoints not in Unicode are mapped to 0x3F
static const unsigned char uni2lbox [] =
{
  0x6B, // U+FE70 ARABIC FATHATAN ISOLATED FORM
  0x90, // U+FE71 ARABIC FATHATAN ON TATWEEL
  0x6C, // U+FE72 ARABIC DAMMATAN ISOLATED FORM
  0x3F, // U+FE73
  0x6D, // U+FE74 ARABIC KASRATAN ISOLATED FORM
  0x3F, // U+FE75
  0x6E, // U+FE76 ARABIC FATHA ISOLATED FORM
  0x93, // U+FE77 ARABIC FATHA ON TATWEEL
  0x6F, // U+FE78 ARABIC DAMMA ISOLATED FORM
  0x94, // U+FE79 ARABIC DAMMA ON TATWEEL
  0x70, // U+FE7A ARABIC KASRA ISOLATED FORM
  0x95, // U+FE7B ARABIC KASRA ON TATWEEL
  0x71, // U+FE7C ARABIC SHADDA ISOLATED FORM
  0x97, // U+FE7D ARABIC SHADDA ON TATWEEL
  0x72, // U+FE7E ARABIC SUKUN ISOLATED FORM
  0x96, // U+FE7F ARABIC SUKUN ON TATWEEL
  0x41, // U+FE80 ARABIC HAMZA ISOLATED FORM
  0x42, // U+FE81 ARABIC LIGATURE MADDA ON ALEF ISOLATED FORM
  0xA1, // U+FE82 ARABIC LIGATURE MADDA ON ALEF FINAL FORM
  0x43, // U+FE83 ARABIC LIGATURE HAMZA ON ALEF ISOLATED FORM
  0xA2, // U+FE84 ARABIC LIGATURE HAMZA ON ALEF FINAL FORM
  0x44, // U+FE85 ARABIC LIGATURE HAMZA ON WAW ISOLATED FORM
  0xA3, // U+FE86 ARABIC LIGATURE HAMZA ON WAW FINAL FORM
  0x45, // U+FE87 ARABIC LIGATURE HAMZA UNDER ALEF ISOLATED FORM
  0xA4, // U+FE88 ARABIC LIGATURE HAMZA UNDER ALEF FINAL FORM
  0x46, // U+FE89 ARABIC LIGATURE HAMZA ON YEH ISOLATED FORM
  0xF9, // U+FE8A ARABIC LIGATURE HAMZA ON YA FINAL FORM
  0xF8, // U+FE8B ARABIC LIGATURE HAMZA ON YA INITIAL FORM
  0xA0, // U+FE8C ARABIC LIGATURE HAMZA ON YA MEDIAL FORM
  0x47, // U+FE8D ARABIC ALEF ISOLATED FORM
  0xA5, // U+FE8E ARABIC ALEF FINAL FORM
  0x48, // U+FE8F ARABIC BAA ISOLATED FORM
  0xAE, // U+FE90 ARABIC BAA FINAL FORM
  0xAC, // U+FE91 ARABIC BAA INITTIAL FORM
  0xAD, // U+FE92 ARABIC BAA MEDIAL FORM
  0x49, // U+FE93 ARABIC TAA MARBUTA ISOLATED FORM
  0xB1, // U+FE94 ARABIC TAA MARBUTA FINAL FORM
  0x4A, // U+FE95 ARABIC TAA ISOLATED FORM
  0xB4, // U+FE96 ARABIC TAA FINAL FORM
  0xB2, // U+FE97 ARABIC TAA INITIAL FORM
  0xB3, // U+FE98 ARABIC TAA MEDIAL FORM
  0x4B, // U+FE99 ARABIC THAA ISOLATED FORM
  0xB7, // U+FE9A ARABIC THAA FINAL FORM
  0xB5, // U+FE9B ARABIC THAA INITIAL FORM
  0xB6, // U+FE9C ARABIC THAA MEDIAL FORM
  0x4C, // U+FE9D ARABIC JEEM ISOLATED FORM
  0xBA, // U+FE9E ARABIC JEEM FINAL FORM
  0xB8, // U+FE9F ARABIC JEEM INITIAL FORM
  0xB9, // U+FEA0 ARABIC JEEM MEDIAL FORM
  0x4D, // U+FEA1 ARABIC HAA ISOLATED FORM
  0xBD, // U+FEA2 ARABIC HAA FINAL FORM
  0xBB, // U+FEA3 ARABIC HAA INITIAL FORM
  0xBC, // U+FEA4 ARABIC HAA MEDIAL FORM
  0x4E, // U+FEA5 ARABIC KHAA ISOLATED FORM
  0xC0, // U+FEA6 ARABIC KHAA FINAL FORM
  0xBE, // U+FEA7 ARABIC KHAA INITIAL FORM
  0xBF, // U+FEA8 ARABIC KHAA MEDIAL FORM
  0x4F, // U+FEA9 ARABIC DAL ISOLATED FORM
  0xA6, // U+FEAA ARABIC DAL FINAL FORM
  0x50, // U+FEAB ARABIC THAL ISOLATED FORM
  0xA7, // U+FEAC ARABIC THAL FINAL FORM
  0x51, // U+FEAD ARABIC RA ISOLATED FORM
  0xA8, // U+FEAE ARABIC RA FINAL FORM
  0x52, // U+FEAF ARABIC ZAIN ISOLATED FORM
  0xA9, // U+FEB0 ARABIC ZAIN FINAL FORM
  0x53, // U+FEB1 ARABIC SEEN ISOLATED FORM
  0xC3, // U+FEB2 ARABIC SEEN FINAL FORM
  0xC1, // U+FEB3 ARABIC SEEN INITIAL FORM
  0xC2, // U+FEB4 ARABIC SEEN IMEDIAL FORM
  0x54, // U+FEB5 ARABIC SHEEN ISOLATED FORM
  0xC6, // U+FEB6 ARABIC SHEEN FINAL FORM
  0xC4, // U+FEB7 ARABIC SHEEN INITIAL FORM
  0xC5, // U+FEB8 ARABIC SHEEN MEDIAL FORM
  0x55, // U+FEB9 ARABIC SAD ISOLATED FORM
  0xC9, // U+FEBA ARABIC SAD FINAL FORM
  0xC7, // U+FEBB ARABIC SAD INITIAL FORM
  0xC8, // U+FEBC ARABIC SAD MEDIAL FORM
  0x56, // U+FEBD ARABIC DAD ISOLATED FORM
  0xCC, // U+FEBE ARABIC DAD FINAL FORM
  0xCA, // U+FEBF ARABIC DAD INITIAL FORM
  0xCB, // U+FEC0 ARABIC DAD MEDIAL FORM
  0x57, // U+FEC1 ARABIC TAH ISOLATED FORM
  0xCF, // U+FEC2 ARABIC TAH FINAL FORM
  0xCD, // U+FEC3 ARABIC TAH INITIAL FORM
  0xCE, // U+FEC4 ARABIC TAH MEDIAL FORM
  0x58, // U+FEC5 ARABIC ZAH ISOLATED FORM
  0xD2, // U+FEC6 ARABIC ZAH FINAL FORM
  0xD0, // U+FEC7 ARABIC ZAH INITIAL FORM
  0xD1, // U+FEC8 ARABIC ZAH MEDIAL FORM
  0x59, // U+FEC9 ARABIC AIN ISOLATED FORM
  0xD5, // U+FECA ARABIC AIN FINAL FORM
  0xD3, // U+FECB ARABIC AIN INITIAL FORM
  0xD4, // U+FECC ARABIC AIN MEDIAL FORM
  0x5A, // U+FECD ARABIC GHAIN ISOLATED FORM
  0xD8, // U+FECE ARABIC GHAIN FINAL FORM
  0xD6, // U+FECF ARABIC GHAIN INITIAL FORM
  0xD7, // U+FED0 ARABIC GHAIN MEDIAL FORM
  0x61, // U+FED1 ARABIC FA ISOLATED FORM
  0xDB, // U+FED2 ARABIC FEH FINAL FORM
  0xD9, // U+FED3 ARABIC FEH INITIAL FORM
  0xDA, // U+FED4 ARABIC FEH MEDIAL FORM
  0x62, // U+FED5 ARABIC QAF ISOLATED FORM
  0xDE, // U+FED6 ARABIC QAF FINAL FORM
  0xDC, // U+FED7 ARABIC QAF INITIAL FORM
  0xDD, // U+FED8 ARABIC QAF MEDIAL FORM
  0x63, // U+FED9 ARABIC KAF ISOLATED FORM
  0xE1, // U+FEDA ARABIC KAF FINAL FORM
  0xDF, // U+FEDB ARABIC KAF INITIAL FORM
  0xE0, // U+FEDC ARABIC KAF MEDIAL FORM
  0x64, // U+FEDD ARABIC LAM ISOLATED FORM
  0xE4, // U+FEDE ARABIC LAM FINAL FORM
  0xE2, // U+FEDF ARABIC LAM INITIAL FORM
  0xE3, // U+FEE0 ARABIC LAM MEDIAL FORM
  0x65, // U+FEE1 ARABIC MEEM ISOLATED FORM
  0xE7, // U+FEE2 ARABIC MEEM FINAL FORM
  0xE5, // U+FEE3 ARABIC MEEM INITIAL FORM
  0xE6, // U+FEE4 ARABIC MEEM MEDIAL FORM
  0x66, // U+FEE5 ARABIC NOON ISOLATED FORM
  0xEA, // U+FEE6 ARABIC NOON FINAL FORM
  0xE8, // U+FEE7 ARABIC NOON INITIAL FORM
  0xE9, // U+FEE8 ARABIC NOON MEDIAL FORM
  0x67, // U+FEE9 ARABIC HA ISOLATED FORM
  0xED, // U+FEEA ARABIC HEH FINAL FORM
  0xEB, // U+FEEB ARABIC HEH INITIAL FORM
  0xEC, // U+FEEC ARABIC HEH MEDIAL FORM
  0x68, // U+FEED ARABIC WAW ISOLATED FORM
  0xAA, // U+FEEE ARABIC WAW FINAL FORM
  0x69, // U+FEEF ARABIC ALEF MAKSURA ISOLATED FORM
  0xAB, // U+FEF0 ARABIC ALEF MAKSURA FINAL FORM
  0x6A, // U+FEF1 ARABIC YEH ISOLATED FORM
  0xF0, // U+FEF2 ARABIC YEH FINAL FORM
  0xEE, // U+FEF3 ARABIC YEH INITIAL FORM
  0xEF, // U+FEF4 ARABIC YEH MEDIAL FORM
  0x76, // U+FEF5 ARABIC LIGATURE MADDA ON LAM ALEF ISOLATED FORM
  0xFA, // U+FEF6 ARABIC LIGATURE MADDA ON LAM ON ALEF FINAL FORM
  0x77, // U+FEF7 ARABIC LIGATURE HAMZA ON LAM ALEF ISOLATED FORM
  0xFC, // U+FEF8 ARABIC LIGATURE HAMZA ON LAM ALEF FINAL FORM
  0x78, // U+FEF9 ARABIC LIGATURE HAMZA UNDER LAM ALEF ISOLATED FORM
  0xFB, // U+FEFA ARABIC LIGATURE HAMZA UNDER LAM ALEF FINAL FORM
  0x79, // U+FEFB ARABIC LIGATURE LAM ALEF ISOLATED FORM
  0xFD  // U+FEFC ARABIC LIGATURE LAM ALEF FINAL FORM
 };

NS_IMETHODIMP nsUnicodeToLangBoxArabic16::Convert(
      const PRUnichar * aSrc, PRInt32 * aSrcLength,
      char * aDest, PRInt32 * aDestLength)
{
   char* dest = aDest;
   PRInt32 inlen = 0;

   while (inlen < *aSrcLength) {
     PRUnichar aChar = aSrc[inlen];
     
     if (((aChar >= 0x0020) && (aChar <= 0x0027)) ||
          (aChar == 0x2A) ||
          (aChar == 0x2B) ||
         ((aChar >= 0x002D) && (aChar <= 0x002F)) ||
          (aChar == 0x003A) ||
         ((aChar >= 0x003C) && (aChar <= 0x003E)) ||
          (aChar == 0x40) ||
          (aChar == 0x5C) ||
          (aChar == 0x5E) ||
          (aChar == 0x5F) ||
          (aChar == 0x7C) ||
          (aChar == 0x7E)) {
       *dest++ = (char) aChar;
       // ISO-8859-6-16 swaps symmetric characters internally, but we have
       // already swapped them where necessary during Bidi reordering, so we
       // must swap them back here.
     } else if (0x0028 == aChar) {
       *dest++ = 0x29;
     } else if (0x0029 == aChar) {
       *dest++ = 0x28;
     } else if (0x005B == aChar) {
       *dest++ = 0x5D;
     } else if (0x005D == aChar) {
       *dest++ = 0x5B;
     } else if (0x007B == aChar) {
       *dest++ = 0x7D;
     } else if (0x007D == aChar) {
       *dest++ = 0x7B;
     } else if (0x060C == aChar) {
       // ARABIC COMMA
       *dest++ = 0x2C;
     } else if (0x061B == aChar) {
       // ARABIC SEMICOLON
       *dest++ = 0x3B;
     } else if (0x061F == aChar) {
       // ARABIC QUESTION MARK
       *dest++ = 0x3F;
     } else if (0x0640 == aChar) {
       // ARABIC TATWEEL
       *dest++ = 0x60;
     } else if ((aChar >= 0x0660) && (aChar <=0x0669)) {
       // ARABIC-INDIC DIGITS
       *dest++ = (char)(aChar - 0x0660 + 0x30);
     } else if ((aChar>=0xFE70) && (aChar <= 0xFEFC)) {
       // ARABIC PRESENTATION FORMS
       *dest++ = uni2lbox[aChar-0xFE70];
     } else {
       // do nothing
     }
     inlen++;
   }

    *aDestLength = dest - aDest;
    return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic16::GetMaxLength(
const PRUnichar * aSrc, PRInt32 aSrcLength,
                           PRInt32 * aDestLength)
{
  *aDestLength = aSrcLength;
  return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic16::Finish(
      char * aDest, PRInt32 * aDestLength)
{
   *aDestLength=0;
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic16::Reset()
{
   return NS_OK;
}

NS_IMETHODIMP nsUnicodeToLangBoxArabic16::SetOutputErrorBehavior(
      PRInt32 aBehavior,
      nsIUnicharEncoder * aEncoder, PRUnichar aChar)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsUnicodeToLangBoxArabic16::FillInfo(PRUint32* aInfo)
{
   PRUnichar i;

   /* Start off by marking the whole ASCII range as unrepresentable. If we
    * don't do this we will try to use an ISO-8859-6.16 font for ASCII text
    * embedded in Arabic content, and it will not be rendered correctly.
    * See discussion in bug 172491
    */
   for(i=0x0000; i <= 0x007F; i++)
     CLEAR_REPRESENTABLE(aInfo, i);

   // Mark the few exceptions as representable.
   for(i=0x0020; i <= 0x002B; i++)
     SET_REPRESENTABLE(aInfo, i);
   for(i=0x002D; i <= 0x002F; i++)
     SET_REPRESENTABLE(aInfo, i);
   SET_REPRESENTABLE(aInfo, 0x003A);
   for(i=0x003C; i <= 0x003E; i++)
     SET_REPRESENTABLE(aInfo, i);
   SET_REPRESENTABLE(aInfo, 0x0040);
   for(i=0x005B; i <= 0x005F; i++)
     SET_REPRESENTABLE(aInfo, i);
   for(i=0x007B; i <= 0x007E;i++)
     SET_REPRESENTABLE(aInfo, i);

   // Arabic punctuation and numerals
   SET_REPRESENTABLE(aInfo, 0x060c);
   SET_REPRESENTABLE(aInfo, 0x061b);
   SET_REPRESENTABLE(aInfo, 0x061f);
   SET_REPRESENTABLE(aInfo, 0x0640);
   for(i=0x0660; i<=0x0669; i++)
      SET_REPRESENTABLE(aInfo, i);

   // Arabic Pres Form-B
   for(i=0xFE70; i <= 0xFE72; i++)
     SET_REPRESENTABLE(aInfo, i);
   SET_REPRESENTABLE(aInfo, 0xFE74);
   for(i=0xFE76; i <= 0xFEFC; i++)
     SET_REPRESENTABLE(aInfo, i);

   return NS_OK;
}
