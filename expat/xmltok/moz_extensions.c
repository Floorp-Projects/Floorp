/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Peter Van der Beken.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peter@propagandism.org>
 *
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

/* XXX We really want to be using little2_encoding_ns.type but we don't
       yet define XML_NS in Mozilla. */
static
int MOZ_byte_type(unsigned char p)
{
  /* little2_encoding.type and big2_encoding.type are equal. */
  return p == ':' ? BT_COLON : little2_encoding.type[p];
}

#ifdef IS_LITTLE_ENDIAN

#define BYTE_TYPE(p) \
  ((p)[1] == 0 ? \
    MOZ_byte_type((unsigned char)*(p)) : unicode_byte_type((p)[1], (p)[0]))
#define IS_NAME_CHAR_MINBPC(p) LITTLE2_IS_NAME_CHAR_MINBPC(0, p)
#define IS_NMSTRT_CHAR_MINBPC(p) LITTLE2_IS_NMSTRT_CHAR_MINBPC(0, p)

#else

#define BYTE_TYPE(p) \
  ((p)[0] == 0 ? \
    MOZ_byte_type((unsigned char)(p)[1]) : unicode_byte_type((p)[0], (p)[1]))
#define IS_NAME_CHAR_MINBPC(p) BIG2_IS_NAME_CHAR_MINBPC(0, p)
#define IS_NMSTRT_CHAR_MINBPC(p) BIG2_IS_NMSTRT_CHAR_MINBPC(0, p)

#endif

#define MOZ_EXPAT_VALID_QNAME       (0)
#define MOZ_EXPAT_EMPTY_QNAME       (1 << 0)
#define MOZ_EXPAT_INVALID_CHARACTER (1 << 1)
#define MOZ_EXPAT_MALFORMED         (1 << 2)

int MOZ_XMLCheckQName(const char* ptr, const char* end, int ns_aware,
                      const char** colon)
{
  int result = MOZ_EXPAT_VALID_QNAME;
  int nmstrt = 1;
  *colon = 0;
  if (ptr == end) {
    return MOZ_EXPAT_EMPTY_QNAME;
  }
  do {
    switch (BYTE_TYPE(ptr)) {
    case BT_COLON:
      if (ns_aware) {
        if (*colon != 0 || nmstrt || ptr + 2 == end) {
          /* We already encountered a colon or this is the first or the last
             character so the QName is malformed. */
          result |= MOZ_EXPAT_MALFORMED;
        }
        *colon = ptr;
        nmstrt = 1;
      }
      else if (nmstrt) {
        /* This is the first character so the QName is malformed. */
        result |= MOZ_EXPAT_MALFORMED;
        nmstrt = 0;
      }
      break;
    case BT_NONASCII:
      if (nmstrt) {
        if (!IS_NMSTRT_CHAR_MINBPC(ptr)) {
          /* If this is a valid name character the QName is malformed, 
             otherwise it contains an invalid character. */
          result |= IS_NAME_CHAR_MINBPC(ptr) ? MOZ_EXPAT_MALFORMED :
                                               MOZ_EXPAT_INVALID_CHARACTER;
        }
      }
      else if (!IS_NAME_CHAR_MINBPC(ptr)) {
        result |= MOZ_EXPAT_INVALID_CHARACTER;
      }
      nmstrt = 0;
      break;
    case BT_NMSTRT:
    case BT_HEX:
      nmstrt = 0;
      break;
    case BT_DIGIT:
    case BT_NAME:
    case BT_MINUS:
      if (nmstrt) {
        result |= MOZ_EXPAT_MALFORMED;
        nmstrt = 0;
      }
      break;
    default:
      result |= MOZ_EXPAT_INVALID_CHARACTER;
      nmstrt = 0;
    }
    ptr += 2;
  } while (ptr != end);
  return result;
}

int MOZ_XMLIsLetter(const char* ptr)
{
  switch (BYTE_TYPE(ptr)) {
  case BT_NONASCII:
    if (!IS_NMSTRT_CHAR_MINBPC(ptr)) {
      return 0;
    }
  case BT_NMSTRT:
  case BT_HEX:
    return 1;
  default:
    return 0;
  }
}

int MOZ_XMLIsNCNameChar(const char* ptr)
{
  switch (BYTE_TYPE(ptr)) {
  case BT_NONASCII:
    if (!IS_NAME_CHAR_MINBPC(ptr)) {
      return 0;
    }
  case BT_NMSTRT:
  case BT_HEX:
  case BT_DIGIT:
  case BT_NAME:
  case BT_MINUS:
    return 1;
  default:
    return 0;
  }
}

#undef BYTE_TYPE
#undef IS_NAME_CHAR_MINBPC
#undef IS_NMSTRT_CHAR_MINBPC
#undef CHECK_NAME_CASES
#undef CHECK_NMSTRT_CASES
