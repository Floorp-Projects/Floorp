/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*-
 * Copyright (c) 1992, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* We need this because Solaris' version of qsort is broken and
 * causes array bounds reads.
 */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKQUICKSORT_
#include "morkQuickSort.h"
#endif

#if !defined(DEBUG) && (defined(__cplusplus) || defined(__gcc))
# ifndef INLINE
#  define INLINE inline
# endif
#else
# define INLINE
#endif

static INLINE mork_u1*
morkQS_med3(mork_u1 *, mork_u1 *, mork_u1 *, mdbAny_Order, void *);

static INLINE void
morkQS_swapfunc(mork_u1 *, mork_u1 *, int, int);

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define morkQS_swapcode(TYPE, parmi, parmj, n) {     \
  long i = (n) / sizeof (TYPE);       \
  register TYPE *pi = (TYPE *) (parmi);     \
  register TYPE *pj = (TYPE *) (parmj);     \
  do {             \
    register TYPE  t = *pi;    \
    *pi++ = *pj;        \
    *pj++ = t;        \
        } while (--i > 0);        \
}

#define morkQS_SwapInit(a, es) swaptype = (a - (mork_u1 *)0) % sizeof(long) || \
  es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static INLINE void
morkQS_swapfunc(mork_u1* a, mork_u1* b, int n, int swaptype)
{
  if(swaptype <= 1)
    morkQS_swapcode(long, a, b, n)
  else
    morkQS_swapcode(mork_u1, a, b, n)
}

#define morkQS_swap(a, b)          \
  if (swaptype == 0) {        \
    long t = *(long *)(a);      \
    *(long *)(a) = *(long *)(b);    \
    *(long *)(b) = t;      \
  } else            \
    morkQS_swapfunc(a, b, (int)inSize, swaptype)

#define morkQS_vecswap(a, b, n)   if ((n) > 0) morkQS_swapfunc(a, b, (int)n, swaptype)

static INLINE mork_u1 *
morkQS_med3(mork_u1* a, mork_u1* b, mork_u1* c, mdbAny_Order cmp, void* closure)
{
  return (*cmp)(a, b, closure) < 0 ?
    ((*cmp)(b, c, closure) < 0 ? b : ((*cmp)(a, c, closure) < 0 ? c : a ))
      :((*cmp)(b, c, closure) > 0 ? b : ((*cmp)(a, c, closure) < 0 ? a : c ));
}

#define morkQS_MIN(x,y)     ((x)<(y)?(x):(y))

void
morkQuickSort(mork_u1* ioVec, mork_u4 inCount, mork_u4 inSize,
  mdbAny_Order inOrder, void* ioClosure)
{
  mork_u1* pa, *pb, *pc, *pd, *pl, *pm, *pn;
  int d, r, swaptype, swap_cnt;

tailCall:  morkQS_SwapInit(ioVec, inSize);
  swap_cnt = 0;
  if (inCount < 7) {
    for (pm = ioVec + inSize; pm < ioVec + inCount * inSize; pm += inSize)
      for (pl = pm; pl > ioVec && (*inOrder)(pl - inSize, pl, ioClosure) > 0;
           pl -= inSize)
        morkQS_swap(pl, pl - inSize);
    return;
  }
  pm = ioVec + (inCount / 2) * inSize;
  if (inCount > 7) {
    pl = ioVec;
    pn = ioVec + (inCount - 1) * inSize;
    if (inCount > 40) {
      d = (inCount / 8) * inSize;
      pl = morkQS_med3(pl, pl + d, pl + 2 * d, inOrder, ioClosure);
      pm = morkQS_med3(pm - d, pm, pm + d, inOrder, ioClosure);
      pn = morkQS_med3(pn - 2 * d, pn - d, pn, inOrder, ioClosure);
    }
    pm = morkQS_med3(pl, pm, pn, inOrder, ioClosure);
  }
  morkQS_swap(ioVec, pm);
  pa = pb = ioVec + inSize;

  pc = pd = ioVec + (inCount - 1) * inSize;
  for (;;) {
    while (pb <= pc && (r = (*inOrder)(pb, ioVec, ioClosure)) <= 0) {
      if (r == 0) {
        swap_cnt = 1;
        morkQS_swap(pa, pb);
        pa += inSize;
      }
      pb += inSize;
    }
    while (pb <= pc && (r = (*inOrder)(pc, ioVec, ioClosure)) >= 0) {
      if (r == 0) {
        swap_cnt = 1;
        morkQS_swap(pc, pd);
        pd -= inSize;
      }
      pc -= inSize;
    }
    if (pb > pc)
      break;
    morkQS_swap(pb, pc);
    swap_cnt = 1;
    pb += inSize;
    pc -= inSize;
  }
  if (swap_cnt == 0) {  /* Switch to insertion sort */
    for (pm = ioVec + inSize; pm < ioVec + inCount * inSize; pm += inSize)
      for (pl = pm; pl > ioVec && (*inOrder)(pl - inSize, pl, ioClosure) > 0;
           pl -= inSize)
        morkQS_swap(pl, pl - inSize);
    return;
  }

  pn = ioVec + inCount * inSize;
  r = morkQS_MIN(pa - ioVec, pb - pa);
  morkQS_vecswap(ioVec, pb - r, r);
  r = morkQS_MIN(pd - pc, (int)(pn - pd - inSize));
  morkQS_vecswap(pb, pn - r, r);
  if ((r = pb - pa) > (int)inSize)
        morkQuickSort(ioVec, r / inSize, inSize, inOrder, ioClosure);
  if ((r = pd - pc) > (int)inSize) {
    /* Iterate rather than recurse to save stack space */
    ioVec = pn - r;
    inCount = r / inSize;
    goto tailCall;
  }
/*    morkQuickSort(pn - r, r / inSize, inSize, inOrder, ioClosure);*/
}

