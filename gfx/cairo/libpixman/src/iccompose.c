/*
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#include "icint.h"

/*
 * General purpose compositing code optimized for minimal memory
 * references
 *
 * All work is done on canonical ARGB values, functions for fetching
 * and storing these exist for each format.
 */

/*
 * Combine src and mask using IN
 */

static uint32_t
IcCombineMaskU (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk)
{
    uint32_t  x;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetch) (msk) >> 24;
    if (!a)
	return 0;
    
    x = (*src->fetch) (src);
    if (a == 0xff)
	return x;
    
    m = IcInU(x,0,a,t);
    n = IcInU(x,8,a,t);
    o = IcInU(x,16,a,t);
    p = IcInU(x,24,a,t);
    return m|n|o|p;
}

static IcCompSrc
IcCombineMaskC (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk)
{
    IcCompSrc	s;
    uint32_t	x;
    uint32_t	a;
    uint16_t	xa;
    uint16_t	t;
    uint32_t	m,n,o,p;

    if (!msk)
    {
	x = (*src->fetch) (src);
	s.value = x;
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	s.alpha = x;
	return s;
    }
    
    a = (*msk->fetcha) (msk);
    if (!a)
    {
	s.value = 0;
	s.alpha = 0;
	return s;
    }
    
    x = (*src->fetch) (src);
    if (a == 0xffffffff)
    {
	s.value = x;
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	s.alpha = x;
	return s;
    }
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    s.value = m|n|o|p;
    xa = x >> 24;
    m = IcInU(a,0,xa,t);
    n = IcInU(a,8,xa,t);
    o = IcInU(a,16,xa,t);
    p = IcInU(a,24,xa,t);
    s.alpha = m|n|o|p;
    return s;
}

static uint32_t
IcCombineMaskValueC (pixman_compositeOperand   *src,
		     pixman_compositeOperand   *msk)
{
    uint32_t	x;
    uint32_t	a;
    uint16_t	t;
    uint32_t	m,n,o,p;

    if (!msk)
    {
	return (*src->fetch) (src);
    }
    
    a = (*msk->fetcha) (msk);
    if (!a)
	return a;
    
    x = (*src->fetch) (src);
    if (a == 0xffffffff)
	return x;
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    return m|n|o|p;
}

/*
 * Combine src and mask using IN, generating only the alpha component
 */
static uint32_t
IcCombineMaskAlphaU (pixman_compositeOperand   *src,
		     pixman_compositeOperand   *msk)
{
    uint32_t  x;
    uint16_t  a;
    uint16_t  t;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetch) (msk) >> 24;
    if (!a)
	return 0;
    
    x = (*src->fetch) (src);
    if (a == 0xff)
	return x;
    
    return IcInU(x,24,a,t);
}

static uint32_t
IcCombineMaskAlphaC (pixman_compositeOperand   *src,
		     pixman_compositeOperand   *msk)
{
    uint32_t	x;
    uint32_t	a;
    uint16_t	t;
    uint32_t	m,n,o,p;

    if (!msk)
	return (*src->fetch) (src);
    
    a = (*msk->fetcha) (msk);
    if (!a)
	return 0;
    
    x = (*src->fetcha) (src);
    if (a == 0xffffffff)
	return x;
    
    m = IcInC(x,0,a,t);
    n = IcInC(x,8,a,t);
    o = IcInC(x,16,a,t);
    p = IcInC(x,24,a,t);
    return m|n|o|p;
}

/*
 * All of the composing functions
 */
static void
IcCombineClear (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk,
		pixman_compositeOperand   *dst)
{
    (*dst->store) (dst, 0);
}

static void
IcCombineSrcU (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    (*dst->store) (dst, IcCombineMaskU (src, msk));
}

static void
IcCombineSrcC (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    (*dst->store) (dst, IcCombineMaskValueC (src, msk));
}

static void
IcCombineDst (pixman_compositeOperand    *src,
	      pixman_compositeOperand    *msk,
	      pixman_compositeOperand    *dst)
{
    /* noop */
}

