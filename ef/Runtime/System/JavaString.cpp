/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "plstr.h"
#include "JavaString.h"
#include "ClassCentral.h"
#include "ClassFileSummary.h"

static inline JavaArray *newCharArray(Uint32 length) 
{
    void *mem = malloc(arrayEltsOffset(tkChar) + getTypeKindSize(tkChar)*length);
    return (new (mem) JavaArray(Array::obtain(tkChar), length));
}

/* Count the number of bytes it would take to encode the given Unicode
 * string using UTF-8.  Add in the extra byte for the terminating NUL.
 */
static int
countUtf8Chars(const uint16 *ucs2, int ucs2len)
{
    int utf8len = 1;    // Need one character for terminating NUL

    for (int i = ucs2len-1; i >= 0; i--) {
        uint16 u = ucs2[i];
        if (u < 0x80)
            utf8len += 1;
        else if (u < 0x800)
            utf8len += 2;
        else
            utf8len += 3;
    }
    return utf8len;
}

/* Convert a Unicode (UCS-2) string to UTF-8 encoding.  The length of
 * the destination string, in bytes, is given by the utf8len argument.
 * A NUL character is appended to the destination string, if possible.
 * Returns: the actual length of the resulting string, in bytes.
 */
static int
convertUnicodeToUtf8(char *utf8, const uint16* ucs2, int utf8len)
{
    char* start_utf8 = utf8;
    char* lastchar = utf8 + utf8len - 1;

    while (utf8 < lastchar) {
        uint16 u = *ucs2++;
        if (u < 0x80) {
            *utf8++ = (char)u;
        } else if (u < 0x800) {
            if (utf8 >= (lastchar - 1))
                break;
            *utf8++ = 0xc0 | ((u >> 6) & 0x1f);
            *utf8++ = 0x80 | (u & 0x3f);
        } else {
            if (utf8 >= (lastchar - 2))
                break;
            *utf8++ = 0xe0 | ((u >> 12) & 0x0f);
            *utf8++ = 0x80 | ((u >> 6) & 0x3f);
            *utf8++ = 0x80 | (u & 0x3f);
        }
    }
    if (utf8 <= lastchar)
        *utf8 = 0;

    return utf8 - start_utf8;
}

/* Return the UTF8 representation of this string. This routine allocates 
 * enough memory for the conversion; this memory can be freed using 
 * JavaString::freeUtf()
 */
char *JavaString::convertUtf()
{
    const uint16 *chars = getStr();
    int utf8len = countUtf8Chars(chars, count);
    char *utf8 = new char[utf8len];

    convertUnicodeToUtf8(utf8, chars, utf8len);

    return utf8;
}

void JavaString::freeUtf(char *str)
{
  delete [] str;
}

/* Count the number of Unicode characters in a NUL-terminated
 * UTF8 string.  Don't count the final NUL character.
 */
static int
countUnicodeChars(const char *utf8)
{
    signed char c;
    int length = 0;
    
    // Unicode characters are encoded as 1, 2, or 3 bytes in a UCS-2 string
    while (c = *utf8) {
        length++;

        if (c >= 0) {
            // Characters in the range of 0..0x7f are encoded using one byte
            // b0xxxxxxx
            utf8++;
        } else if ((c & 0xe0) == 0xc0) {
            // Characters in the range 0x80..0x7ff are encoded using two bytes
            // b110xxxxx b10yyyyyy
            utf8 += 2;
        } else {
            // Characters in the range 0x800..0xffff are encoded using three bytes
            // b1110xxxx b10yyyyyy b10zzzzzz
            PR_ASSERT((c & 0xf0) == 0xe0);
            utf8 += 3;  
        }
    }
    return length;
}

/* Convert a UTF-8 encoded string to Unicode (UCS-2) representation.  The
 * length of the destination string, in 16-bit characters, is given by the
 * ucs2 argument.  The result is *not* NUL-terminated.
 * Returns: the actual length of the resulting string, in characters.
 */
static int
convertUTF8ToUnicode(uint16 *ucs2, const char *utf8, int ucs2len)
{
    signed char c;
    int length = 0;
    
    // Unicode characters are encoded as 1, 2, or 3 bytes in a UCS-2 string
    while ((c = *utf8) != 0) {
        length++;
        if (length > ucs2len)
            return ucs2len;

        if (c >= 0) {
            // Characters in the range of 0..0x7f are encoded using one byte
            // b0xxxxxxx
            *ucs2 = c;
            utf8++;
        } else if ((c & 0xe0) == 0xc0) {
            // Characters in the range 0x80..0x7ff are encoded using two bytes
            // b110xxxxx b10yyyyyy
            *ucs2 = ((c & 0x1f) << 6) | (utf8[1] & 0x3f);
            utf8 += 2;
        } else {
            // Characters in the range 0x800..0xffff are encoded using three bytes
            // b1110xxxx b10yyyyyy b10zzzzzz
            PR_ASSERT((c & 0xf0) == 0xe0);
            *ucs2 = ((c & 0x0f) << 12) | ((utf8[1] & 0x3f) << 6) | (utf8[2] & 0x3f);
            utf8 += 3;
        }
        ucs2++;
    }
    return length;
}


/* Create a new JavaString from a char array that represents the string in UTF-8
 * format.
 */
JavaString::JavaString(const char *str) : JavaObject(*strType)
{
  count = countUnicodeChars(str);

  offset = 0;

  value = (JavaArray *) newCharArray(count);
  uint16 *chars = const_cast<uint16 *>(getStr());

  convertUTF8ToUnicode(chars, str, count);
}

/* print a textual representation of this string */
void JavaString::dump()
{
  const uint16 *chars = getStr();

  for (int16 i = 0; i < count; i++)
    putchar(chars[i]);

  putchar('\n');
}

Type *JavaString::strType;

void JavaString::staticInit()
{
  strType = &asType(Standard::get(cString));
}



