/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Maha Abou El Rous <mahar@eg.ibm.com>
 *   Lina Kemmel <lkemmel@il.ibm.com>
 *   Simon Montagu <smontagu@netscape.com>
 *   Roozbeh Pournader <roozbeh@sharif.edu>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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
#include "nsBidiUtils.h"
#include "nsCharTraits.h"
#include "nsUnicodeProperties.h"

#define ARABIC_TO_HINDI_DIGIT_INCREMENT (START_HINDI_DIGITS - START_ARABIC_DIGITS)
#define PERSIAN_TO_HINDI_DIGIT_INCREMENT (START_HINDI_DIGITS - START_FARSI_DIGITS)
#define ARABIC_TO_PERSIAN_DIGIT_INCREMENT (START_FARSI_DIGITS - START_ARABIC_DIGITS)
#define NUM_TO_ARABIC(c) \
  ((((c)>=START_HINDI_DIGITS) && ((c)<=END_HINDI_DIGITS)) ? \
   ((c) - (PRUint16)ARABIC_TO_HINDI_DIGIT_INCREMENT) : \
   ((((c)>=START_FARSI_DIGITS) && ((c)<=END_FARSI_DIGITS)) ? \
    ((c) - (PRUint16)ARABIC_TO_PERSIAN_DIGIT_INCREMENT) : \
     (c)))
#define NUM_TO_HINDI(c) \
  ((((c)>=START_ARABIC_DIGITS) && ((c)<=END_ARABIC_DIGITS)) ? \
   ((c) + (PRUint16)ARABIC_TO_HINDI_DIGIT_INCREMENT): \
   ((((c)>=START_FARSI_DIGITS) && ((c)<=END_FARSI_DIGITS)) ? \
    ((c) + (PRUint16)PERSIAN_TO_HINDI_DIGIT_INCREMENT) : \
     (c)))
#define NUM_TO_PERSIAN(c) \
  ((((c)>=START_HINDI_DIGITS) && ((c)<=END_HINDI_DIGITS)) ? \
   ((c) - (PRUint16)PERSIAN_TO_HINDI_DIGIT_INCREMENT) : \
   ((((c)>=START_ARABIC_DIGITS) && ((c)<=END_ARABIC_DIGITS)) ? \
    ((c) + (PRUint16)ARABIC_TO_PERSIAN_DIGIT_INCREMENT) : \
     (c)))

PRUnichar HandleNumberInChar(PRUnichar aChar, bool aPrevCharArabic, PRUint32 aNumFlag)
{
  // IBMBIDI_NUMERAL_NOMINAL *
  // IBMBIDI_NUMERAL_REGULAR
  // IBMBIDI_NUMERAL_HINDICONTEXT
  // IBMBIDI_NUMERAL_ARABIC
  // IBMBIDI_NUMERAL_HINDI

  switch (aNumFlag) {
    case IBMBIDI_NUMERAL_HINDI:
      return NUM_TO_HINDI(aChar);
    case IBMBIDI_NUMERAL_ARABIC:
      return NUM_TO_ARABIC(aChar);
    case IBMBIDI_NUMERAL_PERSIAN:
      return NUM_TO_PERSIAN(aChar);
    case IBMBIDI_NUMERAL_REGULAR:
    case IBMBIDI_NUMERAL_HINDICONTEXT:
    case IBMBIDI_NUMERAL_PERSIANCONTEXT:
      // for clipboard handling
      //XXX do we really want to convert numerals when copying text?
      if (aPrevCharArabic) {
        if (aNumFlag == IBMBIDI_NUMERAL_PERSIANCONTEXT)
          return NUM_TO_PERSIAN(aChar);
        else
          return NUM_TO_HINDI(aChar);
      }
      else
        return NUM_TO_ARABIC(aChar);
    case IBMBIDI_NUMERAL_NOMINAL:
    default:
      return aChar;
  }
}

nsresult HandleNumbers(PRUnichar* aBuffer, PRUint32 aSize, PRUint32 aNumFlag)
{
  PRUint32 i;

  switch (aNumFlag) {
    case IBMBIDI_NUMERAL_HINDI:
    case IBMBIDI_NUMERAL_ARABIC:
    case IBMBIDI_NUMERAL_PERSIAN:
    case IBMBIDI_NUMERAL_REGULAR:
    case IBMBIDI_NUMERAL_HINDICONTEXT:
    case IBMBIDI_NUMERAL_PERSIANCONTEXT:
      for (i=0;i<aSize;i++)
        aBuffer[i] = HandleNumberInChar(aBuffer[i], !!(i>0 ? aBuffer[i-1] : 0), aNumFlag);
      break;
    case IBMBIDI_NUMERAL_NOMINAL:
    default:
      break;
  }
  return NS_OK;
}

#define LRM_CHAR 0x200e
#define LRE_CHAR 0x202a
#define RLO_CHAR 0x202e
bool IsBidiControl(PRUint32 aChar)
{
  // This method is used when stripping Bidi control characters for
  // display, so it will return TRUE for LRM, RLM, LRE, RLE, PDF, LRO and RLO
  return ((LRE_CHAR <= aChar && aChar <= RLO_CHAR) ||
          ((aChar)&0xfffffe)==LRM_CHAR);
}

bool HasRTLChars(const nsAString& aString)
{
// This is used to determine whether to enable bidi if a string has 
// right-to-left characters. To simplify things, anything that could be a
// surrogate or RTL presentation form is covered just by testing >= 0xD800).
// It's fine to enable bidi in rare cases where it actually isn't needed.
  PRInt32 length = aString.Length();
  for (PRInt32 i = 0; i < length; i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch >= 0xD800 || IS_IN_BMP_RTL_BLOCK(ch)) {
      return true;
    }
  }
  return false;
}