static void
IcCombineOverU (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk,
		pixman_compositeOperand   *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = ~s >> 24;
    if (a != 0xff)
    {
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

static void
IcCombineOverC (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk,
		pixman_compositeOperand   *dst)
{
    IcCompSrc	cs;
    uint32_t  s, d;
    uint32_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    a = ~cs.alpha;
    if (a != 0xffffffff)
    {
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcOverC(s,d,0,a,t);
	    n = IcOverC(s,d,8,a,t);
	    o = IcOverC(s,d,16,a,t);
	    p = IcOverC(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

static void
IcCombineOverReverseU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcOverU(d,s,0,a,t);
	    n = IcOverU(d,s,8,a,t);
	    o = IcOverU(d,s,16,a,t);
	    p = IcOverU(d,s,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

static void
IcCombineOverReverseC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint32_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcOverU(d,s,0,a,t);
	    n = IcOverU(d,s,8,a,t);
	    o = IcOverU(d,s,16,a,t);
	    p = IcOverU(d,s,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

static void
IcCombineInU (pixman_compositeOperand	    *src,
	      pixman_compositeOperand	    *msk,
	      pixman_compositeOperand	    *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

static void
IcCombineInC (pixman_compositeOperand	    *src,
	      pixman_compositeOperand	    *msk,
	      pixman_compositeOperand	    *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

static void
IcCombineInReverseU (pixman_compositeOperand  *src,
		     pixman_compositeOperand  *msk,
		     pixman_compositeOperand  *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskAlphaU (src, msk);
    a = s >> 24;
    if (a != 0xff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInU(d,0,a,t);
	    n = IcInU(d,8,a,t);
	    o = IcInU(d,16,a,t);
	    p = IcInU(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

static void
IcCombineInReverseC (pixman_compositeOperand  *src,
		     pixman_compositeOperand  *msk,
		     pixman_compositeOperand  *dst)
{
    uint32_t  s, d;
    uint32_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskAlphaC (src, msk);
    a = s;
    if (a != 0xffffffff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInC(d,0,a,t);
	    n = IcInC(d,8,a,t);
	    o = IcInC(d,16,a,t);
	    p = IcInC(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

static void
IcCombineOutU (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskU (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

static void
IcCombineOutC (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    d = (*dst->fetch) (dst);
    a = ~d >> 24;
    s = 0;
    if (a)
    {
	s = IcCombineMaskValueC (src, msk);
	if (a != 0xff)
	{
	    m = IcInU(s,0,a,t);
	    n = IcInU(s,8,a,t);
	    o = IcInU(s,16,a,t);
	    p = IcInU(s,24,a,t);
	    s = m|n|o|p;
	}
    }
    (*dst->store) (dst, s);
}

static void
IcCombineOutReverseU (pixman_compositeOperand *src,
		      pixman_compositeOperand *msk,
		      pixman_compositeOperand *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskAlphaU (src, msk);
    a = ~s >> 24;
    if (a != 0xff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInU(d,0,a,t);
	    n = IcInU(d,8,a,t);
	    o = IcInU(d,16,a,t);
	    p = IcInU(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

static void
IcCombineOutReverseC (pixman_compositeOperand *src,
		      pixman_compositeOperand *msk,
		      pixman_compositeOperand *dst)
{
    uint32_t  s, d;
    uint32_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskAlphaC (src, msk);
    a = ~s;
    if (a != 0xffffffff)
    {
	d = 0;
	if (a)
	{
	    d = (*dst->fetch) (dst);
	    m = IcInC(d,0,a,t);
	    n = IcInC(d,8,a,t);
	    o = IcInC(d,16,a,t);
	    p = IcInC(d,24,a,t);
	    d = m|n|o|p;
	}
	(*dst->store) (dst, d);
    }
}

static void
IcCombineAtopU (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk,
		pixman_compositeOperand   *dst)
{
    uint32_t  s, d;
    uint16_t  ad, as;
    uint16_t  t,u,v;
    uint32_t  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = ~s >> 24;
    as = d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineAtopC (pixman_compositeOperand   *src,
		pixman_compositeOperand   *msk,
		pixman_compositeOperand   *dst)
{
    IcCompSrc	cs;
    uint32_t  s, d;
    uint32_t  ad;
    uint16_t  as;
    uint16_t  t, u, v;
    uint32_t  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = cs.alpha;
    as = d >> 24;
    m = IcGen(s,d,0,as,IcGet8(ad,0),t,u,v);
    n = IcGen(s,d,8,as,IcGet8(ad,8),t,u,v);
    o = IcGen(s,d,16,as,IcGet8(ad,16),t,u,v);
    p = IcGen(s,d,24,as,IcGet8(ad,24),t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineAtopReverseU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  ad, as;
    uint16_t  t, u, v;
    uint32_t  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = s >> 24;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineAtopReverseC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCompSrc	cs;
    uint32_t  s, d, ad;
    uint16_t  as;
    uint16_t  t, u, v;
    uint32_t  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = cs.alpha;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,IcGet8(ad,0),t,u,v);
    n = IcGen(s,d,8,as,IcGet8(ad,8),t,u,v);
    o = IcGen(s,d,16,as,IcGet8(ad,16),t,u,v);
    p = IcGen(s,d,24,as,IcGet8(ad,24),t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineXorU (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  ad, as;
    uint16_t  t, u, v;
    uint32_t  m,n,o,p;
    
    s = IcCombineMaskU (src, msk);
    d = (*dst->fetch) (dst);
    ad = ~s >> 24;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineXorC (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    IcCompSrc	cs;
    uint32_t  s, d, ad;
    uint16_t  as;
    uint16_t  t, u, v;
    uint32_t  m,n,o,p;
    
    cs = IcCombineMaskC (src, msk);
    d = (*dst->fetch) (dst);
    s = cs.value;
    ad = ~cs.alpha;
    as = ~d >> 24;
    m = IcGen(s,d,0,as,ad,t,u,v);
    n = IcGen(s,d,8,as,ad,t,u,v);
    o = IcGen(s,d,16,as,ad,t,u,v);
    p = IcGen(s,d,24,as,ad,t,u,v);
    (*dst->store) (dst, m|n|o|p);
}

static void
IcCombineAddU (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    if (s == ~0)
	(*dst->store) (dst, s);
    else
    {
	d = (*dst->fetch) (dst);
	if (s && d != ~0)
	{
	    m = IcAdd(s,d,0,t);
	    n = IcAdd(s,d,8,t);
	    o = IcAdd(s,d,16,t);
	    p = IcAdd(s,d,24,t);
	    (*dst->store) (dst, m|n|o|p);
	}
    }
}

static void
IcCombineAddC (pixman_compositeOperand    *src,
	       pixman_compositeOperand    *msk,
	       pixman_compositeOperand    *dst)
{
    uint32_t  s, d;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskValueC (src, msk);
    if (s == ~0)
	(*dst->store) (dst, s);
    else
    {
	d = (*dst->fetch) (dst);
	if (s && d != ~0)
	{
	    m = IcAdd(s,d,0,t);
	    n = IcAdd(s,d,8,t);
	    o = IcAdd(s,d,16,t);
	    p = IcAdd(s,d,24,t);
	    (*dst->store) (dst, m|n|o|p);
	}
    }
}

/*
 * All of the disjoint composing functions

 The four entries in the first column indicate what source contributions
 come from each of the four areas of the picture -- areas covered by neither
 A nor B, areas covered only by A, areas covered only by B and finally
 areas covered by both A and B.
 
		Disjoint			Conjoint
		Fa		Fb		Fa		Fb
(0,0,0,0)	0		0		0		0
(0,A,0,A)	1		0		1		0
(0,0,B,B)	0		1		0		1
(0,A,B,A)	1		min((1-a)/b,1)	1		max(1-a/b,0)
(0,A,B,B)	min((1-b)/a,1)	1		max(1-b/a,0)	1		
(0,0,0,A)	max(1-(1-b)/a,0) 0		min(1,b/a)	0
(0,0,0,B)	0		max(1-(1-a)/b,0) 0		min(a/b,1)
(0,A,0,0)	min(1,(1-b)/a)	0		max(1-b/a,0)	0
(0,0,B,0)	0		min(1,(1-a)/b)	0		max(1-a/b,0)
(0,0,B,A)	max(1-(1-b)/a,0) min(1,(1-a)/b)	 min(1,b/a)	max(1-a/b,0)
(0,A,0,B)	min(1,(1-b)/a)	max(1-(1-a)/b,0) max(1-b/a,0)	min(1,a/b)
(0,A,B,0)	min(1,(1-b)/a)	min(1,(1-a)/b)	max(1-b/a,0)	max(1-a/b,0)

 */

#define CombineAOut 1
#define CombineAIn  2
#define CombineBOut 4
#define CombineBIn  8

#define CombineClear	0
#define CombineA	(CombineAOut|CombineAIn)
#define CombineB	(CombineBOut|CombineBIn)
#define CombineAOver	(CombineAOut|CombineBOut|CombineAIn)
#define CombineBOver	(CombineAOut|CombineBOut|CombineBIn)
#define CombineAAtop	(CombineBOut|CombineAIn)
#define CombineBAtop	(CombineAOut|CombineBIn)
#define CombineXor	(CombineAOut|CombineBOut)

/* portion covered by a but not b */
static uint8_t
IcCombineDisjointOutPart (uint8_t a, uint8_t b)
{
    /* min (1, (1-b) / a) */
    
    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0xff;	    /* 1 */
    return IcIntDiv(b,a);   /* (1-b) / a */
}

/* portion covered by both a and b */
static uint8_t
IcCombineDisjointInPart (uint8_t a, uint8_t b)
{
    /* max (1-(1-b)/a,0) */
    /*  = - min ((1-b)/a - 1, 0) */
    /*  = 1 - min (1, (1-b)/a) */

    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0;	    /* 1 - 1 */
    return ~IcIntDiv(b,a);  /* 1 - (1-b) / a */
}

static void
IcCombineDisjointGeneralU (pixman_compositeOperand   *src,
			   pixman_compositeOperand   *msk,
			   pixman_compositeOperand   *dst,
			   uint8_t		combine)
{
    uint32_t  s, d;
    uint32_t  m,n,o,p;
    uint16_t  Fa, Fb, t, u, v;
    uint8_t   sa, da;

    s = IcCombineMaskU (src, msk);
    sa = s >> 24;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	Fa = IcCombineDisjointOutPart (sa, da);
	break;
    case CombineAIn:
	Fa = IcCombineDisjointInPart (sa, da);
	break;
    case CombineA:
	Fa = 0xff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineDisjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineDisjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,Fa,Fb,t,u,v);
    n = IcGen (s,d,8,Fa,Fb,t,u,v);
    o = IcGen (s,d,16,Fa,Fb,t,u,v);
    p = IcGen (s,d,24,Fa,Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

static void
IcCombineDisjointGeneralC (pixman_compositeOperand   *src,
			   pixman_compositeOperand   *msk,
			   pixman_compositeOperand   *dst,
			   uint8_t		combine)
{
    IcCompSrc	cs;
    uint32_t  s, d;
    uint32_t  m,n,o,p;
    uint32_t  Fa;
    uint16_t  Fb, t, u, v;
    uint32_t  sa;
    uint8_t   da;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    sa = cs.alpha;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	m = IcCombineDisjointOutPart ((uint8_t) (sa >> 0), da);
	n = IcCombineDisjointOutPart ((uint8_t) (sa >> 8), da) << 8;
	o = IcCombineDisjointOutPart ((uint8_t) (sa >> 16), da) << 16;
	p = IcCombineDisjointOutPart ((uint8_t) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineAIn:
	m = IcCombineDisjointOutPart ((uint8_t) (sa >> 0), da);
	n = IcCombineDisjointOutPart ((uint8_t) (sa >> 8), da) << 8;
	o = IcCombineDisjointOutPart ((uint8_t) (sa >> 16), da) << 16;
	p = IcCombineDisjointOutPart ((uint8_t) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineA:
	Fa = 0xffffffff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineDisjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineDisjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,IcGet8(Fa,0),Fb,t,u,v);
    n = IcGen (s,d,8,IcGet8(Fa,8),Fb,t,u,v);
    o = IcGen (s,d,16,IcGet8(Fa,16),Fb,t,u,v);
    p = IcGen (s,d,24,IcGet8(Fa,24),Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

static void
IcCombineDisjointOverU (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = s >> 24;
    if (a != 0x00)
    {
	if (a != 0xff)
	{
	    d = (*dst->fetch) (dst);
	    a = IcCombineDisjointOutPart (d >> 24, a);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
}

static void
IcCombineDisjointOverC (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAOver);
}

static void
IcCombineDisjointOverReverseU (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBOver);
}

static void
IcCombineDisjointOverReverseC (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBOver);
}

static void
IcCombineDisjointInU (pixman_compositeOperand	    *src,
		      pixman_compositeOperand	    *msk,
		      pixman_compositeOperand	    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAIn);
}

static void
IcCombineDisjointInC (pixman_compositeOperand	    *src,
		      pixman_compositeOperand	    *msk,
		      pixman_compositeOperand	    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAIn);
}

static void
IcCombineDisjointInReverseU (pixman_compositeOperand  *src,
			     pixman_compositeOperand  *msk,
			     pixman_compositeOperand  *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBIn);
}

static void
IcCombineDisjointInReverseC (pixman_compositeOperand  *src,
			     pixman_compositeOperand  *msk,
			     pixman_compositeOperand  *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBIn);
}

static void
IcCombineDisjointOutU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAOut);
}

static void
IcCombineDisjointOutC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAOut);
}

static void
IcCombineDisjointOutReverseU (pixman_compositeOperand *src,
			      pixman_compositeOperand *msk,
			      pixman_compositeOperand *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBOut);
}

static void
IcCombineDisjointOutReverseC (pixman_compositeOperand *src,
			      pixman_compositeOperand *msk,
			      pixman_compositeOperand *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBOut);
}

static void
IcCombineDisjointAtopU (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineAAtop);
}

static void
IcCombineDisjointAtopC (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineAAtop);
}

static void
IcCombineDisjointAtopReverseU (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineBAtop);
}

static void
IcCombineDisjointAtopReverseC (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineBAtop);
}

static void
IcCombineDisjointXorU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralU (src, msk, dst, CombineXor);
}

static void
IcCombineDisjointXorC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineDisjointGeneralC (src, msk, dst, CombineXor);
}

/* portion covered by a but not b */
static uint8_t
IcCombineConjointOutPart (uint8_t a, uint8_t b)
{
    /* max (1-b/a,0) */
    /* = 1-min(b/a,1) */
    
    /* min (1, (1-b) / a) */
    
    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0x00;	    /* 0 */
    return ~IcIntDiv(b,a);   /* 1 - b/a */
}

/* portion covered by both a and b */
static uint8_t
IcCombineConjointInPart (uint8_t a, uint8_t b)
{
    /* min (1,b/a) */

    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0xff;	    /* 1 */
    return IcIntDiv(b,a);   /* b/a */
}

static void
IcCombineConjointGeneralU (pixman_compositeOperand   *src,
			   pixman_compositeOperand   *msk,
			   pixman_compositeOperand   *dst,
			   uint8_t		combine)
{
    uint32_t  s, d;
    uint32_t  m,n,o,p;
    uint16_t  Fa, Fb, t, u, v;
    uint8_t   sa, da;

    s = IcCombineMaskU (src, msk);
    sa = s >> 24;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	Fa = IcCombineConjointOutPart (sa, da);
	break;
    case CombineAIn:
	Fa = IcCombineConjointInPart (sa, da);
	break;
    case CombineA:
	Fa = 0xff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineConjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineConjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,Fa,Fb,t,u,v);
    n = IcGen (s,d,8,Fa,Fb,t,u,v);
    o = IcGen (s,d,16,Fa,Fb,t,u,v);
    p = IcGen (s,d,24,Fa,Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

static void
IcCombineConjointGeneralC (pixman_compositeOperand   *src,
			   pixman_compositeOperand   *msk,
			   pixman_compositeOperand   *dst,
			   uint8_t		combine)
{
    IcCompSrc	cs;
    uint32_t  s, d;
    uint32_t  m,n,o,p;
    uint32_t  Fa;
    uint16_t  Fb, t, u, v;
    uint32_t  sa;
    uint8_t   da;

    cs = IcCombineMaskC (src, msk);
    s = cs.value;
    sa = cs.alpha;
    
    d = (*dst->fetch) (dst);
    da = d >> 24;
    
    switch (combine & CombineA) {
    default:
	Fa = 0;
	break;
    case CombineAOut:
	m = IcCombineConjointOutPart ((uint8_t) (sa >> 0), da);
	n = IcCombineConjointOutPart ((uint8_t) (sa >> 8), da) << 8;
	o = IcCombineConjointOutPart ((uint8_t) (sa >> 16), da) << 16;
	p = IcCombineConjointOutPart ((uint8_t) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineAIn:
	m = IcCombineConjointOutPart ((uint8_t) (sa >> 0), da);
	n = IcCombineConjointOutPart ((uint8_t) (sa >> 8), da) << 8;
	o = IcCombineConjointOutPart ((uint8_t) (sa >> 16), da) << 16;
	p = IcCombineConjointOutPart ((uint8_t) (sa >> 24), da) << 24;
	Fa = m|n|o|p;
	break;
    case CombineA:
	Fa = 0xffffffff;
	break;
    }
    
    switch (combine & CombineB) {
    default:
	Fb = 0;
	break;
    case CombineBOut:
	Fb = IcCombineConjointOutPart (da, sa);
	break;
    case CombineBIn:
	Fb = IcCombineConjointInPart (da, sa);
	break;
    case CombineB:
	Fb = 0xff;
	break;
    }
    m = IcGen (s,d,0,IcGet8(Fa,0),Fb,t,u,v);
    n = IcGen (s,d,8,IcGet8(Fa,8),Fb,t,u,v);
    o = IcGen (s,d,16,IcGet8(Fa,16),Fb,t,u,v);
    p = IcGen (s,d,24,IcGet8(Fa,24),Fb,t,u,v);
    s = m|n|o|p;
    (*dst->store) (dst, s);
}

static void
IcCombineConjointOverU (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAOver);
/*
    uint32_t  s, d;
    uint16_t  a;
    uint16_t  t;
    uint32_t  m,n,o,p;

    s = IcCombineMaskU (src, msk);
    a = s >> 24;
    if (a != 0x00)
    {
	if (a != 0xff)
	{
	    d = (*dst->fetch) (dst);
	    a = IcCombineConjointOutPart (d >> 24, a);
	    m = IcOverU(s,d,0,a,t);
	    n = IcOverU(s,d,8,a,t);
	    o = IcOverU(s,d,16,a,t);
	    p = IcOverU(s,d,24,a,t);
	    s = m|n|o|p;
	}
	(*dst->store) (dst, s);
    }
 */
}

static void
IcCombineConjointOverC (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAOver);
}

static void
IcCombineConjointOverReverseU (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBOver);
}

static void
IcCombineConjointOverReverseC (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBOver);
}

static void
IcCombineConjointInU (pixman_compositeOperand	    *src,
		      pixman_compositeOperand	    *msk,
		      pixman_compositeOperand	    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAIn);
}

static void
IcCombineConjointInC (pixman_compositeOperand	    *src,
		      pixman_compositeOperand	    *msk,
		      pixman_compositeOperand	    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAIn);
}

static void
IcCombineConjointInReverseU (pixman_compositeOperand  *src,
			     pixman_compositeOperand  *msk,
			     pixman_compositeOperand  *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBIn);
}

static void
IcCombineConjointInReverseC (pixman_compositeOperand  *src,
			     pixman_compositeOperand  *msk,
			     pixman_compositeOperand  *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBIn);
}

static void
IcCombineConjointOutU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAOut);
}

static void
IcCombineConjointOutC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAOut);
}

static void
IcCombineConjointOutReverseU (pixman_compositeOperand *src,
			      pixman_compositeOperand *msk,
			      pixman_compositeOperand *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBOut);
}

