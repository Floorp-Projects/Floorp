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

#ifndef FUNDAMENTALS_H
#define FUNDAMENTALS_H

#include <stddef.h>
#include "Exports.h"
#include "CatchAssert.h"

#include "prlong.h"
#include "prlog.h"

#if defined(DEBUG) || defined(DEBUG_LOG)
 #include <stdio.h>
#endif

#ifndef USE_GENERATEDFILES
#define USE_GENERATEDFILES 1
#endif

#ifdef NO_NSPR
#include "Nonspr.h"
#endif


// Turn this on to have the internal addr values be two-word structures that permit
// relocations of the generated code's references to objects.
#ifndef BATCH_COMPILATION
 #define BATCH_COMPILATION 1
#endif

// DEBUG_LOG controls whether we compile in the routines to disassemble and dump
// internal compiler state to a file or stdout.  These are useful even in nondebug
// builds but should not be present in release builds. 
#if defined(DEBUG) && !defined(DEBUG_LOG)
 #define DEBUG_LOG
#endif


// ----------------------------------------------------------------------------
// Compiler idiosyncrasies

// Metrowerks doesn't recognize the typename keyword yet.
#ifdef __MWERKS__
 #define typename
#endif

// MANUAL_TEMPLATES means we instantiate all templates manually.
#ifdef __GNUC__
 #define MANUAL_TEMPLATES
#endif

//
// Compilers align bools and enums inside structures differently.
// Some are nice enough to allocate only as many bytes as needed;
// others always store these as entire words.  On the latter we need to use
// bit fields to persuade them to store these as bytes or halfwords.
// We don't use bit fields on the former because:
//  1.  Some compilers generate much less efficient code for bit fields,
//  2.  Some compilers (esp. Metrowerks) generate buggy code for bit fields.
//
// Use ENUM_8 after an 8-bit enum field declaration in a structure
// Use ENUM_16 after a 16-bit enum field declaration in a structure
// Use BOOL_8 after an 8-bit bool field declaration in a structure
// Use CHAR_8 after a char field declaration (if it should be packed with one of the above)
// Use SHORT_16 after a short field declaration (if it should be packed with one of the above)
//
// The last two macros are only necessary when packing a char or a short in the same
// word as a bool or enum; some compilers don't put fields with bit widths in the
// same word as fields without bit widths.
//
#if defined __MWERKS__ || defined WIN32
 #define ENUM_8
 #define ENUM_16
 #define BOOL_8
 #define CHAR_8
 #define SHORT_16
#else
 #define ENUM_8 :8
 #define ENUM_16 :16
 #define BOOL_8 :8
 #define CHAR_8 :8
 #define SHORT_16 :16
#endif


// Use the following to make 64-bit integer constants
#ifdef WIN32
 #define CONST64(val) val ## i64
#else
 #define CONST64(val) val ## LL
#endif


// ----------------------------------------------------------------------------
// Universal types

#ifdef __GNUC__
 typedef unsigned int uint;
#endif
typedef char *ptr;
typedef const char *cptr;

typedef PRInt8 Int8;
typedef PRUint8 Uint8;
typedef PRInt16 Int16;
typedef PRUint16 Uint16;
typedef PRInt32 Int32;
typedef PRUint32 Uint32;
typedef PRInt64 Int64;
typedef PRUint64 Uint64;

typedef float Flt32;		// 32-bit IEEE floating point
typedef PRFloat64 Flt64;	// 64-bit IEEE floating point

// ----------------------------------------------------------------------------
// Debugging

// Use DEBUG_ONLY around function arguments that are only used in debug builds.
// This avoids unused variable warnings in nondebug builds.
#ifdef DEBUG
 #define DEBUG_ONLY(arg) arg
 #define NONDEBUG_ONLY(arg)
#else
 #define DEBUG_ONLY(arg)
 #define NONDEBUG_ONLY(arg) arg
#endif

#ifdef DEBUG_LOG
 #define DEBUG_LOG_ONLY(arg) arg
#else
 #define DEBUG_LOG_ONLY(arg)
#endif

#ifdef DEBUG
 #define trespass(string) { fprintf(stderr, "%s", string); assert(false); }
#else
 #define trespass(string) assert(!string)
#endif

// ----------------------------------------------------------------------------
// Tiny pieces of STL
// These can be replaced by the real STL if it happens to be available
// (but make sure that the real STL has these as inlines!).

#ifndef STL
template<class In, class Out>
inline Out copy(In srcBegin, In srcEnd, Out dst)
{
	while (srcBegin != srcEnd) {
		*dst = *srcBegin;
		++srcBegin;
		++dst;
	}
	return dst;
}

template<class Out, class T>
inline void fill(Out first, Out last, T value)
{
	while (first != last) 
		*first++ = value;
}

template<class Out, class T>
inline void fill_n(Out first, Uint32 n, T value)
{
	while (n--) 
		*first++ = value;
}
#endif


// ----------------------------------------------------------------------------
// Non-STL useful templates

template<class In, class Out>
inline Out move(In srcBegin, In srcEnd, Out dst)
{
	while (srcBegin != srcEnd) {
		dst->move(*srcBegin);
		++srcBegin;
		++dst;
	}
	return dst;
}


// An abstract base class for representing closures of functions with no arguments.
template<class Result>
struct Function0
{
	virtual Result operator()() = 0;
};

// An abstract base class for representing closures of functions with one argument.
template<class Result, class Arg>
struct Function1
{
	virtual Result operator()(Arg arg) = 0;
};

// An abstract base class for representing closures of functions with two arguments.
template<class Result, class Arg1, class Arg2>
struct Function2
{
	virtual Result operator()(Arg1 arg1, Arg2 arg2) = 0;
};

// An abstract base class for representing closures of functions with three arguments.
template<class Result, class Arg1, class Arg2, class Arg3>
struct Function3
{
	virtual Result operator()(Arg1 arg1, Arg2 arg2, Arg3 arg3) = 0;
};


// ----------------------------------------------------------------------------
// Pooled memory allocation

inline void *operator new(size_t, void* ptr) {return ptr;}

class Pool;

NS_EXTERN void *operator new(size_t size, Pool &pool);
#if !defined __MWERKS__ && !defined WIN32
	NS_EXTERN void *operator new[](size_t size, Pool &pool);
	NS_EXTERN void *operator new[](size_t size);
#endif
#endif
