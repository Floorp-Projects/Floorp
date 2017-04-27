/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsBidiUtils.h"

#define ARABIC_TO_HINDI_DIGIT_INCREMENT (START_HINDI_DIGITS - START_ARABIC_DIGITS)
#define PERSIAN_TO_HINDI_DIGIT_INCREMENT (START_HINDI_DIGITS - START_FARSI_DIGITS)
#define ARABIC_TO_PERSIAN_DIGIT_INCREMENT (START_FARSI_DIGITS - START_ARABIC_DIGITS)
#define NUM_TO_ARABIC(c) \
  ((((c)>=START_HINDI_DIGITS) && ((c)<=END_HINDI_DIGITS)) ? \
   ((c) - (uint16_t)ARABIC_TO_HINDI_DIGIT_INCREMENT) : \
   ((((c)>=START_FARSI_DIGITS) && ((c)<=END_FARSI_DIGITS)) ? \
    ((c) - (uint16_t)ARABIC_TO_PERSIAN_DIGIT_INCREMENT) : \
     (c)))
#define NUM_TO_HINDI(c) \
  ((((c)>=START_ARABIC_DIGITS) && ((c)<=END_ARABIC_DIGITS)) ? \
   ((c) + (uint16_t)ARABIC_TO_HINDI_DIGIT_INCREMENT): \
   ((((c)>=START_FARSI_DIGITS) && ((c)<=END_FARSI_DIGITS)) ? \
    ((c) + (uint16_t)PERSIAN_TO_HINDI_DIGIT_INCREMENT) : \
     (c)))
#define NUM_TO_PERSIAN(c) \
  ((((c)>=START_HINDI_DIGITS) && ((c)<=END_HINDI_DIGITS)) ? \
   ((c) - (uint16_t)PERSIAN_TO_HINDI_DIGIT_INCREMENT) : \
   ((((c)>=START_ARABIC_DIGITS) && ((c)<=END_ARABIC_DIGITS)) ? \
    ((c) + (uint16_t)ARABIC_TO_PERSIAN_DIGIT_INCREMENT) : \
     (c)))

char16_t HandleNumberInChar(char16_t aChar, bool aPrevCharArabic, uint32_t aNumFlag)
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

nsresult HandleNumbers(char16_t* aBuffer, uint32_t aSize, uint32_t aNumFlag)
{
  uint32_t i;

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

bool HasRTLChars(const char16_t* aText, uint32_t aLength)
{
  // This is used to determine whether a string has right-to-left characters
  // that mean it will require bidi processing.
  const char16_t* cp = aText;
  const char16_t* end = cp + aLength;
  while (cp < end) {
    uint32_t ch = *cp++;
    if (NS_IS_HIGH_SURROGATE(ch) && cp < end && NS_IS_LOW_SURROGATE(*cp)) {
      ch = SURROGATE_TO_UCS4(ch, *cp++);
    }
    if (UTF32_CHAR_IS_BIDI(ch) || IsBidiControlRTL(ch)) {
      return true;
    }
  }
  return false;
}
