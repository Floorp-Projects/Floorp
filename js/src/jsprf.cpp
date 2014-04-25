/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Portable safe sprintf code.
 *
 * Author: Kipp E.B. Hickman
 */

#include "jsprf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jspubtd.h"
#include "jsstr.h"
#include "jsutil.h"

using namespace js;

/*
 * Note: on some platforms va_list is defined as an array,
 * and requires array notation.
 */
#ifdef HAVE_VA_COPY
#define VARARGS_ASSIGN(foo, bar)        VA_COPY(foo, bar)
#elif defined(HAVE_VA_LIST_AS_ARRAY)
#define VARARGS_ASSIGN(foo, bar)        foo[0] = bar[0]
#else
#define VARARGS_ASSIGN(foo, bar)        (foo) = (bar)
#endif

struct SprintfState
{
    int (*stuff)(SprintfState *ss, const char *sp, size_t len);

    char *base;
    char *cur;
    size_t maxlen;

    int (*func)(void *arg, const char *sp, uint32_t len);
    void *arg;
};

/*
 * Numbered Argument State
 */
struct NumArgState
{
    int type;       // type of the current ap
    va_list ap;     // point to the corresponding position on ap
};

const size_t NAS_DEFAULT_NUM = 20;  // default number of NumberedArgumentState array


#define TYPE_INT16      0
#define TYPE_UINT16     1
#define TYPE_INTN       2
#define TYPE_UINTN      3
#define TYPE_INT32      4
#define TYPE_UINT32     5
#define TYPE_INT64      6
#define TYPE_UINT64     7
#define TYPE_STRING     8
#define TYPE_DOUBLE     9
#define TYPE_INTSTR     10
#define TYPE_WSTRING    11
#define TYPE_UNKNOWN    20

#define FLAG_LEFT       0x1
#define FLAG_SIGNED     0x2
#define FLAG_SPACED     0x4
#define FLAG_ZEROS      0x8
#define FLAG_NEG        0x10

inline int
generic_write(SprintfState *ss, const char *src, size_t srclen)
{
    return (*ss->stuff)(ss, src, srclen);
}

inline int
generic_write(SprintfState *ss, const jschar *src, size_t srclen)
{
    const size_t CHUNK_SIZE = 64;
    char chunk[CHUNK_SIZE];

    int rv = 0;
    size_t j = 0;
    size_t i = 0;
    while (i < srclen) {
        // FIXME: truncates characters to 8 bits
        chunk[j++] = char(src[i++]);

        if (j == CHUNK_SIZE || i == srclen) {
            rv = (*ss->stuff)(ss, chunk, j);
            if (rv != 0)
                return rv;
            j = 0;
        }
    }
    return 0;
}

// Fill into the buffer using the data in src
template <typename Char>
static int
fill2(SprintfState *ss, const Char *src, int srclen, int width, int flags)
{
    char space = ' ';
    int rv;

    width -= srclen;
    if (width > 0 && (flags & FLAG_LEFT) == 0) {    // Right adjusting
        if (flags & FLAG_ZEROS)
            space = '0';
        while (--width >= 0) {
            rv = (*ss->stuff)(ss, &space, 1);
            if (rv < 0)
                return rv;
        }
    }

    // Copy out the source data
    rv = generic_write(ss, src, srclen);
    if (rv < 0)
        return rv;

    if (width > 0 && (flags & FLAG_LEFT) != 0) {    // Left adjusting
        while (--width >= 0) {
            rv = (*ss->stuff)(ss, &space, 1);
            if (rv < 0)
                return rv;
        }
    }
    return 0;
}

/*
 * Fill a number. The order is: optional-sign zero-filling conversion-digits
 */
