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
 * The Original Code is lineterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* unistring.c: Unicode string operations implementation */

/* public declarations */
#include "unistring.h"

/* private declarations */

/** Encodes Unicode string US with NUS characters into UTF8 string S with
 * upto NS characters, returning the number of REMAINING Unicode characters
 * and the number of ENCODED Utf8 characters
 */
void ucstoutf8(const UNICHAR* us, int nus, char* s, int ns, 
               int* remaining, int* encoded)
{
  int j, k;

  j = 0;
  k = 0;
  while ((j < ns) && (k < nus)) {
    UNICHAR uch = us[k++];

    if (uch < 0x0080) {
      s[j++] = uch;

    } else if (uch < 0x0800) {
      if (j >= ns-1) break;
      s[j++] = ((uch & 0x07C0) >>  6) | 0xC0;
      s[j++] =  (uch & 0x003F)        | 0x80;

    } else {
      if (j >= ns-2) break;
      s[j++] = ((uch & 0xF000) >> 12) | 0xE0;
      s[j++] = ((uch & 0x0FC0) >>  6) | 0x80;
      s[j++] =  (uch & 0x003F)        | 0x80;
    }
  }

  if (remaining)
    *remaining = nus - k;

  if (encoded)
    *encoded = j;
}


/** Decodes UTF8 string S with NS characters to Unicode string US with
 * upto NUS characters, returning the number of REMAINING Utf8 characters
 * and the number of DECODED Unicode characters.
 * If skipNUL is non-zero, NUL input characters are skipped.
 * returns 0 if successful,
 *        -1 if an error occurred during decoding
 */
int utf8toucs(const char* s, int ns, UNICHAR* us, int nus,
              int skipNUL, int* remaining, int* decoded)
{
  int j, k;
  int retcode = 0;

  j = 0;
  k = 0;
  while ((j < ns) && (k < nus)) {
    char ch = s[j];

    if (0x80 & ch) {
      if (0x40 & ch) {
        if (0x20 & ch) {
          /* consume 3 */
          if (j >= ns-2) break;

          if ( (s[j+1] & 0x40) || !(s[j+1] & 0x80) ||
               (s[j+2] & 0x40) || !(s[j+2] & 0x80) ) {
            retcode = -1;
          }

          us[k++] =   ((ch     & 0x0F) << 12)
                    | ((s[j+1] & 0x3F) << 6)
                    | ( s[j+2] & 0x3F);

          j += 3;

        } else {
          /* consume 2 */
          if (j >= ns-1) break;

          if ( (s[j+1] & 0x40) || !(s[j+1] & 0x80) ) {
            retcode = -1;
          }

          us[k++] =   ((ch     & 0x1F) << 6)
                    | ( s[j+1] & 0x3F);
          j += 2;
        }

      } else {
        /* consume 1 (error) */
        retcode = -1;
        j++;
      }

    } else {
      /* consume 1 */
      if (ch || !skipNUL) {
        us[k++] = ch;
      }
      j++;
    }
  }

  if (remaining)
    *remaining = ns - j;

  if (decoded)
    *decoded = k;

  return retcode;
}


/** Prints Unicode string US with NUS characters to file stream STREAM,
 * escaping non-printable ASCII characters and all non-ASCII characters
 */
void ucsprint(FILE* stream, const UNICHAR* us, int nus)
{
  static const char hexDigits[17] = "0123456789abcdef";
  UNICHAR uch;
  int k;

  for (k=0; k<nus; k++) {
    uch = us[k];

    if (uch < (UNICHAR)U_SPACE) {
      /* ASCII control character */
      fprintf(stream, "^%c", (char) uch+U_ATSIGN);

    } else if (uch == (UNICHAR)U_CARET) {
      /* Caret */
      fprintf(stream, "^^");

    } else if (uch < (UNICHAR)U_DEL) {
      /* Printable ASCII character */
      fprintf(stream, "%c", (char) uch);

    } else {
      /* DEL or non-ASCII character */
      char esc_str[8]="&#0000;";
      int j;
      for (j=5; j>1; j--) {
        esc_str[j] = hexDigits[uch%16];
        uch = uch / 16;
      }
      fprintf(stream, "%s", esc_str);
    }
  }
}


/** Copy exactly n characters from plain character source string to UNICHAR
 * destination string, ignoring source characters past a null character and
 * padding the destination with null characters if necessary.
 */
UNICHAR* ucscopy(register UNICHAR* dest, register const char* srcplain,
                 size_t n)
{
  register UNICHAR ch;
  register const UNICHAR* destmx = dest + n;

  /* Copy characters from source to destination, stopping at NUL */
  while (dest < destmx) {
    *dest++ = (ch = *srcplain++);
    if (ch == U_NUL)
      break;
  }

  /* Pad with NULs, if necessary */
  while (dest < destmx)
    *dest++ = U_NUL;

  return dest;
}
    

