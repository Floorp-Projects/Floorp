/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsCRT.h"

// XXX Bug: These tables don't lowercase the upper 128 characters properly

// This table maps uppercase characters to lower case characters;
// characters that are neither upper nor lower case are unaffected.
static const unsigned char kUpper2Lower[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64,

    // upper band mapped to lower [A-Z] => [a-z]
       97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,

                                               91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

static const unsigned char kLower2Upper[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96,

    // lower band mapped to upper [a-z] => [A-Z]
       65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,

                                              123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

static const PRUnichar kIsoLatin1ToUCS2[256] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

//----------------------------------------------------------------------

#define TOLOWER(_ucs2) \
  (((_ucs2) < 256) ? PRUnichar(kUpper2Lower[_ucs2]) : _ToLower(_ucs2))

#define TOUPPER(_ucs2) \
  (((_ucs2) < 256) ? PRUnichar(kLower2Upper[_ucs2]) : _ToUpper(_ucs2))

static PRUnichar _ToLower(PRUnichar aChar)
{
  // XXX need i18n code here
  return aChar;
}

static PRUnichar _ToUpper(PRUnichar aChar)
{
  // XXX need i18n code here
  return aChar;
}

//----------------------------------------------------------------------

PRUnichar nsCRT::ToUpper(PRUnichar aChar)
{
  return TOUPPER(aChar);
}

PRUnichar nsCRT::ToLower(PRUnichar aChar)
{
  return TOLOWER(aChar);
}

PRInt32 nsCRT::strlen(const PRUnichar* s)
{
  PRInt32 len = 0;
  while (*s++ != 0) {
    len++;
  }
  return len;
}

PRInt32 nsCRT::strcmp(const PRUnichar* s1, const PRUnichar* s2)
{
  for (;;) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = *s2++;
    if (c1 != c2) {
      if (c1 < c2) return -1;
      return 1;
    }
    if (c1 == 0) break;
  }
  return 0;
}

// characters following a null character are not compared
PRInt32 nsCRT::strncmp(const PRUnichar* s1, const PRUnichar* s2, PRInt32 n)
{
  while (--n >= 0) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = *s2++;
    if (c1 != c2) {
      if (c1 < c2) return -1;
      return 1;
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strcasecmp(const PRUnichar* s1, const PRUnichar* s2)
{
  for (;;) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = *s2++;
    if (c1 != c2) {
      c1 = TOLOWER(c1);
      c2 = TOLOWER(c2);
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strncasecmp(const PRUnichar* s1, const PRUnichar* s2, PRInt32 n)
{
  while (--n >= 0) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = *s2++;
    if (c1 != c2) {
      c1 = TOLOWER(c1);
      c2 = TOLOWER(c2);
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strcmp(const PRUnichar* s1, const char* s2)
{
  for (;;) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
    if (c1 != c2) {
      if (c1 < c2) return -1;
      return 1;
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strncmp(const PRUnichar* s1, const char* s2, PRInt32 n)
{
  while (--n >= 0) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
    if (c1 != c2) {
      if (c1 < c2) return -1;
      return 1;
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strcasecmp(const PRUnichar* s1, const char* s2)
{
  for (;;) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
    if (c1 != c2) {
      c1 = TOLOWER(c1);
      c2 = TOLOWER(c2);
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::strncasecmp(const PRUnichar* s1, const char* s2, PRInt32 n)
{
  while (--n >= 0) {
    PRUnichar c1 = *s1++;
    PRUnichar c2 = kIsoLatin1ToUCS2[*(const unsigned char*)s2++];
    if (c1 != c2) {
      c1 = TOLOWER(c1);
      c2 = TOLOWER(c2);
      if (c1 != c2) {
        if (c1 < c2) return -1;
        return 1;
      }
    }
    if (c1 == 0) break;
  }
  return 0;
}

PRInt32 nsCRT::HashCode(const PRUnichar* us)
{
  PRInt32 rv = 0;
  PRUnichar ch;
  while ((ch = *us++) != 0) {
    // FYI: rv = rv*37 + ch
    rv = ((rv << 5) + (rv << 2) + rv) + ch;
  }
  return rv;
}

PRInt32 nsCRT::HashCode(const PRUnichar* us, PRInt32* uslenp)
{
  PRInt32 rv = 0;
  PRInt32 len = 0;
  PRUnichar ch;
  while ((ch = *us++) != 0) {
    // FYI: rv = rv*37 + ch
    rv = ((rv << 5) + (rv << 2) + rv) + ch;
    len++;
  }
  *uslenp = len;
  return rv;
}

PRInt32 nsCRT::atoi( const PRUnichar *string )
{
  return atoi(string);
}

