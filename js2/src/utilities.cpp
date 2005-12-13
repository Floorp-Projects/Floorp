/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "utilities.h"
#ifdef DEBUG
#    include <cstdio>
#    ifdef XP_MAC
#        include <cstdarg>
#        include <Types.h>
#    endif
#    ifdef WIN32
#        include <stdlib.h>
#        include <windows.h>
#    endif
#    ifdef XP_UNIX
#        include <stdlib.h>
#    endif
#endif

namespace JS = JavaScript;


#ifdef DEBUG
#ifdef XP_MAC
// PStrFromCStr converts the source C string to a destination
// pascal string as it copies. The dest string will
// be truncated to fit into an Str255 if necessary.
// If the C String pointer is NULL, the pascal string's length is
// set to zero.
static void PStrFromCStr(const char* src, Str255 &dst)
{
    int length = 0;

    if (src)
    {
        char *p = reinterpret_cast<char *>(&dst[1]);
        int spaceLeft = 255;
        char ch;
        while ((ch = *src++) != 0) {
            *p++ = ch;
            if (--spaceLeft == 0)
                break;
        }
        length = 255 - spaceLeft;
    }
    dst[0] = (uchar)length;
}

static void jsdebugstr(const char *debuggerMsg)
{
    Str255 pStr;

    PStrFromCStr(debuggerMsg, pStr);
    DebugStr(pStr);
}

static void dprintf(const char *format, ...)
{
    std::va_list ap;
    char buffer[4096];

    va_start(ap, format);
    std::vsprintf(buffer, format, ap);
    va_end(ap);

    jsdebugstr(buffer);
}
#endif   /* XP_MAC */


void JS::Assert(const char *s, const char *file, int line)
{
#if defined(XP_UNIX) || defined(XP_OS2)
# ifdef stderr
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, line);
#else
    fprintf(std::stderr, "Assertion failure: %s, at %s:%d\n", s, file, line);
#endif
#endif
#ifdef XP_MAC
    dprintf("Assertion failure: %s, at %s:%d\n", s, file, line);
#endif
#ifdef WIN32
    DebugBreak();
#endif
#ifndef XP_MAC
    abort();
#endif
}
#endif /* DEBUG */


// Return lg2 of the least power of 2 greater than or equal to n.
// Return 0 if n is 0 or 1.
uint JS::ceilingLog2(uint32 n)
{
    uint log2 = 0;

    if (n & (n-1))
        log2++;
    if (n >> 16)
        log2 += 16, n >>= 16;
    if (n >> 8)
        log2 += 8, n >>= 8;
    if (n >> 4)
        log2 += 4, n >>= 4;
    if (n >> 2)
        log2 += 2, n >>= 2;
    if (n >> 1)
        log2++;
    return log2;
}


// Return lg2 of the greatest power of 2 less than or equal to n.
// This really just finds the highest set bit in the word.
// Return 0 if n is 0 or 1.
uint JS::floorLog2(uint32 n)
{
    uint log2 = 0;

    if (n >> 16)
        log2 += 16, n >>= 16;
    if (n >> 8)
        log2 += 8, n >>= 8;
    if (n >> 4)
        log2 += 4, n >>= 4;
    if (n >> 2)
        log2 += 2, n >>= 2;
    if (n >> 1)
        log2++;
    return log2;
}
