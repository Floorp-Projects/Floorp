/*
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_PRIVATE_H
#define HB_PRIVATE_H

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "hb-common.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* We only use these two for debug output.  However, the debug code is
 * always seen by the compiler (and optimized out in non-debug builds.
 * If including these becomes a problem, we can start thinking about
 * someway around that. */
#include <stdio.h>
#include <errno.h>

HB_BEGIN_DECLS


/* Essentials */

#ifndef NULL
# define NULL ((void *) 0)
#endif

#undef FALSE
#define FALSE 0

#undef TRUE
#define TRUE 1


/* Basics */

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#undef  ARRAY_LENGTH
#define ARRAY_LENGTH(__array) ((signed int) (sizeof (__array) / sizeof (__array[0])))

#define HB_STMT_START do
#define HB_STMT_END   while (0)

#define _ASSERT_STATIC1(_line, _cond) typedef int _static_assert_on_line_##_line##_failed[(_cond)?1:-1]
#define _ASSERT_STATIC0(_line, _cond) _ASSERT_STATIC1 (_line, (_cond))
#define ASSERT_STATIC(_cond) _ASSERT_STATIC0 (__LINE__, (_cond))


/* Misc */


#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#define _HB_BOOLEAN_EXPR(expr) ((expr) ? 1 : 0)
#define likely(expr) (__builtin_expect (_HB_BOOLEAN_EXPR(expr), 1))
#define unlikely(expr) (__builtin_expect (_HB_BOOLEAN_EXPR(expr), 0))
#else
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#endif

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

#if __GNUC__ >= 3
#define HB_PURE_FUNC	__attribute__((pure))
#define HB_CONST_FUNC	__attribute__((const))
#else
#define HB_PURE_FUNC
#define HB_CONST_FUNC
#endif
#if __GNUC__ >= 4
#define HB_UNUSED	__attribute__((unused))
#else
#define HB_UNUSED
#endif

#ifndef HB_INTERNAL
# define HB_INTERNAL __attribute__((__visibility__("hidden")))
#endif


#if (defined(__WIN32__) && !defined(__WINE__)) || defined(_MSC_VER)
#define snprintf _snprintf
#endif

#ifdef _MSC_VER
#undef inline
#define inline __inline
#endif

#ifdef __STRICT_ANSI__
#undef inline
#define inline __inline__
#endif


#if __GNUC__ >= 3
#define HB_FUNC __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define HB_FUNC __FUNCSIG__
#else
#define HB_FUNC __func__
#endif


/* Return the number of 1 bits in mask. */
static inline HB_CONST_FUNC unsigned int
_hb_popcount32 (uint32_t mask)
{
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
  return __builtin_popcount (mask);
#else
  /* "HACKMEM 169" */
  register uint32_t y;
  y = (mask >> 1) &033333333333;
  y = mask - y - ((y >>1) & 033333333333);
  return (((y + (y >> 3)) & 030707070707) % 077);
#endif
}

/* Returns the number of bits needed to store number */
static inline HB_CONST_FUNC unsigned int
_hb_bit_storage (unsigned int number)
{
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(__OPTIMIZE__)
  return likely (number) ? (sizeof (unsigned int) * 8 - __builtin_clz (number)) : 0;
#else
  register unsigned int n_bits = 0;
  while (number) {
    n_bits++;
    number >>= 1;
  }
  return n_bits;
#endif
}

/* Returns the number of zero bits in the least significant side of number */
static inline HB_CONST_FUNC unsigned int
_hb_ctz (unsigned int number)
{
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(__OPTIMIZE__)
  return likely (number) ? __builtin_ctz (number) : 0;
#else
  register unsigned int n_bits = 0;
  if (unlikely (!number)) return 0;
  while (!(number & 1)) {
    n_bits++;
    number >>= 1;
  }
  return n_bits;
#endif
}

/* Type of bsearch() / qsort() compare function */
typedef int (*hb_compare_func_t) (const void *, const void *);


/* We need external help for these */

#ifdef HAVE_GLIB

#include <glib.h>

typedef int hb_atomic_int_t;
#define hb_atomic_int_fetch_and_add(AI, V)	g_atomic_int_exchange_and_add (&(AI), V)
#define hb_atomic_int_get(AI)			g_atomic_int_get (&(AI))
#define hb_atomic_int_set(AI, V)		g_atomic_int_set (&(AI), V)

typedef GStaticMutex hb_mutex_t;
#define HB_MUTEX_INIT			G_STATIC_MUTEX_INIT
#define hb_mutex_init(M)		g_static_mutex_init (&M)
#define hb_mutex_lock(M)		g_static_mutex_lock (&M)
#define hb_mutex_trylock(M)		g_static_mutex_trylock (&M)
#define hb_mutex_unlock(M)		g_static_mutex_unlock (&M)

#else

#ifdef _MSC_VER
#pragma message(__LOC__"Could not find any system to define platform macros, library will NOT be thread-safe")
#else
#warning "Could not find any system to define platform macros, library will NOT be thread-safe"
#endif

typedef int hb_atomic_int_t;
#define hb_atomic_int_fetch_and_add(AI, V)	((AI) += (V), (AI) - (V))
#define hb_atomic_int_get(AI)			(AI)
#define hb_atomic_int_set(AI, V)		HB_STMT_START { (AI) = (V); } HB_STMT_END

typedef int hb_mutex_t;
#define HB_MUTEX_INIT				0
#define hb_mutex_init(M)			HB_STMT_START { (M) = 0; } HB_STMT_END
#define hb_mutex_lock(M)			HB_STMT_START { (M) = 1; } HB_STMT_END
#define hb_mutex_trylock(M)			((M) = 1, 1)
#define hb_mutex_unlock(M)			HB_STMT_START { (M) = 0; } HB_STMT_END

#endif


/* Big-endian handling */

#define hb_be_uint16(v)		((uint16_t) ((((const uint8_t *)&(v))[0] << 8) + (((const uint8_t *)&(v))[1])))

#define hb_be_uint16_put(v,V)	HB_STMT_START { v[0] = (V>>8); v[1] = (V); } HB_STMT_END
#define hb_be_uint16_get(v)	(uint16_t) ((v[0] << 8) + v[1])
#define hb_be_uint16_cmp(a,b)	(a[0] == b[0] && a[1] == b[1])

#define hb_be_uint32_put(v,V)	HB_STMT_START { v[0] = (V>>24); v[1] = (V>>16); v[2] = (V>>8); v[3] = (V); } HB_STMT_END
#define hb_be_uint32_get(v)	(uint32_t) ((v[0] << 24) + (v[1] << 16) + (v[2] << 8) + v[3])
#define hb_be_uint32_cmp(a,b)	(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3])


/* Debug */

#ifndef HB_DEBUG
#define HB_DEBUG 0
#endif

static inline hb_bool_t /* always returns TRUE */
_hb_trace (const char *what,
	   const char *function,
	   const void *obj,
	   unsigned int depth,
	   unsigned int max_depth)
{
  (void) ((depth < max_depth) && fprintf (stderr, "%s(%p) %-*d-> %s\n", what, obj, depth, depth, function));
  return TRUE;
}


#include "hb-object-private.h"


HB_END_DECLS

#endif /* HB_PRIVATE_H */
