/*
 * Copyright © 2007  Chris Wilson
 * Copyright © 2009,2010  Red Hat, Inc.
 * Copyright © 2011,2012  Google, Inc.
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
 * Contributor(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_ATOMIC_PRIVATE_HH
#define HB_ATOMIC_PRIVATE_HH

#include "hb-private.hh"


/* atomic_int */

/* We need external help for these */

#if 0


#elif !defined(HB_NO_MT) && defined(_MSC_VER) && _MSC_VER >= 1600

#include <intrin.h>
/* On x86, _InterlockedCompareExchangePointer is a macro defined in concrt.h */
#include <concrt.h>

typedef long hb_atomic_int_t;
#define hb_atomic_int_add(AI, V)	_InterlockedExchangeAdd (&(AI), (V))

#define hb_atomic_ptr_get(P)		(MemoryBarrier (), (void *) *(P))
#define hb_atomic_ptr_cmpexch(P,O,N)	(_InterlockedCompareExchangePointer ((void **) (P), (void *) (N), (void *) (O)) == (void *) (O))


#elif !defined(HB_NO_MT) && defined(__APPLE__)

#include <libkern/OSAtomic.h>

typedef int32_t hb_atomic_int_t;
#define hb_atomic_int_add(AI, V)	(OSAtomicAdd32Barrier ((V), &(AI)) - (V))

#define hb_atomic_ptr_get(P)		(OSMemoryBarrier (), (void *) *(P))
#define hb_atomic_ptr_cmpexch(P,O,N)	OSAtomicCompareAndSwapPtrBarrier ((void *) (O), (void *) (N), (void **) (P))


#elif !defined(HB_NO_MT) && defined(HAVE_INTEL_ATOMIC_PRIMITIVES)

typedef int hb_atomic_int_t;
#define hb_atomic_int_add(AI, V)	__sync_fetch_and_add (&(AI), (V))

#define hb_atomic_ptr_get(P)		(void *) (__sync_synchronize (), *(P))
#define hb_atomic_ptr_cmpexch(P,O,N)	__sync_bool_compare_and_swap ((P), (O), (N))

#elif !defined(HB_NO_MT) && defined(HAVE_GLIB)

#include <glib.h>
typedef int hb_atomic_int_t;
#if GLIB_CHECK_VERSION(2,29,5)
#define hb_atomic_int_add(AI, V)	g_atomic_int_add (&(AI), (V))
#else
#define hb_atomic_int_add(AI, V)	g_atomic_int_exchange_and_add (&(AI), (V))
#endif

#define hb_atomic_ptr_get(P)		g_atomic_pointer_get (P)
#define hb_atomic_ptr_cmpexch(P,O,N)	g_atomic_pointer_compare_and_exchange ((void **) (P), (void *) (O), (void *) (N))


#elif !defined(HB_NO_MT)

#define HB_ATOMIC_INT_NIL 1 /* Warn that fallback implementation is in use. */
typedef volatile int hb_atomic_int_t;
#define hb_atomic_int_add(AI, V)	(((AI) += (V)) - (V))

#define hb_atomic_ptr_get(P)		((void *) *(P))
#define hb_atomic_ptr_cmpexch(P,O,N)	(* (void * volatile *) (P) == (void *) (O) ? (* (void * volatile *) (P) = (void *) (N), true) : false)


#else /* HB_NO_MT */

typedef int hb_atomic_int_t;
#define hb_atomic_int_add(AI, V)	(((AI) += (V)) - (V))

#define hb_atomic_ptr_get(P)		((void *) *(P))
#define hb_atomic_ptr_cmpexch(P,O,N)	(* (void **) (P) == (void *) (O) ? (* (void **) (P) = (void *) (N), true) : false)

#endif

/* TODO Add tracing. */

#endif /* HB_ATOMIC_PRIVATE_HH */