static void
IcCombineConjointOutReverseC (pixman_compositeOperand *src,
			      pixman_compositeOperand *msk,
			      pixman_compositeOperand *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBOut);
}

static void
IcCombineConjointAtopU (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineAAtop);
}

static void
IcCombineConjointAtopC (pixman_compositeOperand   *src,
			pixman_compositeOperand   *msk,
			pixman_compositeOperand   *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineAAtop);
}

static void
IcCombineConjointAtopReverseU (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineBAtop);
}

static void
IcCombineConjointAtopReverseC (pixman_compositeOperand    *src,
			       pixman_compositeOperand    *msk,
			       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineBAtop);
}

static void
IcCombineConjointXorU (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralU (src, msk, dst, CombineXor);
}

static void
IcCombineConjointXorC (pixman_compositeOperand    *src,
		       pixman_compositeOperand    *msk,
		       pixman_compositeOperand    *dst)
{
    IcCombineConjointGeneralC (src, msk, dst, CombineXor);
}

static IcCombineFunc const IcCombineFuncU[] = {
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineOverU,
    IcCombineOverReverseU,
    IcCombineInU,
    IcCombineInReverseU,
    IcCombineOutU,
    IcCombineOutReverseU,
    IcCombineAtopU,
    IcCombineAtopReverseU,
    IcCombineXorU,
    IcCombineAddU,
    IcCombineDisjointOverU, /* Saturate */
    0,
    0,
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineDisjointOverU,
    IcCombineDisjointOverReverseU,
    IcCombineDisjointInU,
    IcCombineDisjointInReverseU,
    IcCombineDisjointOutU,
    IcCombineDisjointOutReverseU,
    IcCombineDisjointAtopU,
    IcCombineDisjointAtopReverseU,
    IcCombineDisjointXorU,
    0,
    0,
    0,
    0,
    IcCombineClear,
    IcCombineSrcU,
    IcCombineDst,
    IcCombineConjointOverU,
    IcCombineConjointOverReverseU,
    IcCombineConjointInU,
    IcCombineConjointInReverseU,
    IcCombineConjointOutU,
    IcCombineConjointOutReverseU,
    IcCombineConjointAtopU,
    IcCombineConjointAtopReverseU,
    IcCombineConjointXorU,
};

