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

#ifndef prtypes_h___
#define prtypes_h___
/*
 * Fundamental types and related macros.
 */
#include <stddef.h>
#include "prcpucfg.h"

/*
 * Bitsize-explicit integer types.
 */
typedef unsigned char   uint8;
typedef signed char     int8;
typedef unsigned short  uint16;
typedef signed short    int16;
#if PR_BITS_PER_WORD == 64
typedef unsigned int    uint32;
typedef signed int      int32;
#else
typedef unsigned long   uint32;
typedef signed long     int32;
#endif

/*
 * Use these to get native int types guaranteed to be at least 16 bits wide.
 */
typedef int             intN;
typedef unsigned int    uintN;

/*
 * IEEE single- and double-precision.
 */
typedef float           float32;
typedef double          float64;

/*
 * A prword is an integer that is the same size as a void pointer.
 */
typedef long            prword;
typedef unsigned long   pruword;

/*
 * Use PRBool for variables and parameter types.  Use PRPackedBool within
 * structs where bitfields are not desirable but minimum overhead matters.
 *
 * Use PR_FALSE and PR_TRUE for clarity of target type in assignments and
 * actual args.  Use 'if (bool)', 'while (!bool)', '(bool ? x : y)', etc.
 * to test Booleans just as you would C int-valued conditions.
 */
typedef enum { PR_FALSE, PR_TRUE } PRBool;
typedef uint8 PRPackedBool;
typedef enum { PR_FAILURE = -1, PR_SUCCESS = 0, PR_PENDING_INTERRUPT = 1 } PRStatus;
typedef unsigned int PRUint32;
typedef unsigned int PRUintn;
typedef int PRIntn;
typedef int PRInt32;
typedef short PRUint16;

/*
 * Common struct typedefs.
 */
typedef struct PRArena      PRArena;
typedef struct PRArenaPool  PRArenaPool;
typedef struct PRCList      PRCList;
typedef struct PRHashEntry  PRHashEntry;
typedef struct PRHashTable  PRHashTable;
typedef struct PRTime       PRTime;

/************************************************************************/

#if defined(XP_PC)

#if defined(_WIN32)

#define PR_PUBLIC_API(__x)      _declspec(dllexport) __x
#define PR_IMPORT_API(__x)      _declspec(dllimport) __x
#define PR_PUBLIC_DATA(__x)     _declspec(dllexport) __x
#define PR_IMPORT_DATA(__x)     _declspec(dllimport) __x
#define PR_CALLBACK
#define PR_STATIC_CALLBACK(__x)	static __x
#define PR_EXTERN(__type) extern _declspec(dllexport) __type
#define PR_IMPLEMENT(__type) _declspec(dllexport) __type

#elif defined(__WATCOMC__) /* WIN386 */

#define PR_PUBLIC_API(__x)      __x
#define PR_IMPORT_API(__x)      __x
#define PR_PUBLIC_DATA(__x)     __x
#define PR_IMPORT_DATA(__x)     __x
#define PR_CALLBACK
#define PR_STATIC_CALLBACK(__x) static __x

#else  /* _WIN16 */

#define PR_PUBLIC_API(__x)      __x _cdecl _loadds _export
#define PR_IMPORT_API(__x)      PR_PUBLIC_API(__x)
#define PR_PUBLIC_DATA(__x)     __x
#define PR_IMPORT_DATA(__x)     __x

#if defined(_WINDLL)
#define PR_CALLBACK             _cdecl _loadds
#define PR_STATIC_CALLBACK(__x)	static __x PR_CALLBACK
#else
#define PR_CALLBACK             _cdecl __export
#define PR_STATIC_CALLBACK(__x)	__x PR_CALLBACK
#endif

#endif /* _WIN16 */

#else  /* Mac or Unix */

#define PR_PUBLIC_API(__x)      __x
#define PR_IMPORT_API(__x)      __x
#define PR_PUBLIC_DATA(__x)     __x
#define PR_IMPORT_DATA(__x)     __x
#define PR_CALLBACK
#define PR_STATIC_CALLBACK(__x)	static __x
#define PR_EXTERN(__type) extern __type
#define PR_IMPLEMENT(__type) __type

#endif /* Mac or Unix */

/************************************************************************/

/*
 * Macro body brackets so that macros with compound statement definitions
 * behave syntactically more like functions when called.
 */
#define PR_BEGIN_MACRO		do {
#define PR_END_MACRO		} while (0)

/*
 * Macro shorthands for conditional C++ extern block delimiters.
 */
#ifdef __cplusplus
#define PR_BEGIN_EXTERN_C	extern "C" {
#define PR_END_EXTERN_C		}
#else
#define PR_BEGIN_EXTERN_C
#define PR_END_EXTERN_C
#endif

/*
 * Bit masking macros.  XXX n must be <= 31 to be portable
 */
#define PR_BIT(n)	((pruword)1 << (n))
#define PR_BITMASK(n)	(PR_BIT(n) - 1)

/* Commonly used macros */
#define PR_ROUNDUP(x,y) ((((x)+((y)-1))/(y))*(y))
#define PR_MIN(x,y)     ((x)<(y)?(x):(y))
#define PR_MAX(x,y)     ((x)>(y)?(x):(y))

/*
 * Compute the log of the least power of 2 greater than or equal to n.
 */
extern PR_PUBLIC_API(int32)
PR_CeilingLog2(uint32 n);

/************************************************************************/

/*
 * Prototypes and macros used to make up for deficiencies in ANSI environments
 * that we have found.
 *
 * Since we do not wrap <stdlib.h> and all the other standard headers, authors
 * of portable code will not know in general that they need these definitions.
 * Instead of requiring these authors to find the dependent uses in their code
 * and take the following steps only in those C files, we take steps once here
 * for all C files.
 */
#ifdef SUNOS4
# include "sunos4.h"
#endif

#endif /* prtypes_h___ */
