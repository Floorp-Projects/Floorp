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

#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "prio.h"
#include "plstr.h"

#include "Nonspr.h"

extern "C" {

struct PRFileDesc {
  FILE *fp;
};

/* Implementation of select NSPR routines for platforms that NSPR does
 * not currently support, or does not build on. Hopefully, this will
 * go away as NSPR progresses.
 */
PRStatus PR_GetFileInfo(const char *fn, PRFileInfo *info)
{
  struct stat sb;
  PRInt64 s, s2us;
  PRInt32 rv;

  rv = stat(fn, &sb);
  if (rv < 0)
    return PR_FAILURE;
  else if (info) {
    if (S_IFREG & sb.st_mode)
      info->type = PR_FILE_FILE ;
    else if (S_IFDIR & sb.st_mode)
      info->type = PR_FILE_DIRECTORY;
    else
      info->type = PR_FILE_OTHER;

    info->size = sb.st_size;
    LL_I2L(s, sb.st_mtime);
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_MUL(s, s, s2us);
    info->modifyTime = s;
    LL_I2L(s, sb.st_ctime);
    LL_MUL(s, s, s2us);
    info->creationTime = s;
  }
  return PR_SUCCESS;
}

PRFileDesc *PR_Open(const char *name, PRWord flags, PRWord)
{
  PRFileDesc *desc = (PRFileDesc *) malloc(sizeof(PRFileDesc));
  
  if (!desc)
    return 0;

  const char *readMode;
  switch (flags) {
  case PR_RDONLY:
    readMode = "rb";
    break;

  case PR_WRONLY:
    readMode = "wb";
    break;

  case PR_RDWR:
    readMode = "rw";
    break;

  case PR_APPEND:
    readMode = "wa";
    break;

  default:
    return 0;
  }
  
  if (!(desc->fp = fopen(name, readMode))) {
    free(desc);
    return 0;
  }

  return desc;
}

PRStatus PR_Close(PRFileDesc *desc)
{
  if (desc->fp)
    fclose(desc->fp);

  free(desc);
  return PR_SUCCESS;
}

PRInt32 PR_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
  return fread(buf, 1, amount, fd->fp);
}

PRInt32 PR_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
  return fwrite(buf, 1, amount, fd->fp);
}

PRInt32 PR_Seek(PRFileDesc *fd, PRInt32 offset, 
		PRSeekWhence prWhence)
{
  int whence;

  switch (prWhence) {
  case PR_SEEK_SET:
    whence = SEEK_SET;
    break;

  case PR_SEEK_CUR:
    whence = SEEK_CUR;
    break;
    
  case PR_SEEK_END:
    whence = SEEK_END;
    break;

  default:
    return -1;
  }
    
 if (fseek(fd->fp, offset, whence) < 0)
   return 0;
 else
   return ftell(fd->fp);
}

const char *PR_GetEnv(const char *var)
{
  return getenv(var);
}


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
//#include "primpl.h"
#include "prprf.h"
#include "prlong.h"
#include "prlog.h"
#include "prmem.h"

typedef struct SprintfStateStr SprintfState;

struct SprintfStateStr {
    int (*stuff)(SprintfState *ss, const char *sp, PRUint32 len);

    char *base;
    char *cur;
    PRUint32 maxlen;

    int (*func)(void *arg, const char *sp, PRUint32 len);
    void *arg;
};

#define TYPE_INT16	0
#define TYPE_UINT16	1
#define TYPE_INTN	2
#define TYPE_UINTN	3
#define TYPE_INT32	4
#define TYPE_UINT32	5
#define TYPE_INT64	6
#define TYPE_UINT64	7

#define _LEFT		0x1
#define _SIGNED		0x2
#define _SPACED		0x4
#define _ZEROS		0x8
#define _NEG		0x10

