/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is lineterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

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

    if (uch < U_SPACE) {
      /* ASCII control character */
      fprintf(stream, "^%c", (char) uch+U_ATSIGN);

    } else if (uch == U_CARET) {
      /* Caret */
      fprintf(stream, "^^");

    } else if (uch < U_DEL) {
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