#ifndef USE_WCHAR
/** Locates first occurrence of character within string and returns pointer
 * to it if found, else returning null pointer. (character may be NUL)
 */
UNICHAR* ucschr(register const UNICHAR* str, register const UNICHAR chr)
{
  do {
    if (*str == chr)
      return (UNICHAR*) str;
  } while (*str++ != U_NUL);

  return NULL;
}


/** Locates last occurrence of character within string and returns pointer
 * to it if found, else returning null pointer. (character may be NUL)
 */
UNICHAR* ucsrchr(register const UNICHAR* str, register const UNICHAR chr)
{
  const UNICHAR* retstr = NULL;
  do {
    if (*str == chr)
      retstr = str;
  } while (*str++ != U_NUL);

  return (UNICHAR*) retstr;
}


/** Compare all characters between string1 and string2, returning
 * a zero value if all characters are equal, or returning
 * character1 - character2 for the first character that is different
 * between the two strings.
 * (Characters following a null character are not compared.)
 */
int ucscmp(register const UNICHAR* str1, register const UNICHAR* str2)
{
  register UNICHAR ch1, ch2;

  do {
    if ((ch1 = *str1++) != (ch2 = *str2++))
      return ch1 - ch2;

  } while (ch1 != U_NUL);

  return 0;
}

    
/** Compare upto n characters between string1 and string2, returning
 * a zero value if all compared characters are equal, or returning
 * character1 - character2 for the first character that is different
 * between the two strings.
 * (Characters following a null character are not compared.)
 */
int ucsncmp(register const UNICHAR* str1, register const UNICHAR* str2,
            size_t n)
{
  register UNICHAR ch1, ch2;
  register const UNICHAR* str1mx = str1 + n;

  while (str1 < str1mx) {
    if ((ch1 = *str1++) != (ch2 = *str2++))
      return ch1 - ch2;

    if (ch1 == U_NUL)
      break;
  }

  return 0;
}

    
/** Copy exactly n characters from source to destination, ignoring source
 * characters past a null character and padding the destination with null
 * characters if necessary.
 */
UNICHAR* ucsncpy(register UNICHAR* dest, register const UNICHAR* src,
                 size_t n)
{
  register UNICHAR ch;
  register const UNICHAR* destmx = dest + n;

  /* Copy characters from source to destination, stopping at NUL */
  while (dest < destmx) {
    *dest++ = (ch = *src++);
    if (ch == U_NUL)
      break;
  }

  /* Pad with NULs, if necessary */
  while (dest < destmx)
    *dest++ = U_NUL;

  return dest;
}
    

/** Returns string length
 */
size_t ucslen(const UNICHAR* str)
{
  register const UNICHAR* strcp = str;

  while (*strcp++ != U_NUL);

  return strcp - str - 1;
}

    
/** Locates substring within string and returns pointer to it if found,
 * else returning null pointer. If substring has zero length, then full
 * string is returned.
 */
UNICHAR* ucsstr(register const UNICHAR* str, const UNICHAR* substr)
{
  register UNICHAR subch1, ch;

  /* If null substring, return string */
  if (*substr == U_NUL)
    return (UNICHAR*) str;

  /* First character of non-null substring */
  subch1 = *substr;

  if ((ch = *str) == U_NUL)
    return NULL;

  do {

    if (ch == subch1) {
      /* First character matches; check if rest of substring matches */
      register const UNICHAR* strcp = str;
      register const UNICHAR* substrcp = substr;
      do {
        substrcp++;
        strcp++;
        if (*substrcp == U_NUL)
          return (UNICHAR*) str;
      } while (*substrcp == *strcp);
    }

  } while ((ch = *(++str)) != U_NUL);

  return NULL;
}
    

/** Returns length of longest initial segment of string that contains
 * only the specified characters.
 */
size_t ucsspn(const UNICHAR* str, const UNICHAR* chars)
{
  register UNICHAR strch, ch;
  register const UNICHAR* charscp;
  register const UNICHAR* strcp = str;

  while ((strch = *strcp++) != U_NUL) {
    charscp = chars;

    /* Check that it is one of the specified characters */
    while ((ch = *charscp++) != U_NUL) {
      if (strch == ch)
        break;
    }
    if (ch == U_NUL)
      return (size_t) (strcp - str - 1);
  }

  return (size_t) (strcp - str - 1);
}
    

/** Returns length of longest initial segment of string that does not
 * contain any of the specified characters.
 */
size_t ucscspn(const UNICHAR* str, const UNICHAR* chars)
{
  register UNICHAR strch, ch;
  register const UNICHAR* charscp;
  register const UNICHAR* strcp = str;

  while ((strch = *strcp++) != U_NUL) {
    charscp = chars;

    /* Check that it is not one of the specified characters */
    while ((ch = *charscp++) != U_NUL) {
      if (strch == ch)
        return (size_t) (strcp - str - 1);
    }
  }

  return (size_t) (strcp - str - 1);
}
#endif  /* !USE_WCHAR */
