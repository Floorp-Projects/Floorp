/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
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

/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#ifndef __SMPRIV__
#define __SMPRIV__

#include "prtypes.h"
#include "prlog.h"      /* for PR_ASSERT */
#ifdef XP_MAC
#include "pprthred.h"   /* for PR_InMonitor */
#else
#include "private/pprthred.h"
#endif

PR_BEGIN_EXTERN_C

/*******************************************************************************
 * Build Settings
 ******************************************************************************/

#ifdef DEBUG
#ifndef SM_STATS
#define SM_STATS
#endif
#ifndef SM_DUMP
#define SM_DUMP
#endif
#endif

/*******************************************************************************
 * Macros
 ******************************************************************************/

#define SM_BEGIN_EXTERN_C               PR_BEGIN_EXTERN_C
#define SM_END_EXTERN_C                 PR_END_EXTERN_C
#define SM_EXTERN(ResultType)           PR_EXTERN(ResultType)
#define SM_IMPLEMENT(ResultType)        PR_IMPLEMENT(ResultType)
#define SM_MAX_VALUE(Type)              ((1 << (sizeof(Type) * 8)) - 1)

#ifdef DEBUG
SM_EXTERN(void) SM_Assert(const char* message, const char* file, int line);
#define SM_ASSERT(test)                 ((test) ? ((void)0) : SM_Assert(#test , __FILE__, __LINE__))
#else
#define SM_ASSERT(test)                 ((void)0)
#endif

/* Use this "ensure" stuff to verify type-correctness of macros. It all 
   becomes a no-op in non-debug mode. */
#ifdef DEBUG
#define SM_DECLARE_ENSURE(Type)         SM_EXTERN(Type*) sm_Ensure##Type(Type* x);
#define SM_IMPLEMENT_ENSURE(Type)       SM_IMPLEMENT(Type*) sm_Ensure##Type(Type* x) { return x; }
#define SM_ENSURE(Type, x)              sm_Ensure##Type(x)
#else
#define SM_DECLARE_ENSURE(Type)         /* no-op */
#define SM_IMPLEMENT_ENSURE(Type)       /* no-op */
#define SM_ENSURE(Type, x)              (x)
#endif

/*******************************************************************************
 * Pointer Encryption
 *
 * These special pointer manipulation routines are used in attempt to guard
 * against unsafe pointer assigments -- failure to go through the write barrier.
 * To guarantee that assignments go through the write barrier, we encrypt all
 * pointers that live in the heap, so that attempts to use them without these
 * macros will fail.
 *
 * These macros are private and should never be used directly by a user program.
 ******************************************************************************/

#ifdef SM_CHECK_PTRS

#define SM_POINTER_KEY          0xFADEDEAD      /* must be odd */
#define SM_ENCRYPT(addr)        ((addr) ? ((PRUword)(((PRUword)(addr)) ^ SM_POINTER_KEY)) : (PRUword)0)
#define SM_DECRYPT(addr)        ((addr) ? ((void*)(((PRUword)(addr)) ^ SM_POINTER_KEY)) : NULL)

#else /* !SM_CHECK_PTRS */

#define SM_ENCRYPT(addr)        ((PRUword)(addr))
#define SM_DECRYPT(addr)        ((void*)(addr))

#endif /* !SM_CHECK_PTRS */

/*******************************************************************************
 * Alignment Macros
 ******************************************************************************/

#define SM_ALIGN(p, nBits)      ((PRWord)(p) & ~((1 << nBits) - 1))
#define SM_IS_ALIGNED(p, nBits) ((PRWord)(p) == SM_ALIGN(p, nBits))

#ifdef IS_64
#define SM_POINTER_ALIGNMENT    4
#else
#define SM_POINTER_ALIGNMENT    2
#endif

/******************************************************************************/

#include <string.h> /* for memset */
#include <stdio.h>

#if defined(DEBUG) || defined(SM_DUMP)
extern FILE* sm_DebugOutput;
#endif

#ifdef DEBUG

#define DBG_MEMSET(dest, pattern, size)         memset(dest, pattern, size)

#define SM_PAGE_ALLOC_PATTERN   0xCB
#define SM_PAGE_FREE_PATTERN    0xCD
#define SM_UNUSED_PATTERN       0xDD
#define SM_MALLOC_PATTERN       0xEB
#define SM_FREE_PATTERN         0xED
#define SM_GC_FREE_PATTERN      0xFD

#else

#define DBG_MEMSET(dest, pattern, size)         ((void)0)

#endif

/*******************************************************************************
 * "Unrolled" while loops for minimizing loop overhead
 ******************************************************************************/

#define SM_UNROLL_WHILE
#ifdef  SM_UNROLL_WHILE

#define SM_UNROLLED_WHILE(count, body)             \
{                                                  \
    PRUword _cnt = (PRUword)(count);               \
    int _rem = _cnt & 7;                           \
    _cnt -= _rem;                                  \
    /* First time through, use switch to jump */   \
    /* right into the middle of the while loop. */ \
    switch (_rem) {                                \
        while (_cnt > 0) {                         \
            _cnt -= 8;                             \
            body;                                  \
          case 7:                                  \
            body;                                  \
          case 6:                                  \
            body;                                  \
          case 5:                                  \
            body;                                  \
          case 4:                                  \
            body;                                  \
          case 3:                                  \
            body;                                  \
          case 2:                                  \
            body;                                  \
          case 1:                                  \
            body;                                  \
          case 0:                                  \
            ;                                      \
        }                                          \
    }                                              \
}                                                  \

#else 

#define SM_UNROLLED_WHILE(count, body)             \
{                                                  \
    PRUword _cnt = (PRUword)(count);               \
    while (_cnt-- > 0) {                           \
        body;                                      \
    }                                              \
}                                                  \

#endif

/*******************************************************************************
 * Test for overlapping (one-dimensional) regions
 ******************************************************************************/

#define SM_OVERLAPPING(min1, max1, min2, max2) \
    (SM_ASSERT((min1) < (max1)),               \
     SM_ASSERT((min2) < (max2)),               \
     ((min1) < (max2) && (max1) > (min2)))     \

/******************************************************************************/

PR_END_EXTERN_C

#endif /* __SMPRIV__ */
/******************************************************************************/