static IcCombineFunc const IcCombineFuncC[] = {
    IcCombineClear,
    IcCombineSrcC,
    IcCombineDst,
    IcCombineOverC,
    IcCombineOverReverseC,
    IcCombineInC,
    IcCombineInReverseC,
    IcCombineOutC,
    IcCombineOutReverseC,
    IcCombineAtopC,
    IcCombineAtopReverseC,
    IcCombineXorC,
    IcCombineAddC,
    IcCombineDisjointOverC, /* Saturate */
    0,
    0,
    IcCombineClear,	    /* 0x10 */
    IcCombineSrcC,
    IcCombineDst,
    IcCombineDisjointOverC,
    IcCombineDisjointOverReverseC,
    IcCombineDisjointInC,
    IcCombineDisjointInReverseC,
    IcCombineDisjointOutC,
    IcCombineDisjointOutReverseC,
    IcCombineDisjointAtopC,
    IcCombineDisjointAtopReverseC,
    IcCombineDisjointXorC,  /* 0x1b */
    0,
    0,
    0,
    0,
    IcCombineClear,
    IcCombineSrcC,
    IcCombineDst,
    IcCombineConjointOverC,
    IcCombineConjointOverReverseC,
    IcCombineConjointInC,
    IcCombineConjointInReverseC,
    IcCombineConjointOutC,
    IcCombineConjointOutReverseC,
    IcCombineConjointAtopC,
    IcCombineConjointAtopReverseC,
    IcCombineConjointXorC,
};

/*
 * All of the fetch functions
 */

static uint32_t
IcFetch_a8r8g8b8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    return ((uint32_t *)line)[offset >> 5];
}

static uint32_t
IcFetch_x8r8g8b8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    return ((uint32_t *)line)[offset >> 5] | 0xff000000;
}

static uint32_t
IcFetch_a8b8g8r8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint32_t *)line)[offset >> 5];

    return ((pixel & 0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static uint32_t
IcFetch_x8b8g8r8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint32_t *)line)[offset >> 5];

    return ((0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static uint32_t
IcFetch_r8g8b8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (pixel[0] << 16) |
	    (pixel[1] << 8) |
	    (pixel[2]));
#else
    return (0xff000000 |
	    (pixel[2] << 16) |
	    (pixel[1] << 8) |
	    (pixel[0]));
#endif
}

static uint32_t
IcFetch_b8g8r8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (pixel[2] << 16) |
	    (pixel[1] << 8) |
	    (pixel[0]));
