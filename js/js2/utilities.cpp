// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#include <new>
#include <iomanip>
#include "utilities.h"

#ifdef WIN32
 #include <windows.h>
#endif

#ifdef XP_MAC
 #include <cstdarg>
 #include <Types.h>
#endif

namespace JS = JavaScript;


//
// Assertions
//


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
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, line);
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



//
// Bit manipulation
//


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



//
// C++ I/O
//

JS::SaveFormat::SaveFormat(ostream &out): o(out), flags(out.flags()), fill(out.fill()) {}

JS::SaveFormat::~SaveFormat()
{
	o.flags(flags);
	o.fill(fill);
}


void JS::showChar(ostream &out, char16 ch)
{
	SaveFormat sf(out);
	out << '[' << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << (uint16)ch << ']';
}


void JS::showString(ostream &out, const String &str)
{
	showString(out, str.begin(), str.end());
}


//
// Static Initializers
//


static void jsNewHandler()
{
    std::bad_alloc outOfMemory;
    throw outOfMemory;
}


struct InitUtilities
{
	InitUtilities() {std::set_new_handler(&jsNewHandler);}
};
InitUtilities initUtilities;
