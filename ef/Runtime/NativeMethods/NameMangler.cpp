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
#include "NameMangler.h"

#include <ctype.h>

#include "prprf.h"
#include "plstr.h"

typedef Uint16 unicode;


Int32 NameMangler::mangleUnicodeChar(Int32 ch, char *bufptr, char *bufend) 
{
  int x, len = bufend - bufptr;

  // We always want 6 characters plus a null.
  switch (len)
    {
    case 0:
      return 0;

    default:
      len = 7;
      x = ch & 0x0f;
      bufptr[5] = x < 10 ? x + '0' : x + 'a';
    case 6:
      x = (ch >> 4) & 0x0f;
      bufptr[4] = x < 10 ? x + '0' : x + 'a';
    case 5:
      x = (ch >> 8) & 0x0f;
      bufptr[3] = x < 10 ? x + '0' : x + 'a';
    case 4:
      x = (ch >> 12) & 0x0f;
      bufptr[2] = x < 10 ? x + '0' : x + 'a';
    case 3:
      bufptr[1] = '0';
    case 2:
      bufptr[0] = '_';
    case 1:;
    }
  bufptr[--len] = '\0';
  return len;
}

// FIXME - does not do UTF8 to unicode conversion - fur
Int32 NameMangler::mangleUTFString(const char *name, 
				   char *buffer, 
				   int buflen, 
				   NameMangler::MangleUTFType type)
{
  if (buflen == 0)
    return 0;

  switch (type)
    {
    case mangleUTFClass:
      // Just transform '/' to '_'.
      {
        strncpy(buffer, name, buflen-1);
        buffer[buflen-1] = '\0';

        char *p = buffer;
        while (*p) {
            if ((*p == '/') || (*p == '.'))
                *p = '_';
            p++;
        }
        break;
      }

    case mangleUTFFieldStub:
    case mangleUTFSignature:
      // These don't appear to be used, and the header doesn't
      // decribe exactly what they mean.
      PR_ASSERT(0);
      return 0;

    case mangleUTFJNI:
      // See http://java.sun.com/products/jdk/1.2/docs/guide/jni/spec/design.doc.html#615
      {
        char *p = buffer;
        char *bufend = buffer + buflen - 1;

	// Don't include the leading ( of a function sig in the mangle.
	if (*name == '(')
	  name++;

        while (*name && p < bufend)
          {
            unsigned char c = *name++;
            unsigned char alt;

            if (c & 0x80)
              {
                unicode full;
                if ((c & 0xE0) == 0xC0)
                  full = ((unicode)c & 0x1f) << 6;
                else
                  {
                    full = ((unicode)c & 0xf) << 12;
                    full |= (*name++ & 0x3f) << 6;
                  }
                full |= *name++ & 0x3f;

                if (mangleUnicodeChar(full, p, bufend+1) == 0)
                  break;
                p += 6;
              }
            else if (c == '/')
              *p++ = '_';
            else if ((alt = '1', c == '_')
                     || (alt = '2', c == ';')
                     || (alt = '3', c == '['))
              {
                if (p+1 >= bufend)
                  break;
                *p++ = '_';
                *p++ = alt;
              }
            else
            *p++ = c;
          }

        *p = '\0';
        break;
      }
    }

  return strlen(buffer);
}