#else
    return (0xff000000 |
	    (pixel[0] << 16) |
	    (pixel[1] << 8) |
	    (pixel[2]));
#endif
}

static uint32_t
IcFetch_r5g6b5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    r = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) << 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_b5g6r5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    b = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) >> 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_a1r5g5b5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  a,r,g,b;

    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (a | r | g | b);
}

static uint32_t
IcFetch_x1r5g5b5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_a1b5g5r5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  a,r,g,b;

    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (a | r | g | b);
}

static uint32_t
IcFetch_x1b5g5r5 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_a4r4g4b4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  a,r,g,b;

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}
    
static uint32_t
IcFetch_x4r4g4b4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}
    
static uint32_t
IcFetch_a4b4g4r4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  a,r,g,b;

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}
    
static uint32_t
IcFetch_x4b4g4r4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint16_t *) line)[offset >> 4];
    uint32_t  r,g,b;

    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}
    
static uint32_t
IcFetch_a8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t   pixel = ((uint8_t *) line)[offset>>3];
    
    return pixel << 24;
}

static uint32_t
IcFetcha_a8 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t   pixel = ((uint8_t *) line)[offset>>3];
    
    pixel |= pixel << 8;
    pixel |= pixel << 16;
    return pixel;
}

static uint32_t
IcFetch_r3g3b2 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t   pixel = ((uint8_t *) line)[offset>>3];
    uint32_t  r,g,b;
    
    r = ((pixel & 0xe0) | ((pixel & 0xe0) >> 3) | ((pixel & 0xc0) >> 6)) << 16;
    g = ((pixel & 0x1c) | ((pixel & 0x18) >> 3) | ((pixel & 0x1c) << 3)) << 8;
    b = (((pixel & 0x03)     ) | 
	 ((pixel & 0x03) << 2) | 
	 ((pixel & 0x03) << 4) |
	 ((pixel & 0x03) << 6));
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_b2g3r3 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t   pixel = ((uint8_t *) line)[offset>>3];
    uint32_t  r,g,b;
    
    b = (((pixel & 0xc0)     ) | 
	 ((pixel & 0xc0) >> 2) |
	 ((pixel & 0xc0) >> 4) |
	 ((pixel & 0xc0) >> 6));
    g = ((pixel & 0x38) | ((pixel & 0x38) >> 3) | ((pixel & 0x30) << 2)) << 8;
    r = (((pixel & 0x07)     ) | 
	 ((pixel & 0x07) << 3) | 
	 ((pixel & 0x06) << 6)) << 16;
    return (0xff000000 | r | g | b);
}

static uint32_t
IcFetch_a2r2g2b2 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t   pixel = ((uint8_t *) line)[offset>>3];
    uint32_t   a,r,g,b;

    a = ((pixel & 0xc0) * 0x55) << 18;
    r = ((pixel & 0x30) * 0x55) << 12;
    g = ((pixel & 0x0c) * 0x55) << 6;
    b = ((pixel & 0x03) * 0x55);
    return a|r|g|b;
}

#define Fetch8(l,o)    (((uint8_t *) (l))[(o) >> 3])
#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) & 0xf : Fetch8(l,o) >> 4)
#else
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) >> 4 : Fetch8(l,o) & 0xf)
#endif

static uint32_t
IcFetch_a4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    
    pixel |= pixel << 4;
    return pixel << 24;
}

static uint32_t
IcFetcha_a4 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    
    pixel |= pixel << 4;
    pixel |= pixel << 8;
    pixel |= pixel << 16;
    return pixel;
}

static uint32_t
IcFetch_r1g2b1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    uint32_t  r,g,b;

    r = ((pixel & 0x8) * 0xff) << 13;
    g = ((pixel & 0x6) * 0x55) << 7;
    b = ((pixel & 0x1) * 0xff);
    return 0xff000000|r|g|b;
}

static uint32_t
IcFetch_b1g2r1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    uint32_t  r,g,b;

    b = ((pixel & 0x8) * 0xff) >> 3;
    g = ((pixel & 0x6) * 0x55) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;
    return 0xff000000|r|g|b;
}

static uint32_t
IcFetch_a1r1g1b1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    uint32_t  a,r,g,b;

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) << 14;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff);
    return a|r|g|b;
}

static uint32_t
IcFetch_a1b1g1r1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = Fetch4(line, offset);
    uint32_t  a,r,g,b;

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) >> 3;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff) << 16;
    return a|r|g|b;
}

static uint32_t
IcFetcha_a1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint32_t *)line)[offset >> 5];
    uint32_t  a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    a |= a << 1;
    a |= a << 2;
    a |= a << 4;
    a |= a << 8;
    a |= a << 16;
    return a;
}

static uint32_t
IcFetch_a1 (pixman_compositeOperand *op)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel = ((uint32_t *)line)[offset >> 5];
    uint32_t  a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    a |= a << 1;
    a |= a << 2;
    a |= a << 4;
    return a << 24;
}

/*
 * All the store functions
 */

#define Splita(v)	uint32_t	a = ((v) >> 24), r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff
#define Split(v)	uint32_t	r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff

static void
IcStore_a8r8g8b8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    ((uint32_t *)line)[offset >> 5] = value;
}

static void
IcStore_x8r8g8b8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    ((uint32_t *)line)[offset >> 5] = value & 0xffffff;
}

static void
IcStore_a8b8g8r8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    Splita(value);
    ((uint32_t *)line)[offset >> 5] = a << 24 | b << 16 | g << 8 | r;
}

static void
IcStore_x8b8g8r8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    Split(value);
    ((uint32_t *)line)[offset >> 5] = b << 16 | g << 8 | r;
}

static void
IcStore_r8g8b8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    Split(value);
#if IMAGE_BYTE_ORDER == MSBFirst
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
#else
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
#endif
}

static void
IcStore_b8g8r8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    Split(value);
#if IMAGE_BYTE_ORDER == MSBFirst
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
#else
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
#endif
}

static void
IcStore_r5g6b5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 8) & 0xf800) |
	      ((g << 3) & 0x07e0) |
	      ((b >> 3)         ));
}

static void
IcStore_b5g6r5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 8) & 0xf800) |
	      ((g << 3) & 0x07e0) |
	      ((r >> 3)         ));
}

static void
IcStore_a1r5g5b5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0x8000) |
	      ((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
}

static void
IcStore_x1r5g5b5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
}

static void
IcStore_a1b5g5r5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0x8000) |
	      ((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
}

static void
IcStore_x1b5g5r5 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
}

static void
IcStore_a4r4g4b4 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0xf000) |
	      ((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
}

static void
IcStore_x4r4g4b4 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
}

static void
IcStore_a4b4g4r4 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Splita(value);
    *pixel = (((a << 8) & 0xf000) |
	      ((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
}

static void
IcStore_x4b4g4r4 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint16_t  *pixel = ((uint16_t *) line) + (offset >> 4);
    Split(value);
    *pixel = (((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
}

static void
IcStore_a8 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    *pixel = value >> 24;
}

static void
IcStore_r3g3b2 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    Split(value);
    *pixel = (((r     ) & 0xe0) |
	      ((g >> 3) & 0x1c) |
	      ((b >> 6)       ));
}

static void
IcStore_b2g3r3 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    Split(value);
    *pixel = (((b     ) & 0xe0) |
	      ((g >> 3) & 0x1c) |
	      ((r >> 6)       ));
}

static void
IcStore_a2r2g2b2 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint8_t   *pixel = ((uint8_t *) line) + (offset >> 3);
    Splita(value);
    *pixel = (((a     ) & 0xc0) |
	      ((r >> 2) & 0x30) |
	      ((g >> 4) & 0x0c) |
	      ((b >> 6)       ));
}

