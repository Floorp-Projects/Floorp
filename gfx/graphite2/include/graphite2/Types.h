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

#include <stddef.h>

typedef unsigned char   gr_uint8;
typedef gr_uint8        gr_byte;
typedef signed char     gr_int8;
typedef unsigned short  gr_uint16;
typedef short           gr_int16;
typedef unsigned int    gr_uint32;
typedef int             gr_int32;

enum gr_encform {
  gr_utf8 = 1/*sizeof(uint8)*/, gr_utf16 = 2/*sizeof(uint16)*/, gr_utf32 = 4/*sizeof(uint32)*/
};

// Definitions for library publicly exported symbols
#if defined _WIN32 || defined __CYGWIN__
  #if defined GRAPHITE2_STATIC
    #define GR2_API
  #elif defined GRAPHITE2_EXPORTING
    #if defined __GNUC__
      #define GR2_API    __attribute__((dllexport))
    #else
      #define GR2_API    __declspec(dllexport)
    #endif
  #else
    #if defined __GNUC__
      #define GR2_API    __attribute__((dllimport))
    #else
      #define GR2_API    __declspec(dllimport)
    #endif
  #endif
  #define GR2_LOCAL
#else
  #if __GNUC__ >= 4
    #define GR2_API      __attribute__ ((visibility("default")))
    #define GR2_LOCAL       __attribute__ ((visibility("hidden")))
  #else
    #define GR2_API
    #define GR2_LOCAL
  #endif
#endif
