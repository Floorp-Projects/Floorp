/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef js_cpucfg___
#define js_cpucfg___

#include "jsosdep.h"

#ifdef XP_MAC
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   2L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L
#endif /* XP_MAC */

#ifdef _WIN32
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L
#endif /* _WIN32 */

#if defined(_WINDOWS) && !defined(_WIN32) /* WIN16 */
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    2L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     16L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    4L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     2L
#define JS_ALIGN_OF_LONG    2L
#define JS_ALIGN_OF_INT64   2L
#define JS_ALIGN_OF_FLOAT   2L
#define JS_ALIGN_OF_DOUBLE  2L
#define JS_ALIGN_OF_POINTER 2L
#define JS_ALIGN_OF_WORD    2L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L
#endif /* defined(_WINDOWS) && !defined(_WIN32) */

#ifdef XP_UNIX

#ifdef AIXV3
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(BSDI)
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(HPUX)
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(IRIX)
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(linux)

#ifdef __powerpc__
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN    1
#elif __i386__
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN
#elif __alpha__
#define IS_BIG_ENDIAN    1
#undef  IS_LITTLE_ENDIAN
#elif __sparc__
#define IS_BIG_ENDIAN    1
#undef  IS_LITTLE_ENDIAN
#else
#error "linux cpu architecture not supported by jscpucfg.h"
#endif

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(OSF1)
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   8L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   8L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    64L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    64L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   6L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   6L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    8L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 8L
#define JS_ALIGN_OF_WORD    8L

#define JS_BYTES_PER_WORD_LOG2   3L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  0L


#elif defined(SOLARIS)

#ifdef i386
/* PC-based */
#undef  IS_BIG_ENDIAN
#define IS_LITTLE_ENDIAN 1
#else
/* Sparc-based */
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1
#endif

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#ifdef i386
#undef JS_ALIGN_OF_INT64
#undef JS_ALIGN_OF_DOUBLE
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_DOUBLE  4L
#endif

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(SUNOS4)
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(SNI)
#undef  IS_LITTLE_ENDIAN
#define IS_BIG_ENDIAN 1

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   8L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  8L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(SONY)
/* Don't have it */

#elif defined(NECSVR4)
/* Don't have it */

#elif defined(SCO)
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L


#elif defined(UNIXWARE)
#define IS_LITTLE_ENDIAN 1
#undef  IS_BIG_ENDIAN

#define JS_BYTES_PER_BYTE   1L
#define JS_BYTES_PER_SHORT  2L
#define JS_BYTES_PER_INT    4L
#define JS_BYTES_PER_INT64  8L
#define JS_BYTES_PER_LONG   4L
#define JS_BYTES_PER_FLOAT  4L
#define JS_BYTES_PER_DOUBLE 8L
#define JS_BYTES_PER_WORD   4L
#define JS_BYTES_PER_DWORD  8L

#define JS_BITS_PER_BYTE    8L
#define JS_BITS_PER_SHORT   16L
#define JS_BITS_PER_INT     32L
#define JS_BITS_PER_INT64   64L
#define JS_BITS_PER_LONG    32L
#define JS_BITS_PER_FLOAT   32L
#define JS_BITS_PER_DOUBLE  64L
#define JS_BITS_PER_WORD    32L

#define JS_BITS_PER_BYTE_LOG2   3L
#define JS_BITS_PER_SHORT_LOG2  4L
#define JS_BITS_PER_INT_LOG2    5L
#define JS_BITS_PER_INT64_LOG2  6L
#define JS_BITS_PER_LONG_LOG2   5L
#define JS_BITS_PER_FLOAT_LOG2  5L
#define JS_BITS_PER_DOUBLE_LOG2 6L
#define JS_BITS_PER_WORD_LOG2   5L

#define JS_ALIGN_OF_SHORT   2L
#define JS_ALIGN_OF_INT     4L
#define JS_ALIGN_OF_LONG    4L
#define JS_ALIGN_OF_INT64   4L
#define JS_ALIGN_OF_FLOAT   4L
#define JS_ALIGN_OF_DOUBLE  4L
#define JS_ALIGN_OF_POINTER 4L
#define JS_ALIGN_OF_WORD    4L

#define JS_BYTES_PER_WORD_LOG2   2L
#define JS_BYTES_PER_DWORD_LOG2  3L
#define PR_WORDS_PER_DWORD_LOG2  1L

#endif
#endif /* XP_UNIX */

#endif /* js_cpucfg___ */