#define Store8(l,o,v)  (((uint8_t *) l)[(o) >> 3] = (v))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ? \
				   (Fetch8(l,o) & 0xf0) | (v) : \
				   (Fetch8(l,o) & 0x0f) | ((v) << 4)))
#else
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ? \
				   (Fetch8(l,o) & 0x0f) | ((v) << 4) : \
				   (Fetch8(l,o) & 0xf0) | (v)))
#endif

static void
IcStore_a4 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    Store4(line,offset,value>>28);
}

static void
IcStore_r1g2b1 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel;
    
    Split(value);
    pixel = (((r >> 4) & 0x8) |
	     ((g >> 5) & 0x6) |
	     ((b >> 7)      ));
    Store4(line,offset,pixel);
}

static void
IcStore_b1g2r1 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel;
    
    Split(value);
    pixel = (((b >> 4) & 0x8) |
	     ((g >> 5) & 0x6) |
	     ((r >> 7)      ));
    Store4(line,offset,pixel);
}

static void
IcStore_a1r1g1b1 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel;
    Splita(value);
    pixel = (((a >> 4) & 0x8) |
	     ((r >> 5) & 0x4) |
	     ((g >> 6) & 0x2) |
	     ((b >> 7)      ));
    Store4(line,offset,pixel);
}

static void
IcStore_a1b1g1r1 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  pixel;
    Splita(value);
    pixel = (((a >> 4) & 0x8) |
	     ((b >> 5) & 0x4) |
	     ((g >> 6) & 0x2) |
	     ((r >> 7)      ));
    Store4(line,offset,pixel);
}

static void
IcStore_a1 (pixman_compositeOperand *op, uint32_t value)
{
    pixman_bits_t  *line = op->u.drawable.line; uint32_t offset = op->u.drawable.offset;
    uint32_t  *pixel = ((uint32_t *) line) + (offset >> 5);
    uint32_t  mask = IcStipMask(offset & 0x1f, 1);

    value = value & 0x80000000 ? mask : 0;
    *pixel = (*pixel & ~mask) | value;
}

static uint32_t
IcFetch_external (pixman_compositeOperand *op)
{
    uint32_t  rgb = (*op[1].fetch) (&op[1]);
    uint32_t  a = (*op[2].fetch) (&op[2]);

    return (rgb & 0xffffff) | (a & 0xff000000);
}


static uint32_t
IcFetcha_external (pixman_compositeOperand *op)
{
    return (*op[2].fetch) (&op[2]);
}

static void
IcStore_external (pixman_compositeOperand *op, uint32_t value)
{
    (*op[1].store) (&op[1], value | 0xff000000);
    (*op[2].store) (&op[2], value & 0xff000000);
}

static uint32_t
IcFetch_transform (pixman_compositeOperand *op)
{
    pixman_vector_t	v;
    int		x, y;
    int		minx, maxx, miny, maxy;
    uint32_t	rtot, gtot, btot, atot;
    uint32_t	xerr, yerr;
    uint32_t	bits;
    pixman_box16_t	box;

    v.vector[0] = IntToxFixed(op->u.transform.x);
    v.vector[1] = IntToxFixed(op->u.transform.y);
    v.vector[2] = xFixed1;
    if (!pixman_transform_point (op->u.transform.transform, &v))
	return 0;
    switch (op->u.transform.filter) {
    case PIXMAN_FILTER_FAST:
    case PIXMAN_FILTER_NEAREST:
	y = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	x = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	if (op->u.transform.repeat)
	{
	    y = MOD (y, op->u.transform.height);
	    x = MOD (x, op->u.transform.width);
	}
	if (pixman_region_contains_point (op->clip, x, y, &box))
	{
	    (*op[1].set) (&op[1], x, y);
	    bits = (*op[1].fetch) (&op[1]);
	}
	else
	    bits = 0;
	break;
    case PIXMAN_FILTER_GOOD:
    case PIXMAN_FILTER_BEST:
    case PIXMAN_FILTER_BILINEAR:
	rtot = gtot = btot = atot = 0;
	miny = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	maxy = xFixedToInt (xFixedCeil (v.vector[1])) + op->u.transform.top_y;
	
	minx = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	maxx = xFixedToInt (xFixedCeil (v.vector[0])) + op->u.transform.left_x;
	
	yerr = xFixed1 - xFixedFrac (v.vector[1]);
	for (y = miny; y <= maxy; y++)
	{
	    uint32_t	lrtot = 0, lgtot = 0, lbtot = 0, latot = 0;
	    int         tx, ty;

	    if (op->u.transform.repeat)
		ty = MOD (y, op->u.transform.height);
	    else
		ty = y;
	    
	    xerr = xFixed1 - xFixedFrac (v.vector[0]);
	    for (x = minx; x <= maxx; x++)
	    {
		if (op->u.transform.repeat)
		    tx = MOD (x, op->u.transform.width);
		else
		    tx = x;

		if (pixman_region_contains_point (op->clip, tx, ty, &box))
		{
		    (*op[1].set) (&op[1], tx, ty);
		    bits = (*op[1].fetch) (&op[1]);
		    {
			Splita(bits);
			lrtot += r * xerr;
			lgtot += g * xerr;
			lbtot += b * xerr;
			latot += a * xerr;
		    }
		}
		xerr = xFixed1 - xerr;
	    }
	    rtot += (lrtot >> 10) * yerr;
	    gtot += (lgtot >> 10) * yerr;
	    btot += (lbtot >> 10) * yerr;
	    atot += (latot >> 10) * yerr;
	    yerr = xFixed1 - yerr;
	}
	if ((atot >>= 22) > 0xff) atot = 0xff;
	if ((rtot >>= 22) > 0xff) rtot = 0xff;
	if ((gtot >>= 22) > 0xff) gtot = 0xff;
	if ((btot >>= 22) > 0xff) btot = 0xff;
	bits = ((atot << 24) |
		(rtot << 16) |
		(gtot <<  8) |
		(btot       ));
	break;
    default:
	bits = 0;
	break;
    }
    return bits;
}

