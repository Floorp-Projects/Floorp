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

/*
 * Portable safe sprintf code.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prprintf.h"
#include "prlong.h"

typedef struct SprintfStateStr SprintfState;

struct SprintfStateStr {
    int (*stuff)(SprintfState *ss, const char *sp, size_t len);

    char *base;
    char *cur;
    size_t maxlen;

    int (*func)(void *arg, const char *sp, size_t len);
    void *arg;
};

#define PRTYPE_SHORT		0
#define PRTYPE_USHORT		1
#define PRTYPE_INT		2
#define PRTYPE_UINT		3
#define PRTYPE_LONG		4
#define PRTYPE_ULONG		5
#define PRTYPE_LONGLONG		6
#define PRTYPE_ULONGLONG	7

#define PRFORMAT_LEFT		0x1
#define PRFORMAT_SIGNED		0x2
#define PRFORMAT_SPACED		0x4
#define PRFORMAT_ZERO		0x8
#define PRFORMAT_NEG		0x10

#ifdef HAVE_LONG_LONG
#ifndef _WIN32
#define FETCH_LL(d,ap)		((d) = va_arg(ap, long long))
#define FETCH_ULL(d,ap)		((d) = va_arg(ap, unsigned long long))
#else
#define FETCH_LL(d,ap)		((d) = va_arg(ap, LONGLONG))
#define FETCH_ULL(d,ap)		((d) = va_arg(ap, DWORDLONG))
#endif
#else
#define FETCH_LL(d,ap)		(((long*)&(d))[0] = va_arg(ap, long),         \
				 ((long*)&(d))[1] = va_arg(ap, long))
#define FETCH_ULL(d,ap)		FETCH_LL(d,ap)
#endif

/*
 * Fill into the buffer using the data in src
 */
