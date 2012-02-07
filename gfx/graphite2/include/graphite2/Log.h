/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

    Alternatively, the contents of this file may be used under the terms
    of the Mozilla Public License (http://mozilla.org/MPL) or the GNU
    General Public License, as published by the Free Software Foundation,
    either version 2 of the License or (at your option) any later version.
*/
#pragma once

#include <graphite2/Types.h>
#include <stdio.h>

typedef enum {
    GRLOG_NONE = 0x0,
    GRLOG_FACE = 0x01,
    GRLOG_SEGMENT = 0x02,
    GRLOG_PASS = 0x04,
    GRLOG_CACHE = 0x08,
    
    GRLOG_OPCODE = 0x80,
    GRLOG_ALL = 0xFF
} GrLogMask;

// If startGraphiteLogging returns true, logging is enabled and the FILE handle
// will be closed by graphite when stopGraphiteLogging is called.
#ifdef __cplusplus
extern "C"
{
#endif

GR2_API bool graphite_start_logging(FILE * logFile, GrLogMask mask);		//may not do anthing if disabled in the implementation of the engine.
GR2_API void graphite_stop_logging();

#ifdef __cplusplus
}
#endif