static uint32_t
IcFetcha_transform (pixman_compositeOperand *op)
{
    pixman_vector_t	v;
    int		x, y;
    int		minx, maxx, miny, maxy;
    uint32_t	rtot, gtot, btot, atot;
    uint32_t	xerr, yerr;
    uint32_t	bits;
    pixman_box16_t	box;

    v.vector[0] = IntToxFixed(op->u.transform.x);
    v.vector[1] = IntToxFixed(op->u.transform.y);
    v.vector[2] = xFixed1;
    if (!pixman_transform_point (op->u.transform.transform, &v))
	return 0;
    switch (op->u.transform.filter) {
    case PIXMAN_FILTER_FAST:
    case PIXMAN_FILTER_NEAREST:
	y = xFixedToInt (v.vector[1]) + op->u.transform.left_x;
	x = xFixedToInt (v.vector[0]) + op->u.transform.top_y;
	if (op->u.transform.repeat)
	{
	    y = MOD (y, op->u.transform.height);
	    x = MOD (x, op->u.transform.width);
	}
	if (pixman_region_contains_point (op->clip, x, y, &box))
	{
	    (*op[1].set) (&op[1], x, y);
	    bits = (*op[1].fetcha) (&op[1]);
	}
	else
	    bits = 0;
	break;
    case PIXMAN_FILTER_GOOD:
    case PIXMAN_FILTER_BEST:
    case PIXMAN_FILTER_BILINEAR:
	rtot = gtot = btot = atot = 0;
	
	miny = xFixedToInt (v.vector[1]) + op->u.transform.top_y;
	maxy = xFixedToInt (xFixedCeil (v.vector[1])) + op->u.transform.top_y;
	
	minx = xFixedToInt (v.vector[0]) + op->u.transform.left_x;
	maxx = xFixedToInt (xFixedCeil (v.vector[0])) + op->u.transform.left_x;
	
	yerr = xFixed1 - xFixedFrac (v.vector[1]);
	for (y = miny; y <= maxy; y++)
	{
	    uint32_t	lrtot = 0, lgtot = 0, lbtot = 0, latot = 0;
	    int         tx, ty;

	    if (op->u.transform.repeat)
		ty = MOD (y, op->u.transform.height);
	    else
		ty = y;
	    
	    xerr = xFixed1 - xFixedFrac (v.vector[0]);
	    for (x = minx; x <= maxx; x++)
	    {
		if (op->u.transform.repeat)
		    tx = MOD (x, op->u.transform.width);
		else
		    tx = x;
		
		if (pixman_region_contains_point (op->clip, tx, ty, &box))
		{
		    (*op[1].set) (&op[1], tx, ty);
		    bits = (*op[1].fetcha) (&op[1]);
		    {
			Splita(bits);
			lrtot += r * xerr;
			lgtot += g * xerr;
			lbtot += b * xerr;
			latot += a * xerr;
		    }
		}
		x++;
		xerr = xFixed1 - xerr;
	    }
	    rtot += (lrtot >> 10) * yerr;
	    gtot += (lgtot >> 10) * yerr;
	    btot += (lbtot >> 10) * yerr;
	    atot += (latot >> 10) * yerr;
	    y++;
	    yerr = xFixed1 - yerr;
	}
	if ((atot >>= 22) > 0xff) atot = 0xff;
	if ((rtot >>= 22) > 0xff) rtot = 0xff;
	if ((gtot >>= 22) > 0xff) gtot = 0xff;
	if ((btot >>= 22) > 0xff) btot = 0xff;
	bits = ((atot << 24) |
		(rtot << 16) |
		(gtot <<  8) |
		(btot       ));
	break;
    default:
	bits = 0;
	break;
    }
    return bits;
}

static IcAccessMap const icAccessMap[] = {
    /* 32bpp formats */
    { PICT_a8r8g8b8,	IcFetch_a8r8g8b8,	IcFetch_a8r8g8b8,	IcStore_a8r8g8b8 },
    { PICT_x8r8g8b8,	IcFetch_x8r8g8b8,	IcFetch_x8r8g8b8,	IcStore_x8r8g8b8 },
    { PICT_a8b8g8r8,	IcFetch_a8b8g8r8,	IcFetch_a8b8g8r8,	IcStore_a8b8g8r8 },
    { PICT_x8b8g8r8,	IcFetch_x8b8g8r8,	IcFetch_x8b8g8r8,	IcStore_x8b8g8r8 },

    /* 24bpp formats */
    { PICT_r8g8b8,	IcFetch_r8g8b8,		IcFetch_r8g8b8,		IcStore_r8g8b8 },
    { PICT_b8g8r8,	IcFetch_b8g8r8,		IcFetch_b8g8r8,		IcStore_b8g8r8 },

    /* 16bpp formats */
    { PICT_r5g6b5,	IcFetch_r5g6b5,		IcFetch_r5g6b5,		IcStore_r5g6b5 },
    { PICT_b5g6r5,	IcFetch_b5g6r5,		IcFetch_b5g6r5,		IcStore_b5g6r5 },

    { PICT_a1r5g5b5,	IcFetch_a1r5g5b5,	IcFetch_a1r5g5b5,	IcStore_a1r5g5b5 },
    { PICT_x1r5g5b5,	IcFetch_x1r5g5b5,	IcFetch_x1r5g5b5,	IcStore_x1r5g5b5 },
    { PICT_a1b5g5r5,	IcFetch_a1b5g5r5,	IcFetch_a1b5g5r5,	IcStore_a1b5g5r5 },
    { PICT_x1b5g5r5,	IcFetch_x1b5g5r5,	IcFetch_x1b5g5r5,	IcStore_x1b5g5r5 },
    { PICT_a4r4g4b4,	IcFetch_a4r4g4b4,	IcFetch_a4r4g4b4,	IcStore_a4r4g4b4 },
    { PICT_x4r4g4b4,	IcFetch_x4r4g4b4,	IcFetch_x4r4g4b4,	IcStore_x4r4g4b4 },
    { PICT_a4b4g4r4,	IcFetch_a4b4g4r4,	IcFetch_a4b4g4r4,	IcStore_a4b4g4r4 },
    { PICT_x4b4g4r4,	IcFetch_x4b4g4r4,	IcFetch_x4b4g4r4,	IcStore_x4b4g4r4 },

    /* 8bpp formats */
    { PICT_a8,		IcFetch_a8,		IcFetcha_a8,		IcStore_a8 },
    { PICT_r3g3b2,	IcFetch_r3g3b2,		IcFetch_r3g3b2,		IcStore_r3g3b2 },
    { PICT_b2g3r3,	IcFetch_b2g3r3,		IcFetch_b2g3r3,		IcStore_b2g3r3 },
    { PICT_a2r2g2b2,	IcFetch_a2r2g2b2,	IcFetch_a2r2g2b2,	IcStore_a2r2g2b2 },

    /* 4bpp formats */
    { PICT_a4,		IcFetch_a4,		IcFetcha_a4,		IcStore_a4 },
    { PICT_r1g2b1,	IcFetch_r1g2b1,		IcFetch_r1g2b1,		IcStore_r1g2b1 },
    { PICT_b1g2r1,	IcFetch_b1g2r1,		IcFetch_b1g2r1,		IcStore_b1g2r1 },
    { PICT_a1r1g1b1,	IcFetch_a1r1g1b1,	IcFetch_a1r1g1b1,	IcStore_a1r1g1b1 },
    { PICT_a1b1g1r1,	IcFetch_a1b1g1r1,	IcFetch_a1b1g1r1,	IcStore_a1b1g1r1 },

    /* 1bpp formats */
    { PICT_a1,		IcFetch_a1,		IcFetcha_a1,		IcStore_a1 },
};
#define NumAccessMap (sizeof icAccessMap / sizeof icAccessMap[0])

static void
IcStepOver (pixman_compositeOperand *op)
{
    op->u.drawable.offset += op->u.drawable.bpp;
}