/*
** Fill into the buffer using the data in src
*/
static int fill2(SprintfState *ss, const char *src, int srclen, int width,
		int flags)
{
    char space = ' ';
    int rv;

    width -= srclen;
    if ((width > 0) && ((flags & _LEFT) == 0)) {	/* Right adjusting */
	if (flags & _ZEROS) {
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

    if ((width > 0) && ((flags & _LEFT) != 0)) {	/* Left adjusting */
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
** Fill a number. The order is: optional-sign zero-filling conversion-digits
*/
static int fill_n(SprintfState *ss, const char *src, int srclen, int width,
		  int prec, int type, int flags)
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
	if (flags & _NEG) {
	    sign = '-';
	    signwidth = 1;
	} else if (flags & _SIGNED) {
	    sign = '+';
	    signwidth = 1;
	} else if (flags & _SPACED) {
	    sign = ' ';
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

    if ((flags & _ZEROS) && (prec < 0)) {
	if (width > cvtwidth) {
	    zerowidth = width - cvtwidth;	/* Zero filling */
	    cvtwidth += zerowidth;
	}
    }

    if (flags & _LEFT) {
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
** Convert a long into its printable form
*/
static int cvt_l(SprintfState *ss, long num, int width, int prec, int radix,
		 int type, int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (num == 0)) {
	return 0;
    }

    /*
    ** Converting decimal is a little tricky. In the unsigned case we
    ** need to stop when we hit 10 digits. In the signed case, we can
    ** stop when the number is zero.
    */
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (num) {
	int digit = (((unsigned long)num) % radix) & 0xF;
	*--cvt = hexp[digit];
	digits++;
	num = ((unsigned long)num) / radix;
    }
    if (digits == 0) {
	*--cvt = '0';
	digits++;
    }

    /*
    ** Now that we have the number converted without its sign, deal with
    ** the sign and zero padding.
    */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
** Convert a 64-bit integer into its printable form
*/
static int cvt_ll(SprintfState *ss, PRInt64 num, int width, int prec, int radix,
		  int type, int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;
    PRInt64 rad;

    /* according to the man page this needs to happen */
    if ((prec == 0) && (LL_IS_ZERO(num))) {
	return 0;
    }

    /*
    ** Converting decimal is a little tricky. In the unsigned case we
    ** need to stop when we hit 10 digits. In the signed case, we can
    ** stop when the number is zero.
    */
    LL_I2L(rad, radix);
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (!LL_IS_ZERO(num)) {
	PRInt32 digit;
	PRInt64 quot, rem;
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
    ** Now that we have the number converted without its sign, deal with
    ** the sign and zero padding.
    */
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
** Convert a double precision floating point number into its printable
** form.
**
** XXX stop using sprintf to convert floating point
*/
static int cvt_f(SprintfState *ss, double d, const char *fmt0, const char *fmt1)
{
    char fin[20];
    char fout[300];
    int amount = fmt1 - fmt0;

    if (amount >= sizeof(fin)) {
	/* Totally bogus % command to sprintf. Just ignore it */
	return 0;
    }
    memcpy(fin, fmt0, amount);
    fin[amount] = 0;

    /* Convert floating point using the native sprintf code */
    sprintf(fout, fin, d);

    /*
    ** This assert will catch overflow's of fout, when building with
    ** debugging on. At least this way we can track down the evil piece
    ** of calling code and fix it!
    */

    return (*ss->stuff)(ss, fout, strlen(fout));
}

/*
** Convert a string into its printable form.  "width" is the output
** width. "prec" is the maximum number of characters of "s" to output,
** where -1 means until NUL.
*/
static int cvt_s(SprintfState *ss, const char *s, int width, int prec,
		 int flags)
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
** The workhorse sprintf code.
*/
static int dosprintf(SprintfState *ss, const char *fmt, va_list ap)
{
    char c;
    int flags, width, prec, radix, type;
    union {
	char ch;
	int i;
	long l;
	PRInt64 ll;
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

	/*
	 * Examine optional flags.  Note that we do not implement the
	 * '#' flag of sprintf().  The ANSI C spec. of the '#' flag is
	 * somewhat ambiguous and not ideal, which is perhaps why
	 * the various sprintf() implementations are inconsistent
	 * on this feature.
	 */
	while ((c == '-') || (c == '+') || (c == ' ') || (c == '0')) {
	    if (c == '-') flags |= _LEFT;
	    if (c == '+') flags |= _SIGNED;
	    if (c == ' ') flags |= _SPACED;
	    if (c == '0') flags |= _ZEROS;
	    c = *fmt++;
	}
	if (flags & _SIGNED) flags &= ~_SPACED;
	if (flags & _LEFT) flags &= ~_ZEROS;

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
	type = TYPE_INTN;
	if (c == 'h') {
	    type = TYPE_INT16;
	    c = *fmt++;
	} else if (c == 'L') {
	    /* XXX not quite sure here */
	    type = TYPE_INT64;
	    c = *fmt++;
	} else if (c == 'l') {
	    type = TYPE_INT32;
	    c = *fmt++;
	    if (c == 'l') {
		type = TYPE_INT64;
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
	      case TYPE_INT16:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
	      case TYPE_UINT16:
		u.l = va_arg(ap, int) & 0xffff;
		goto do_long;
	      case TYPE_INTN:
		u.l = va_arg(ap, int);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
	      case TYPE_UINTN:
		u.l = va_arg(ap, unsigned int);
		goto do_long;

	      case TYPE_INT32:
		u.l = va_arg(ap, PRInt32);
		if (u.l < 0) {
		    u.l = -u.l;
		    flags |= _NEG;
		}
		goto do_long;
	      case TYPE_UINT32:
		u.l = va_arg(ap, PRUint32);
	      do_long:
		rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
		if (rv < 0) {
		    return rv;
		}
		break;

	      case TYPE_INT64:
		u.ll = va_arg(ap, PRInt64);
		if (!LL_GE_ZERO(u.ll)) {
		    LL_NEG(u.ll, u.ll);
		    flags |= _NEG;
		}
		goto do_longlong;
	      case TYPE_UINT64:
		u.ll = va_arg(ap, PRUint64);
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
	    if (sizeof(void *) == sizeof(PRInt32)) {
	    	type = TYPE_UINT32;
	    } else if (sizeof(void *) == sizeof(PRInt64)) {
	    	type = TYPE_UINT64;
	    } else if (sizeof(void *) == sizeof(int)) {
		type = TYPE_UINTN;
	    } else {
		break;
	    }
	    radix = 16;
	    goto fetch_and_convert;

#if 0
	  case 'C':
	  case 'S':
	  case 'E':
	  case 'G':
	    /* XXX not supported I suppose */
	    break;
#endif

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
#if 0
	    PR_ASSERT(0);
#endif
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

static int FuncStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    int rv;

    rv = (*ss->func)(ss->arg, sp, len);
    if (rv < 0) {
	return rv;
    }
    ss->maxlen += len;
    return 0;
}

PR_IMPLEMENT(PRUint32) PR_sxprintf(PRStuffFunc func, void *arg, 
                                 const char *fmt, ...)
{
    va_list ap;
    int rv;

    va_start(ap, fmt);
    rv = PR_vsxprintf(func, arg, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(PRUint32) PR_vsxprintf(PRStuffFunc func, void *arg, 
                                  const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = FuncStuff;
    ss.func = func;
    ss.arg = arg;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    return (rv < 0) ? (PRUint32)-1 : ss.maxlen;
}

/*
** Stuff routine that automatically grows the malloc'd output buffer
** before it overflows.
*/
static int GrowStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    ptrdiff_t off;
    char *newbase;
    PRUint32 newlen;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
	/* Grow the buffer */
	newlen = ss->maxlen + ((len > 32) ? len : 32);
	if (ss->base) {
	    newbase = (char*) PR_REALLOC(ss->base, newlen);
	} else {
	    newbase = (char*) PR_MALLOC(newlen);
	}
	if (!newbase) {
	    /* Ran out of memory */
	    return -1;
	}
	ss->base = newbase;
	ss->maxlen = newlen;
	ss->cur = ss->base + off;
    }

    /* Copy data */
    while (len) {
	--len;
	*ss->cur++ = *sp++;
    }
    return 0;
}

/*
** sprintf into a malloc'd buffer
*/
PR_IMPLEMENT(char *) PR_smprintf(const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(char *) PR_vsmprintf(const char *fmt, va_list ap)
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
	    PR_DELETE(ss.base);
	}
	return 0;
    }
    return ss.base;
}

/*
** Stuff routine that discards overflow data
*/
static int LimitStuff(SprintfState *ss, const char *sp, PRUint32 len)
{
    PRUint32 limit = ss->maxlen - (ss->cur - ss->base);

    if (len > limit) {
	len = limit;
    }
    while (len) {
	--len;
	*ss->cur++ = *sp++;
    }
    return 0;
}

/*
** sprintf into a fixed size buffer. Make sure there is a NUL at the end
** when finished.
*/
PR_IMPLEMENT(PRUint32) PR_snprintf(char *out, PRUint32 outlen, const char *fmt, ...)
{
    va_list ap;
    int rv;

    if ((PRInt32)outlen <= 0) {
	return 0;
    }

    va_start(ap, fmt);
    rv = PR_vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(PRUint32) PR_vsnprintf(char *out, PRUint32 outlen,const char *fmt,
                                  va_list ap)
{
    SprintfState ss;
    PRUint32 n;

    if ((PRInt32)outlen <= 0) {
	return 0;
    }

    ss.stuff = LimitStuff;
    ss.base = out;
    ss.cur = out;
    ss.maxlen = outlen;
    (void) dosprintf(&ss, fmt, ap);

    /* If we added chars, and we didn't append a null, do it now. */
    if( (ss.cur != ss.base) && (*(ss.cur - 1) != '\0') )
        *(--ss.cur) = '\0';

    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

PR_IMPLEMENT(char *) PR_sprintf_append(char *last, const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = PR_vsprintf_append(last, fmt, ap);
    va_end(ap);
    return rv;
}

PR_IMPLEMENT(char *) PR_vsprintf_append(char *last, const char *fmt, va_list ap)
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
	    PR_DELETE(ss.base);
	}
	return 0;
    }
    return ss.base;
}

}