static int
fill2(SprintfState *ss, const char *src, int srclen, int width, int flags)
{
    char space = ' ';
    int rv;

    width -= srclen;
    if ((width > 0) && ((flags & PRFORMAT_LEFT) == 0)) {
	/* Right adjusting */
	if (flags & PRFORMAT_ZERO) {
	    space = '0';
	}
	while (--width >= 0) {
	    rv = (*ss->stuff)(ss, &space, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }

    /* Copy out the source data */
    rv = (*ss->stuff)(ss, src, srclen);
    if (rv < 0) {
	return rv;
    }

    if ((width > 0) && ((flags & PRFORMAT_LEFT) != 0)) {
	/* Left adjusting */
	while (--width >= 0) {
	    rv = (*ss->stuff)(ss, &space, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }
    return 0;
}

/*
 * Fill a number. The order is: optional-sign zero-filling conversion-digits
 */
static int
fill_n(SprintfState *ss, const char *src, int srclen, int width, int prec,
       int type, int flags)
{
    int zerowidth = 0;
    int precwidth = 0;
    int signwidth = 0;
    int leftspaces = 0;
    int rightspaces = 0;
    int cvtwidth;
    int rv;
    char sign;

    if ((type & 1) == 0) {
	if (flags & PRFORMAT_NEG) {
	    sign = '-';
	    signwidth = 1;
	} else if (flags & PRFORMAT_SIGNED) {
	    sign = '+';
	    signwidth = 1;
	}
    }
    cvtwidth = signwidth + srclen;

    if (prec > 0) {
	if (prec > srclen) {
	    precwidth = prec - srclen;		/* Need zero filling */
	    cvtwidth += precwidth;
	}
    }

    if ((flags & PRFORMAT_ZERO) && (prec < 0)) {
	if (width > cvtwidth) {
	    zerowidth = width - cvtwidth;	/* Zero filling */
	    cvtwidth += zerowidth;
	}
    }

    if (flags & PRFORMAT_LEFT) {
	if (width > cvtwidth) {
	    /* Space filling on the right (i.e. left adjusting) */
	    rightspaces = width - cvtwidth;
	}
    } else {
	if (width > cvtwidth) {
	    /* Space filling on the left (i.e. right adjusting) */
	    leftspaces = width - cvtwidth;
	}
    }
    while (--leftspaces >= 0) {
	rv = (*ss->stuff)(ss, " ", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    if (signwidth) {
	rv = (*ss->stuff)(ss, &sign, 1);
	if (rv < 0) {
	    return rv;
	}
    }
    while (--precwidth >= 0) {
	rv = (*ss->stuff)(ss, "0", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    while (--zerowidth >= 0) {
	rv = (*ss->stuff)(ss, "0", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    rv = (*ss->stuff)(ss, src, srclen);
    if (rv < 0) {
	return rv;
    }
    while (--rightspaces >= 0) {
	rv = (*ss->stuff)(ss, " ", 1);
	if (rv < 0) {
	    return rv;
	}
    }
    return 0;
}

/*
 * Convert a long into its printable form
 */
static int
cvt_l(SprintfState *ss, long num, int width, int prec, int radix, int type,
      int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (num == 0)) {
	return 0;
    }

    /*
     * Converting decimal is a little tricky. In the unsigned case we
     * need to stop when we hit 10 digits. In the signed case, we can
     * stop when the number is zero.
     */
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (num) {
	int digit = (int)((((unsigned long)num) % radix) & 0xF);
	*--cvt = hexp[digit];
	digits++;
	num = ((unsigned long)num) / radix;
    }
    if (digits == 0) {
	*--cvt = '0';
	digits++;
    }

    /*
     * Now that we have the number converted without its sign, deal with
     * the sign and zero padding.
     */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
 * Convert a longlong into its printable form
 */
static int
cvt_ll(SprintfState *ss, int64 num, int width, int prec, int radix, int type,
       int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;
    int64 rad;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (LL_IS_ZERO(num))) {
	return 0;
    }

    /*
     * Converting decimal is a little tricky. In the unsigned case we
     * need to stop when we hit 10 digits. In the signed case, we can
     * stop when the number is zero.
     */
    LL_I2L(rad, radix);
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (!LL_IS_ZERO(num)) {
	int32 digit;
	int64 quot, rem;
	LL_UDIVMOD(&quot, &rem, num, rad);
	LL_L2I(digit, rem);
	*--cvt = hexp[digit & 0xf];
	digits++;
	num = quot;
    }
    if (digits == 0) {
	*--cvt = '0';
	digits++;
    }

    /*
     * Now that we have the number converted without its sign, deal with
     * the sign and zero padding.
     */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
 * Convert a double precision floating point number into its printable
 * form.
 *
 * XXX stop using sprintf to convert floating point
 */
static int
cvt_f(SprintfState *ss, double d, const char *fmt0, const char *fmt1)
{
    char fin[20];
    char fout[300];
    int amount = fmt1 - fmt0;

    PR_ASSERT((amount > 0) && (amount < sizeof(fin)));
    if (amount >= sizeof(fin)) {
	/* Totally bogus % command to sprintf. Just ignore it */
	return 0;
    }
    memcpy(fin, fmt0, amount);
    fin[amount] = 0;

    /* Convert floating point using the native sprintf code */
    sprintf(fout, fin, (double)d);

    /*
     * This assert will catch overflow's of fout, when building with
     * debugging on. At least this way we can track down the evil piece
     * of calling code and fix it!
     */
    PR_ASSERT(strlen(fout) < sizeof(fout));

    return (*ss->stuff)(ss, fout, strlen(fout));
}

/*
 * Convert a string into its printable form.  "width" is the output
 * width. "prec" is the maximum number of characters of "s" to output,
 * where -1 means until NUL.
 */
static int
cvt_s(SprintfState *ss, const char *s, int width, int prec, int flags)
{
    int slen;

    if (prec == 0)
	return 0;

    /* Limit string length by precision value */
    slen = s ? strlen(s) : 6;
    if (prec > 0) {
	if (prec < slen) {
	    slen = prec;
	}
    }

    /* and away we go */
    return fill2(ss, s ? s : "(null)", slen, width, flags);
}

/*
 * The workhorse sprintf code.
 */
static int
dosprintf(SprintfState *ss, const char *fmt, va_list ap)
{
    char c;
    int flags, width, prec, radix, type;
    union {
	char ch;
	int i;
	long l;
	int64 ll;
	double d;
	const char *s;
	int *ip;
    } u;
    const char *fmt0;
    static char *hex = "0123456789abcdef";
    static char *HEX = "0123456789ABCDEF";
    char *hexp;
    int rv;

    while ((c = *fmt++) != 0) {
	if (c != '%') {
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	    continue;
	}
	fmt0 = fmt - 1;

	/*
	** Gobble up the % format string. Hopefully we have handled all
	** of the strange cases!
	*/
	flags = 0;
	c = *fmt++;
	if (c == '%') {
	    /* quoting a % with %% */
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	    continue;
	}

	/* examine optional flags */
	while ((c == '-') || (c == '+') || (c == ' ') || (c == '0')) {
	    if (c == '-') flags |= PRFORMAT_LEFT;
	    if (c == '+') flags |= PRFORMAT_SIGNED;
	    if (c == ' ') flags |= PRFORMAT_SPACED;
	    if (c == '0') flags |= PRFORMAT_ZERO;
	    c = *fmt++;
	}
	if (flags & PRFORMAT_SIGNED) flags &= ~PRFORMAT_SPACED;
	if (flags & PRFORMAT_LEFT) flags &= ~PRFORMAT_ZERO;

	/* width */
	if (c == '*') {
	    c = *fmt++;
	    width = va_arg(ap, int);
	} else {
	    width = 0;
	    while ((c >= '0') && (c <= '9')) {
		width = (width * 10) + (c - '0');
		c = *fmt++;
	    }
	}

	/* precision */
	prec = -1;
	if (c == '.') {
	    c = *fmt++;
	    if (c == '*') {
		c = *fmt++;
		prec = va_arg(ap, int);
	    } else {
		prec = 0;
		while ((c >= '0') && (c <= '9')) {
		    prec = (prec * 10) + (c - '0');
		    c = *fmt++;
		}
	    }
	}

	/* size */
	type = PRTYPE_INT;
	if (c == 'h') {
	    type = PRTYPE_SHORT;
	    c = *fmt++;
	} else if (c == 'L') {
	    /* XXX not quite sure here */
	    type = PRTYPE_LONGLONG;
	    c = *fmt++;
	} else if (c == 'l') {
	    type = PRTYPE_LONG;
	    c = *fmt++;
	    if (c == 'l') {
		type = PRTYPE_LONGLONG;
		c = *fmt++;
	    }
	}

	/* format */
	hexp = hex;
	switch (c) {
	  case 'd': case 'i':			/* decimal/integer */
	    radix = 10;
	    goto fetch_and_convert;

	  case 'o':				/* octal */
	    radix = 8;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'u':				/* unsigned decimal */
	    radix = 10;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'x':				/* unsigned hex */
	    radix = 16;
	    type |= 1;
	    goto fetch_and_convert;

	  case 'X':				/* unsigned HEX */
	    radix = 16;
	    hexp = HEX;
	    type |= 1;
	    goto fetch_and_convert;

	  fetch_and_convert:
	    switch (type) {
	      case PRTYPE_SHORT:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= PRFORMAT_NEG;
		}
		goto do_long;
	      case PRTYPE_USHORT:
		u.l = va_arg(ap, int) & 0xffff;
		goto do_long;
	      case PRTYPE_INT:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= PRFORMAT_NEG;
		}
		goto do_long;
	      case PRTYPE_UINT:
		u.l = va_arg(ap, int) & 0xffffffffL;
		goto do_long;

	      case PRTYPE_LONG:
		u.l = va_arg(ap, long);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= PRFORMAT_NEG;
		}
		goto do_long;
	      case PRTYPE_ULONG:
		u.l = va_arg(ap, unsigned long);
	      do_long:
		rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;

	      case PRTYPE_LONGLONG:
		FETCH_LL(u.ll, ap);
		if (!LL_GE_ZERO(u.ll)) {
		    LL_NEG(u.ll, u.ll);
		    flags |= PRFORMAT_NEG;
		}
		goto do_longlong;
	      case PRTYPE_ULONGLONG:
		FETCH_ULL(u.ll, ap);
	      do_longlong:
		rv = cvt_ll(ss, u.ll, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;
	    }
	    break;

	  case 'e':
	  case 'f':
	  case 'g':
	    u.d = va_arg(ap, double);
	    rv = cvt_f(ss, u.d, fmt0, fmt);
	    if (rv < 0) {
		return rv;
	    }
	    break;

	  case 'c':
	    u.ch = va_arg(ap, int);
	    rv = (*ss->stuff)(ss, &u.ch, 1);
	    if (rv < 0) {
		return rv;
	    }
	    break;

	  case 'p':
	    if (sizeof(void *) == sizeof(long)) {
	    	type = PRTYPE_ULONG;
	    } else if (sizeof(void *) == sizeof(int64)) {
	    	type = PRTYPE_ULONGLONG;
	    } else if (sizeof(void *) == sizeof(int)) {
		type = PRTYPE_UINT;
	    } else {
		PR_ASSERT(0);
		break;
	    }
	    radix = 16;
	    goto fetch_and_convert;

	  case 'C':
	  case 'S':
	  case 'E':
	  case 'G':
	    /* XXX not supported I suppose */
	    PR_ASSERT(0);
	    break;

	  case 's':
	    u.s = va_arg(ap, const char*);
	    rv = cvt_s(ss, u.s, width, prec, flags);
	    if (rv < 0) {
		return rv;
	    }
	    break;

	  case 'n':
	    u.ip = va_arg(ap, int*);
	    if (u.ip) {
		*u.ip = ss->cur - ss->base;
	    }
	    break;

	  default:
	    /* Not a % token after all... skip it */
	    PR_ASSERT(0);
	    rv = (*ss->stuff)(ss, "%", 1);
	    if (rv < 0) {
		return rv;
	    }
	    rv = (*ss->stuff)(ss, fmt - 1, 1);
	    if (rv < 0) {
		return rv;
	    }
	}
    }

    /* Stuff trailing NUL */
    rv = (*ss->stuff)(ss, "\0", 1);
    return rv;
}

/************************************************************************/

static int
FuncStuff(SprintfState *ss, const char *sp, size_t len)
{
    int rv;

    rv = (*ss->func)(ss->arg, sp, len);
    if (rv < 0) {
	return rv;
    }
    ss->maxlen += len;
    return 0;
}

PR_PUBLIC_API(size_t)
PR_sxprintf(PR_sxprintf_callback func, void *arg, const char *fmt, ...)
{
    va_list ap;
    int rv;

    va_start(ap, fmt);
    rv = PR_vsxprintf(func, arg, fmt, ap);
    va_end(ap);
    return rv;
}

PR_PUBLIC_API(size_t)
PR_vsxprintf(PR_vsxprintf_callback func, void *arg, const char *fmt,
	     va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = FuncStuff;
    ss.func = func;
    ss.arg = arg;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    return (rv < 0) ? (size_t)-1 : ss.maxlen;
}

/*
 * Stuff routine that automatically grows the malloc'd output buffer
 * before it overflows.
 */
static int
GrowStuff(SprintfState *ss, const char *sp, size_t len)
{
    ptrdiff_t off;
    char *newbase;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
	/* Grow the buffer */
	ss->maxlen += (len > 32) ? len : 32;
	if (ss->base) {
	    newbase = (char*) realloc(ss->base, ss->maxlen);
	} else {
	    newbase = (char*) malloc(ss->maxlen);
	}
	if (!newbase) {
	    /* Ran out of memory */
	    return -1;
	}
	ss->base = newbase;
	ss->cur = ss->base + off;
    }

    /* Copy data */
    while (len) {
	--len;
	*ss->cur++ = *sp++;
    }
    PR_ASSERT((size_t)(ss->cur - ss->base) <= ss->maxlen);
    return 0;
}

/*
 * sprintf into a malloc'd buffer
 */
PR_PUBLIC_API(char *)
PR_smprintf(const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

PR_PUBLIC_API(char *)
PR_vsmprintf(const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    ss.base = 0;
    ss.cur = 0;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    if (rv < 0) {
	if (ss.base) {
	    free(ss.base);
	}
	return 0;
    }
    return ss.base;
}

/*
 * Stuff routine that discards overflow data
 */
static int
LimitStuff(SprintfState *ss, const char *sp, size_t len)
{
    char *cp = ss->cur;
    size_t limit = ss->maxlen - (cp - ss->base);

    if (len > limit) {
	len = limit;
    }
    while (len) {
	--len;
	*cp++ = *sp++;
    }
    ss->cur = cp;
    return 0;
}

/*
 * sprintf into a fixed size buffer. Make sure there is a NUL at the end
 * when finished.
 */
PR_PUBLIC_API(size_t)
PR_snprintf(char *out, size_t outlen, const char *fmt, ...)
{
    va_list ap;
    int rv;

    PR_ASSERT((int)outlen > 0);
    if ((int32)outlen < 0) {
	return 0;
    }

    va_start(ap, fmt);
    rv = PR_vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

PR_PUBLIC_API(size_t)
PR_vsnprintf(char *out, size_t outlen, const char *fmt, va_list ap)
{
    SprintfState ss;
    size_t n;

    PR_ASSERT((int)outlen > 0);
    if ((int32)outlen < 0) {
	return 0;
    }

    ss.stuff = LimitStuff;
    ss.base = out;
    ss.cur = out;
    ss.maxlen = outlen;
    (void) dosprintf(&ss, fmt, ap);

    /* Make sure there is a NUL at the end */
    if (outlen) {
	out[outlen-1] = 0;
    }
    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

PR_PUBLIC_API(char *)
PR_sprintf_append(char *last, const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsprintf_append(last, fmt, ap);
    va_end(ap);
    return rv;
}

PR_PUBLIC_API(char *)
PR_vsprintf_append(char *last, const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    if (last) {
	int lastlen = strlen(last);
	ss.base = last;
	ss.cur = last + lastlen;
	ss.maxlen = lastlen;
    } else {
	ss.base = 0;
	ss.cur = 0;
	ss.maxlen = 0;
    }
    rv = dosprintf(&ss, fmt, ap);
    if (rv < 0) {
	if (ss.base) {
	    free(ss.base);
	}
	return 0;
    }
    return ss.base;
}