static void
IcStepDown (pixman_compositeOperand *op)
{
    op->u.drawable.line += op->u.drawable.stride;
    op->u.drawable.offset = op->u.drawable.start_offset;
}

static void
IcSet (pixman_compositeOperand *op, int x, int y)
{
    op->u.drawable.line = op->u.drawable.top_line + y * op->u.drawable.stride;
    op->u.drawable.offset = op->u.drawable.left_offset + x * op->u.drawable.bpp;
}

static void
IcStepOver_external (pixman_compositeOperand *op)
{
    (*op[1].over) (&op[1]);
    (*op[2].over) (&op[2]);
}

static void
IcStepDown_external (pixman_compositeOperand *op)
{
    (*op[1].down) (&op[1]);
    (*op[2].down) (&op[2]);
}

static void
IcSet_external (pixman_compositeOperand *op, int x, int y)
{
    (*op[1].set) (&op[1], x, y);
    (*op[2].set) (&op[2], 
		  x - op->u.external.alpha_dx,
		  y - op->u.external.alpha_dy);
}

static void
IcStepOver_transform (pixman_compositeOperand *op)
{
    op->u.transform.x++;   
}

static void
IcStepDown_transform (pixman_compositeOperand *op)
{
    op->u.transform.y++;
    op->u.transform.x = op->u.transform.start_x;
}

static void
IcSet_transform (pixman_compositeOperand *op, int x, int y)
{
    op->u.transform.x = x - op->u.transform.left_x;
    op->u.transform.y = y - op->u.transform.top_y;
}


int
IcBuildCompositeOperand (pixman_image_t	    *image,
			 pixman_compositeOperand op[4],
			 int16_t		    x,
			 int16_t		    y,
			 int		    transform,
			 int		    alpha)
{
    /* Check for transform */
    if (transform && image->transform)
    {
	if (!IcBuildCompositeOperand (image, &op[1], 0, 0, 0, alpha))
	    return 0;
	
	op->u.transform.top_y = image->pixels->y;
	op->u.transform.left_x = image->pixels->x;
	
	op->u.transform.start_x = x - op->u.transform.left_x;
	op->u.transform.x = op->u.transform.start_x;
	op->u.transform.y = y - op->u.transform.top_y;
	op->u.transform.transform = image->transform;
	op->u.transform.filter = image->filter;
	op->u.transform.repeat = image->repeat;
	op->u.transform.width = image->pixels->width;
	op->u.transform.height = image->pixels->height;
	
	op->fetch = IcFetch_transform;
	op->fetcha = IcFetcha_transform;
	op->store = 0;
	op->over = IcStepOver_transform;
	op->down = IcStepDown_transform;
	op->set = IcSet_transform;

	op->clip = op[1].clip;
	
	return 1;
    }
    /* Check for external alpha */
    else if (alpha && image->alphaMap)
    {
	if (!IcBuildCompositeOperand (image, &op[1], x, y, 0, 0))
	    return 0;
	if (!IcBuildCompositeOperand (image->alphaMap, &op[2],
				      x - image->alphaOrigin.x,
				      y - image->alphaOrigin.y,
				      0, 0))
	    return 0;
	op->u.external.alpha_dx = image->alphaOrigin.x;
	op->u.external.alpha_dy = image->alphaOrigin.y;

	op->fetch = IcFetch_external;
	op->fetcha = IcFetcha_external;
	op->store = IcStore_external;
	op->over = IcStepOver_external;
	op->down = IcStepDown_external;
	op->set = IcSet_external;

	op->clip = op[1].clip;
	
	return 1;
    }
    /* Build simple operand */
    else
    {
	int	    i;
	int	    xoff, yoff;

	for (i = 0; i < NumAccessMap; i++)
	    if (icAccessMap[i].format_code == image->format_code)
	    {
		pixman_bits_t	*bits;
		IcStride	stride;
		int		bpp;

		op->fetch = icAccessMap[i].fetch;
		op->fetcha = icAccessMap[i].fetcha;
		op->store = icAccessMap[i].store;
		op->over = IcStepOver;
		op->down = IcStepDown;
		op->set = IcSet;

		op->clip = image->pCompositeClip;

		IcGetPixels (image->pixels, bits, stride, bpp,
			     xoff, yoff);
		if (image->repeat && image->pixels->width == 1 && 
		    image->pixels->height == 1)
		{
		    bpp = 0;
		    stride = 0;
		}
		/*
		 * Coordinates of upper left corner of drawable
		 */
		op->u.drawable.top_line = bits + yoff * stride;
		op->u.drawable.left_offset = xoff * bpp;

		/*
		 * Starting position within drawable
		 */
		op->u.drawable.start_offset = op->u.drawable.left_offset + x * bpp;
		op->u.drawable.line = op->u.drawable.top_line + y * stride;
		op->u.drawable.offset = op->u.drawable.start_offset;

		op->u.drawable.stride = stride;
		op->u.drawable.bpp = bpp;
		return 1;
	    }
	return 0;
    }
}

void
pixman_compositeGeneral (pixman_operator_t	op,
		    pixman_image_t	*iSrc,
		    pixman_image_t	*iMask,
		    pixman_image_t	*iDst,
		    int16_t	xSrc,
		    int16_t	ySrc,
		    int16_t	xMask,
		    int16_t	yMask,
		    int16_t	xDst,
		    int16_t	yDst,
		    uint16_t	width,
		    uint16_t	height)
{
    pixman_compositeOperand	src[4],msk[4],dst[4],*pmsk;
    pixman_compositeOperand	*srcPict, *srcAlpha;
    pixman_compositeOperand	*dstPict, *dstAlpha;
    pixman_compositeOperand	*mskPict = 0, *mskAlpha = 0;
    IcCombineFunc	f;
    int			w;

    if (!IcBuildCompositeOperand (iSrc, src, xSrc, ySrc, 1, 1))
	return;
    if (!IcBuildCompositeOperand (iDst, dst, xDst, yDst, 0, 1))
	return;
    if (iSrc->alphaMap)
    {
	srcPict = &src[1];
	srcAlpha = &src[2];
    }
    else
    {
	srcPict = &src[0];
	srcAlpha = 0;
    }
    if (iDst->alphaMap)
    {
	dstPict = &dst[1];
	dstAlpha = &dst[2];
    }
    else
    {
	dstPict = &dst[0];
	dstAlpha = 0;
    }
    f = IcCombineFuncU[op];
    if (iMask)
    {
	if (!IcBuildCompositeOperand (iMask, msk, xMask, yMask, 1, 1))
	    return;
	pmsk = msk;
	if (iMask->componentAlpha)
	    f = IcCombineFuncC[op];
	if (iMask->alphaMap)
	{
	    mskPict = &msk[1];
	    mskAlpha = &msk[2];
	}
	else
	{
	    mskPict = &msk[0];
	    mskAlpha = 0;
	}
    }
    else
	pmsk = 0;
    while (height--)
    {
	w = width;
	
	while (w--)
	{
	    (*f) (src, pmsk, dst);
	    (*src->over) (src);
	    (*dst->over) (dst);
	    if (pmsk)
		(*pmsk->over) (pmsk);
	}
	(*src->down) (src);
	(*dst->down) (dst);
	if (pmsk)
	    (*pmsk->down) (pmsk);
    }
}