static int
fill_n(SprintfState *ss, const char *src, int srclen, int width, int prec, int type, int flags)
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
        if (flags & FLAG_NEG) {
            sign = '-';
            signwidth = 1;
        } else if (flags & FLAG_SIGNED) {
            sign = '+';
            signwidth = 1;
        } else if (flags & FLAG_SPACED) {
            sign = ' ';
            signwidth = 1;
        }
    }
    cvtwidth = signwidth + srclen;

    if (prec > 0) {
        if (prec > srclen) {
            precwidth = prec - srclen;          // Need zero filling
            cvtwidth += precwidth;
        }
    }

    if ((flags & FLAG_ZEROS) && (prec < 0)) {
        if (width > cvtwidth) {
            zerowidth = width - cvtwidth;       // Zero filling
            cvtwidth += zerowidth;
        }
    }

    if (flags & FLAG_LEFT) {
        if (width > cvtwidth) {
            // Space filling on the right (i.e. left adjusting)
            rightspaces = width - cvtwidth;
        }
    } else {
        if (width > cvtwidth) {
            // Space filling on the left (i.e. right adjusting)
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
    rv = (*ss->stuff)(ss, src, uint32_t(srclen));
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

/* Convert a long into its printable form. */
static int cvt_l(SprintfState *ss, long num, int width, int prec, int radix,
                 int type, int flags, const char *hexp)
{
    char cvtbuf[100];
    char *cvt;
    int digits;

    // according to the man page this needs to happen
    if ((prec == 0) && (num == 0)) {
        return 0;
    }

    // Converting decimal is a little tricky. In the unsigned case we
    // need to stop when we hit 10 digits. In the signed case, we can
    // stop when the number is zero.
    cvt = cvtbuf + sizeof(cvtbuf);
    digits = 0;
    while (num) {
        int digit = (((unsigned long)num) % radix) & 0xF;
        *--cvt = hexp[digit];
        digits++;
        num = (long)(((unsigned long)num) / radix);
    }
    if (digits == 0) {
        *--cvt = '0';
        digits++;
    }

    // Now that we have the number converted without its sign, deal with
    // the sign and zero padding.
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/* Convert a 64-bit integer into its printable form. */
static int cvt_ll(SprintfState *ss, int64_t num, int width, int prec, int radix,
                  int type, int flags, const char *hexp)
{
    // According to the man page, this needs to happen.
    if (prec == 0 && num == 0)
        return 0;

    // Converting decimal is a little tricky. In the unsigned case we
    // need to stop when we hit 10 digits. In the signed case, we can
    // stop when the number is zero.
    int64_t rad = int64_t(radix);
    char cvtbuf[100];
    char *cvt = cvtbuf + sizeof(cvtbuf);
    int digits = 0;
    while (num != 0) {
        int64_t quot = uint64_t(num) / rad;
        int64_t rem = uint64_t(num) % rad;
        int32_t digit = int32_t(rem);
        *--cvt = hexp[digit & 0xf];
        digits++;
        num = quot;
    }
    if (digits == 0) {
        *--cvt = '0';
        digits++;
    }

    // Now that we have the number converted without its sign, deal with
    // the sign and zero padding.
    return fill_n(ss, cvt, digits, width, prec, type, flags);
}

/*
 * Convert a double precision floating point number into its printable
 * form.
 *
 * XXX stop using sprintf to convert floating point
 */
static int cvt_f(SprintfState *ss, double d, const char *fmt0, const char *fmt1)
{
    char fin[20];
    char fout[300];
    int amount = fmt1 - fmt0;

    JS_ASSERT((amount > 0) && (amount < (int)sizeof(fin)));
    if (amount >= (int)sizeof(fin)) {
        // Totally bogus % command to sprintf. Just ignore it
        return 0;
    }
    js_memcpy(fin, fmt0, (size_t)amount);
    fin[amount] = 0;

    // Convert floating point using the native sprintf code
#ifdef DEBUG
    {
        const char *p = fin;
        while (*p) {
            JS_ASSERT(*p != 'L');
            p++;
        }
    }
#endif
    sprintf(fout, fin, d);

    // This assert will catch overflow's of fout, when building with
    // debugging on. At least this way we can track down the evil piece
    // of calling code and fix it!
    JS_ASSERT(strlen(fout) < sizeof(fout));

    return (*ss->stuff)(ss, fout, strlen(fout));
}

static inline const char *generic_null_str(const char *) { return "(null)"; }
static inline const jschar *generic_null_str(const jschar *) { return MOZ_UTF16("(null)"); }

static inline size_t generic_strlen(const char *s) { return strlen(s); }
static inline size_t generic_strlen(const jschar *s) { return js_strlen(s); }

/*
 * Convert a string into its printable form.  "width" is the output
 * width. "prec" is the maximum number of characters of "s" to output,
 * where -1 means until NUL.
 */
template <typename Char>
static int
cvt_s(SprintfState *ss, const Char *s, int width, int prec, int flags)
{
    if (prec == 0)
        return 0;
    if (!s)
        s = generic_null_str(s);

    // Limit string length by precision value
    int slen = int(generic_strlen(s));
    if (0 < prec && prec < slen)
        slen = prec;

    // and away we go
    return fill2(ss, s, slen, width, flags);
}

/*
 * BuildArgArray stands for Numbered Argument list Sprintf
 * for example,
 *      fmp = "%4$i, %2$d, %3s, %1d";
 * the number must start from 1, and no gap among them
 */
static NumArgState *
BuildArgArray(const char *fmt, va_list ap, int *rv, NumArgState *nasArray)
{
    size_t number = 0, cn = 0, i;
    const char *p;
    char c;
    NumArgState *nas;


    // First pass:
    // Detemine how many legal % I have got, then allocate space.

    p = fmt;
    *rv = 0;
    i = 0;
    while ((c = *p++) != 0) {
        if (c != '%')
            continue;
        if ((c = *p++) == '%')          // skip %% case
            continue;

        while (c != 0) {
            if (c > '9' || c < '0') {
                if (c == '$') {         // numbered argument case
                    if (i > 0) {
                        *rv = -1;
                        return nullptr;
                    }
                    number++;
                } else {                // non-numbered argument case
                    if (number > 0) {
                        *rv = -1;
                        return nullptr;
                    }
                    i = 1;
                }
                break;
            }

            c = *p++;
        }
    }

    if (number == 0)
        return nullptr;

    if (number > NAS_DEFAULT_NUM) {
        nas = (NumArgState *) js_malloc(number * sizeof(NumArgState));
        if (!nas) {
            *rv = -1;
            return nullptr;
        }
    } else {
        nas = nasArray;
    }

    for (i = 0; i < number; i++)
        nas[i].type = TYPE_UNKNOWN;


    // Second pass:
    // Set nas[].type.

    p = fmt;
    while ((c = *p++) != 0) {
        if (c != '%')
            continue;
        c = *p++;
        if (c == '%')
            continue;

        cn = 0;
        while (c && c != '$') {     // should improve error check later
            cn = cn*10 + c - '0';
            c = *p++;
        }

        if (!c || cn < 1 || cn > number) {
            *rv = -1;
            break;
        }

        // nas[cn] starts from 0, and make sure nas[cn].type is not assigned.
        cn--;
        if (nas[cn].type != TYPE_UNKNOWN)
            continue;

        c = *p++;

        // width
        if (c == '*') {
            // not supported feature, for the argument is not numbered
            *rv = -1;
            break;
        }

        while ((c >= '0') && (c <= '9')) {
            c = *p++;
        }

        // precision
        if (c == '.') {
            c = *p++;
            if (c == '*') {
                // not supported feature, for the argument is not numbered
                *rv = -1;
                break;
            }

            while ((c >= '0') && (c <= '9')) {
                c = *p++;
            }
        }

        // size
        nas[cn].type = TYPE_INTN;
        if (c == 'h') {
            nas[cn].type = TYPE_INT16;
            c = *p++;
        } else if (c == 'L') {
            // XXX not quite sure here
            nas[cn].type = TYPE_INT64;
            c = *p++;
        } else if (c == 'l') {
            nas[cn].type = TYPE_INT32;
            c = *p++;
            if (c == 'l') {
                nas[cn].type = TYPE_INT64;
                c = *p++;
            }
        }

        // format
        switch (c) {
        case 'd':
        case 'c':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            break;

        case 'e':
        case 'f':
        case 'g':
            nas[cn].type = TYPE_DOUBLE;
            break;

        case 'p':
            // XXX should use cpp
            if (sizeof(void *) == sizeof(int32_t)) {
                nas[cn].type = TYPE_UINT32;
            } else if (sizeof(void *) == sizeof(int64_t)) {
                nas[cn].type = TYPE_UINT64;
            } else if (sizeof(void *) == sizeof(int)) {
                nas[cn].type = TYPE_UINTN;
            } else {
                nas[cn].type = TYPE_UNKNOWN;
            }
            break;

        case 'C':
        case 'S':
        case 'E':
        case 'G':
            // XXX not supported I suppose
            JS_ASSERT(0);
            nas[cn].type = TYPE_UNKNOWN;
            break;

        case 's':
            nas[cn].type = (nas[cn].type == TYPE_UINT16) ? TYPE_WSTRING : TYPE_STRING;
            break;

        case 'n':
            nas[cn].type = TYPE_INTSTR;
            break;

        default:
            JS_ASSERT(0);
            nas[cn].type = TYPE_UNKNOWN;
            break;
        }

        // get a legal para.
        if (nas[cn].type == TYPE_UNKNOWN) {
            *rv = -1;
            break;
        }
    }


    // Third pass:
    // Fill nas[].ap.

    if (*rv < 0) {
        if (nas != nasArray)
            js_free(nas);
        return nullptr;
    }

    cn = 0;
    while (cn < number) {
        if (nas[cn].type == TYPE_UNKNOWN) {
            cn++;
            continue;
        }

        VARARGS_ASSIGN(nas[cn].ap, ap);

        switch (nas[cn].type) {
        case TYPE_INT16:
        case TYPE_UINT16:
        case TYPE_INTN:
        case TYPE_UINTN:        (void) va_arg(ap, int);         break;
        case TYPE_INT32:        (void) va_arg(ap, int32_t);     break;
        case TYPE_UINT32:       (void) va_arg(ap, uint32_t);    break;
        case TYPE_INT64:        (void) va_arg(ap, int64_t);     break;
        case TYPE_UINT64:       (void) va_arg(ap, uint64_t);    break;
        case TYPE_STRING:       (void) va_arg(ap, char*);       break;
        case TYPE_WSTRING:      (void) va_arg(ap, jschar*);     break;
        case TYPE_INTSTR:       (void) va_arg(ap, int*);        break;
        case TYPE_DOUBLE:       (void) va_arg(ap, double);      break;

        default:
            if (nas != nasArray)
                js_free(nas);
            *rv = -1;
            return nullptr;
        }

        cn++;
    }


    return nas;
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
        jschar wch;
        int i;
        long l;
        int64_t ll;
        double d;
        const char *s;
        const jschar* ws;
        int *ip;
    } u;
    const char *fmt0;
    static const char hex[] = "0123456789abcdef";
    static const char HEX[] = "0123456789ABCDEF";
    const char *hexp;
    int rv, i;
    NumArgState *nas = nullptr;
    NumArgState nasArray[NAS_DEFAULT_NUM];
    char pattern[20];
    const char *dolPt = nullptr;  // in "%4$.2f", dolPt will point to '.'

    // Build an argument array, IF the fmt is numbered argument
    // list style, to contain the Numbered Argument list pointers.

    nas = BuildArgArray(fmt, ap, &rv, nasArray);
    if (rv < 0) {
        // the fmt contains error Numbered Argument format, jliu@netscape.com
        JS_ASSERT(0);
        return rv;
    }

    while ((c = *fmt++) != 0) {
        if (c != '%') {
            rv = (*ss->stuff)(ss, fmt - 1, 1);
            if (rv < 0) {
                return rv;
            }
            continue;
        }
        fmt0 = fmt - 1;

        // Gobble up the % format string. Hopefully we have handled all
        // of the strange cases!
        flags = 0;
        c = *fmt++;
        if (c == '%') {
            // quoting a % with %%
            rv = (*ss->stuff)(ss, fmt - 1, 1);
            if (rv < 0) {
                return rv;
            }
            continue;
        }

        if (nas != nullptr) {
            // the fmt contains the Numbered Arguments feature
            i = 0;
            while (c && c != '$') {         // should improve error check later
                i = (i * 10) + (c - '0');
                c = *fmt++;
            }

            if (nas[i-1].type == TYPE_UNKNOWN) {
                if (nas && nas != nasArray)
                    js_free(nas);
                return -1;
            }

            ap = nas[i-1].ap;
            dolPt = fmt;
            c = *fmt++;
        }

        // Examine optional flags.  Note that we do not implement the
        // '#' flag of sprintf().  The ANSI C spec. of the '#' flag is
        // somewhat ambiguous and not ideal, which is perhaps why
        // the various sprintf() implementations are inconsistent
        // on this feature.
        while ((c == '-') || (c == '+') || (c == ' ') || (c == '0')) {
            if (c == '-') flags |= FLAG_LEFT;
            if (c == '+') flags |= FLAG_SIGNED;
            if (c == ' ') flags |= FLAG_SPACED;
            if (c == '0') flags |= FLAG_ZEROS;
            c = *fmt++;
        }
        if (flags & FLAG_SIGNED) flags &= ~FLAG_SPACED;
        if (flags & FLAG_LEFT) flags &= ~FLAG_ZEROS;

        // width
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

        // precision
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

        // size
        type = TYPE_INTN;
        if (c == 'h') {
            type = TYPE_INT16;
            c = *fmt++;
        } else if (c == 'L') {
            // XXX not quite sure here
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

        // format
        hexp = hex;
        switch (c) {
          case 'd': case 'i':                   // decimal/integer
            radix = 10;
            goto fetch_and_convert;

          case 'o':                             // octal
            radix = 8;
            type |= 1;
            goto fetch_and_convert;

          case 'u':                             // unsigned decimal
            radix = 10;
            type |= 1;
            goto fetch_and_convert;

          case 'x':                             // unsigned hex
            radix = 16;
            type |= 1;
            goto fetch_and_convert;

          case 'X':                             // unsigned HEX
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
                    flags |= FLAG_NEG;
                }
                goto do_long;
              case TYPE_UINT16:
                u.l = va_arg(ap, int) & 0xffff;
                goto do_long;
              case TYPE_INTN:
                u.l = va_arg(ap, int);
                if (u.l < 0) {
                    u.l = -u.l;
                    flags |= FLAG_NEG;
                }
                goto do_long;
              case TYPE_UINTN:
                u.l = (long)va_arg(ap, unsigned int);
                goto do_long;

              case TYPE_INT32:
                u.l = va_arg(ap, int32_t);
                if (u.l < 0) {
                    u.l = -u.l;
                    flags |= FLAG_NEG;
                }
                goto do_long;
              case TYPE_UINT32:
                u.l = (long)va_arg(ap, uint32_t);
              do_long:
                rv = cvt_l(ss, u.l, width, prec, radix, type, flags, hexp);
                if (rv < 0) {
                    return rv;
                }
                break;

              case TYPE_INT64:
                u.ll = va_arg(ap, int64_t);
                if (u.ll < 0) {
                    u.ll = -u.ll;
                    flags |= FLAG_NEG;
                }
                goto do_longlong;
              case TYPE_UINT64:
                u.ll = va_arg(ap, uint64_t);
              do_longlong:
                rv = cvt_ll(ss, u.ll, width, prec, radix, type, flags, hexp);
                if (rv < 0) {
                    return rv;
                }
                break;
            }
            break;

          case 'e':
          case 'E':
          case 'f':
          case 'g':
            u.d = va_arg(ap, double);
            if (nas != nullptr) {
                i = fmt - dolPt;
                if (i < int(sizeof(pattern))) {
                    pattern[0] = '%';
                    js_memcpy(&pattern[1], dolPt, size_t(i));
                    rv = cvt_f(ss, u.d, pattern, &pattern[i + 1]);
                }
            } else
                rv = cvt_f(ss, u.d, fmt0, fmt);

            if (rv < 0) {
                return rv;
            }
            break;

          case 'c':
            if ((flags & FLAG_LEFT) == 0) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, " ", 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
            switch (type) {
              case TYPE_INT16:
              case TYPE_INTN:
                u.ch = va_arg(ap, int);
                rv = (*ss->stuff)(ss, &u.ch, 1);
                break;
            }
            if (rv < 0) {
                return rv;
            }
            if (flags & FLAG_LEFT) {
                while (width-- > 1) {
                    rv = (*ss->stuff)(ss, " ", 1);
                    if (rv < 0) {
                        return rv;
                    }
                }
            }
            break;

          case 'p':
            if (sizeof(void *) == sizeof(int32_t)) {
                type = TYPE_UINT32;
            } else if (sizeof(void *) == sizeof(int64_t)) {
                type = TYPE_UINT64;
            } else if (sizeof(void *) == sizeof(int)) {
                type = TYPE_UINTN;
            } else {
                JS_ASSERT(0);
                break;
            }
            radix = 16;
            goto fetch_and_convert;

#if 0
          case 'C':
          case 'S':
          case 'E':
          case 'G':
            // XXX not supported I suppose
            JS_ASSERT(0);
            break;
#endif

          case 's':
            if(type == TYPE_INT16) {
                u.ws = va_arg(ap, const jschar*);
                rv = cvt_s(ss, u.ws, width, prec, flags);
            } else {
                u.s = va_arg(ap, const char*);
                rv = cvt_s(ss, u.s, width, prec, flags);
            }
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
            // Not a % token after all... skip it
#if 0
            JS_ASSERT(0);
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

    // Stuff trailing NUL
    rv = (*ss->stuff)(ss, "\0", 1);

    if (nas && nas != nasArray)
        js_free(nas);

    return rv;
}

/************************************************************************/

/*
 * Stuff routine that automatically grows the js_malloc'd output buffer
 * before it overflows.
 */
static int
GrowStuff(SprintfState *ss, const char *sp, size_t len)
{
    ptrdiff_t off;
    char *newbase;
    size_t newlen;

    off = ss->cur - ss->base;
    if (off + len >= ss->maxlen) {
        /* Grow the buffer */
        newlen = ss->maxlen + ((len > 32) ? len : 32);
        newbase = static_cast<char *>(js_realloc(ss->base, newlen));
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
    MOZ_ASSERT(size_t(ss->cur - ss->base) <= ss->maxlen);
    return 0;
}

/*
 * sprintf into a js_malloc'd buffer
 */
JS_PUBLIC_API(char *)
JS_smprintf(const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = JS_vsmprintf(fmt, ap);
    va_end(ap);
    return rv;
}

/*
 * Free memory allocated, for the caller, by JS_smprintf
 */
JS_PUBLIC_API(void)
JS_smprintf_free(char *mem)
{
    js_free(mem);
}

JS_PUBLIC_API(char *)
JS_vsmprintf(const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    ss.base = 0;
    ss.cur = 0;
    ss.maxlen = 0;
    rv = dosprintf(&ss, fmt, ap);
    if (rv < 0) {
        js_free(ss.base);
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
    size_t limit = ss->maxlen - (ss->cur - ss->base);

    if (len > limit)
        len = limit;
    while (len) {
        --len;
        *ss->cur++ = *sp++;
    }
    return 0;
}

/*
 * sprintf into a fixed size buffer. Make sure there is a NUL at the end
 * when finished.
 */
JS_PUBLIC_API(uint32_t)
JS_snprintf(char *out, uint32_t outlen, const char *fmt, ...)
{
    va_list ap;
    int rv;

    JS_ASSERT(int32_t(outlen) > 0);
    if (int32_t(outlen) <= 0)
        return 0;

    va_start(ap, fmt);
    rv = JS_vsnprintf(out, outlen, fmt, ap);
    va_end(ap);
    return rv;
}

JS_PUBLIC_API(uint32_t)
JS_vsnprintf(char *out, uint32_t outlen, const char *fmt, va_list ap)
{
    SprintfState ss;
    uint32_t n;

    JS_ASSERT(int32_t(outlen) > 0);
    if (int32_t(outlen) <= 0) {
        return 0;
    }

    ss.stuff = LimitStuff;
    ss.base = out;
    ss.cur = out;
    ss.maxlen = outlen;
    (void) dosprintf(&ss, fmt, ap);

    /* If we added chars, and we didn't append a null, do it now. */
    if (ss.cur != ss.base && ss.cur[-1] != '\0')
        ss.cur[-1] = '\0';

    n = ss.cur - ss.base;
    return n ? n - 1 : n;
}

JS_PUBLIC_API(char *)
JS_sprintf_append(char *last, const char *fmt, ...)
{
    va_list ap;
    char *rv;

    va_start(ap, fmt);
    rv = JS_vsprintf_append(last, fmt, ap);
    va_end(ap);
    return rv;
}

JS_PUBLIC_API(char *)
JS_vsprintf_append(char *last, const char *fmt, va_list ap)
{
    SprintfState ss;
    int rv;

    ss.stuff = GrowStuff;
    if (last) {
        size_t lastlen = strlen(last);
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
        js_free(ss.base);
        return 0;
    }
    return ss.base;
}

#undef TYPE_INT16
#undef TYPE_UINT16
#undef TYPE_INTN
#undef TYPE_UINTN
#undef TYPE_INT32
#undef TYPE_UINT32
#undef TYPE_INT64
#undef TYPE_UINT64
#undef TYPE_STRING
#undef TYPE_DOUBLE
#undef TYPE_INTSTR
#undef TYPE_WSTRING
#undef TYPE_UNKNOWN

#undef FLAG_LEFT
#undef FLAG_SIGNED
#undef FLAG_SPACED
#undef FLAG_ZEROS
#undef FLAG_NEG
