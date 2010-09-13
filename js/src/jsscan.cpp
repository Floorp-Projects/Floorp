/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS lexical scanner.
 */
#include <stdio.h>      /* first to avoid trouble on some systems */
#include <errno.h>
#include <limits.h>
#include <math.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsbit.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsemit.h"
#include "jsexn.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsvector.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

using namespace js;

#define JS_KEYWORD(keyword, type, op, version) \
    const char js_##keyword##_str[] = #keyword;
#include "jskeyword.tbl"
#undef JS_KEYWORD

struct keyword {
    const char  *chars;         /* C string with keyword text */
    TokenKind   tokentype;
    JSOp        op;             /* JSOp */
    JSVersion   version;        /* JSVersion */
};

static const struct keyword keyword_defs[] = {
#define JS_KEYWORD(keyword, type, op, version) \
    {js_##keyword##_str, type, op, version},
#include "jskeyword.tbl"
#undef JS_KEYWORD
};

#define KEYWORD_COUNT JS_ARRAY_LENGTH(keyword_defs)

static const struct keyword *
FindKeyword(const jschar *s, size_t length)
{
    register size_t i;
    const struct keyword *kw;
    const char *chars;

    JS_ASSERT(length != 0);

#define JSKW_LENGTH()           length
#define JSKW_AT(column)         s[column]
#define JSKW_GOT_MATCH(index)   i = (index); goto got_match;
#define JSKW_TEST_GUESS(index)  i = (index); goto test_guess;
#define JSKW_NO_MATCH()         goto no_match;
#include "jsautokw.h"
#undef JSKW_NO_MATCH
#undef JSKW_TEST_GUESS
#undef JSKW_GOT_MATCH
#undef JSKW_AT
#undef JSKW_LENGTH

  got_match:
    return &keyword_defs[i];

  test_guess:
    kw = &keyword_defs[i];
    chars = kw->chars;
    do {
        if (*s++ != (unsigned char)(*chars++))
            goto no_match;
    } while (--length != 0);
    return kw;

  no_match:
    return NULL;
}

TokenKind
js_CheckKeyword(const jschar *str, size_t length)
{
    const struct keyword *kw;

    JS_ASSERT(length != 0);
    kw = FindKeyword(str, length);
    return kw ? kw->tokentype : TOK_EOF;
}

JSBool
js_IsIdentifier(JSString *str)
{
    size_t length;
    jschar c;
    const jschar *chars, *end;

    str->getCharsAndLength(chars, length);
    if (length == 0)
        return JS_FALSE;
    c = *chars;
    if (!JS_ISIDSTART(c))
        return JS_FALSE;
    end = chars + length;
    while (++chars != end) {
        c = *chars;
        if (!JS_ISIDENT(c))
            return JS_FALSE;
    }
    return JS_TRUE;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4351)
#endif

/* Initialize members that aren't initialized in |init|. */
TokenStream::TokenStream(JSContext *cx)
  : cx(cx), tokens(), cursor(), lookahead(), flags(),
    linepos(), lineposNext(), file(), listenerTSData(), tokenbuf(cx)
{}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

bool
TokenStream::init(JSVersion version, const jschar *base, size_t length, FILE *fp,
                  const char *fn, uintN ln)
{
    this->version = version;
    JS_ASSERT_IF(fp, !base);
    JS_ASSERT_IF(!base, length == 0);
    size_t nb = fp
         ? (UNGET_LIMIT + 2 * LINE_LIMIT) * sizeof(jschar)    /* see below */
         : (UNGET_LIMIT + 1 * LINE_LIMIT) * sizeof(jschar);
    jschar *buf;
    JS_ARENA_ALLOCATE_CAST(buf, jschar *, &cx->tempPool, nb);
    if (!buf) {
        js_ReportOutOfScriptQuota(cx);
        return false;
    }
    memset(buf, 0, nb);

    /* Initialize members. */
    filename = fn;
    lineno = ln;
    /* 
     * Split 'buf' into 3 (ungetbuf, linebuf, userbuf) or 2 (ungetbuf, linebuf).
     * ungetbuf is empty and fills backwards.  linebuf is empty and fills forwards.
     */
    ungetbuf.base = buf;
    ungetbuf.limit = ungetbuf.ptr = buf + UNGET_LIMIT;
    linebuf.base = linebuf.limit = linebuf.ptr = buf + UNGET_LIMIT;
    if (fp) {
        file = fp;
        userbuf.base = buf + UNGET_LIMIT + LINE_LIMIT;
        userbuf.ptr = userbuf.limit = userbuf.base + LINE_LIMIT;
    } else {
        userbuf.base = (jschar *)base;
        userbuf.limit = (jschar *)base + length;
        userbuf.ptr = (jschar *)base;
    }
    currbuf = &linebuf;
    listener = cx->debugHooks->sourceHandler;
    listenerData = cx->debugHooks->sourceHandlerData;
    /* See getCharFillLinebuf() for an explanation of maybeEOL[]. */
    memset(maybeEOL, 0, sizeof(maybeEOL));
    maybeEOL['\n'] = true;
    maybeEOL['\r'] = true;
    maybeEOL[LINE_SEPARATOR & 0xff] = true;
    maybeEOL[PARA_SEPARATOR & 0xff] = true;
    /* See getTokenInternal() for an explanation of maybeStrSpecial[]. */
    memset(maybeStrSpecial, 0, sizeof(maybeStrSpecial));
    maybeStrSpecial['"'] = true;
    maybeStrSpecial['\''] = true;
    maybeStrSpecial['\n'] = true;
    maybeStrSpecial['\\'] = true;
    maybeStrSpecial[EOF & 0xff] = true;
    return true;
}

void
TokenStream::close()
{
    if (flags & TSF_OWNFILENAME)
        cx->free((void *) filename);
}

/* Use the fastest available getc. */
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

JS_FRIEND_API(int)
js_fgets(char *buf, int size, FILE *file)
{
    int n, i, c;
    JSBool crflag;

    n = size - 1;
    if (n < 0)
        return -1;

    crflag = JS_FALSE;
    for (i = 0; i < n && (c = fast_getc(file)) != EOF; i++) {
        buf[i] = c;
        if (c == '\n') {        /* any \n ends a line */
            i++;                /* keep the \n; we know there is room for \0 */
            break;
        }
        if (crflag) {           /* \r not followed by \n ends line at the \r */
            ungetc(c, file);
            break;              /* and overwrite c in buf with \0 */
        }
        crflag = (c == '\r');
    }

    buf[i] = '\0';
    return i;
}

/*
 * Nb: This does *not* append a terminating '\0'.  Returns the number of chars
 * read from the file.
 */
int
TokenStream::fillUserbuf()
{
    /*
     * We avoid splitting a \r\n pair, because this makes things much easier
     * for getChar().  To do this, we only try to fill userbuf up with
     * LINE_LIMIT-1 chars.  Once we've reached that number, if the last one is
     * \r then we check if the following one is \n;  if so we get it too,
     * knowing that we have space for it.
     */
    jschar *buf = userbuf.base;
    int n = LINE_LIMIT - 1;     /* reserve space for \n following a \r */
    JS_ASSERT(n > 0);
    int i;
    i = 0;
    while (true) {
        int c = fast_getc(file);
        if (c == EOF)
            break;
        buf[i] = (jschar) (unsigned char) c;
        i++;

        if (i == n) {
            if (buf[i - 1] == '\r') {
                /* Look for a following \n.  We know we have space in buf for it. */
                c = fast_getc(file);
                if (c == EOF)
                    break;
                if (c == '\n') {
                    buf[i] = (jschar) (unsigned char) c;
                    i++;
                    break;
                }
                ungetc(c, file);    /* \r wasn't followed by \n, unget */
            }
            break;
        }
    }
    return i;
}

int32
TokenStream::getCharFillLinebuf()
{
    ptrdiff_t ulen = userbuf.limit - userbuf.ptr;
    if (ulen <= 0) {
        if (!file) {
            flags |= TSF_EOF;
            return EOF;
        }

        /* Fill userbuf so that \r and \r\n convert to \n. */
        ulen = fillUserbuf();
        JS_ASSERT(ulen >= 0);
        if (ulen == 0) {
            flags |= TSF_EOF;
            return EOF;
        }
        userbuf.limit = userbuf.base + ulen;
        userbuf.ptr = userbuf.base;
    }
    if (listener)
        listener(filename, lineno, userbuf.ptr, ulen, &listenerTSData, listenerData);

    /*
     * Copy from userbuf to linebuf.  Stop when any of these happen:
     * (a) we reach the end of userbuf;
     * (b) we reach the end of linebuf;
     * (c) we hit an EOL.
     *
     * "EOL" means any of: \r, \n, \r\n, or the Unicode line and paragraph
     * separators.
     */
    jschar *from = userbuf.ptr;
    jschar *to = linebuf.base;

    int llenAdjust = 0;
    int limit = JS_MIN(size_t(ulen), LINE_LIMIT);
    int i = 0;
    while (i < limit) {
        /* Copy the jschar from userbuf to linebuf. */
        jschar d = to[i] = from[i];
        i++;

        /*
         * Normalize the copied jschar if it was a newline.  We need to detect
         * any of these four characters:  '\n' (0x000a), '\r' (0x000d),
         * LINE_SEPARATOR (0x2028), PARA_SEPARATOR (0x2029).  Testing for each
         * one in turn is slow, so we use a single probabilistic check, and if
         * that succeeds, test for them individually.
         *
         * We use the bottom 8 bits to index into a lookup table, succeeding
         * when d&0xff is 0xa, 0xd, 0x28 or 0x29.  Among ASCII chars (which
         * are by the far the most common) this gives false positives for '('
         * (0x0028) and ')' (0x0029).  We could avoid those by incorporating
         * the 13th bit of d into the lookup, but that requires extra shifting
         * and masking and isn't worthwhile.  See TokenStream::init() for the
         * initialization of the relevant entries in the table.
         */
        if (maybeEOL[d & 0xff]) {
            if (d == '\n') {
                break;
            }

            if (d == '\r') {
                to[i - 1] = '\n';       /* overwrite with '\n' */
                if (i < ulen && from[i] == '\n') {
                    i++;                /* skip over '\n' */
                    llenAdjust = -1;
                }
                break;
            }

            if (d == LINE_SEPARATOR || d == PARA_SEPARATOR) {
                to[i - 1] = '\n';       /* overwrite with '\n' */
                break;
            }
        }
    }
    
    /* At this point 'i' is the index one past the last char copied. */
    ulen = i;
    userbuf.ptr += ulen;

    /* Reset linebuf based on normalized length. */
    linebuf.ptr = linebuf.base;
    linebuf.limit = linebuf.base + ulen + llenAdjust;

    /* Update position of linebuf within physical userbuf line. */
    linepos = lineposNext;
    if (linebuf.limit[-1] == '\n')
        lineposNext = 0;
    else
        lineposNext += ulen;

    return *linebuf.ptr++;
}

/*
 * This gets the next char, normalizing all EOL sequences to '\n' as it goes.
 */
int32
TokenStream::getCharSlowCase()
{
    int32 c;
    if (currbuf->ptr == currbuf->limit - 1) {
        /* Last char of currbuf.  Switch to linebuf if we're in ungetbuf. */
        c = *currbuf->ptr++;
        if (currbuf == &ungetbuf)
            currbuf = &linebuf;

    } else {
        /* One past the last char of currbuf;  can only happen for linebuf. */
        JS_ASSERT(currbuf->ptr == currbuf->limit);
        JS_ASSERT(currbuf == &linebuf);
        c = getCharFillLinebuf();
    }
    if (c == '\n')
        lineno++;
    return c;
}

void
TokenStream::ungetChar(int32 c)
{
    if (c == EOF)
        return;
    JS_ASSERT(ungetbuf.ptr >= ungetbuf.base);
    if (c == '\n') {
        /* We can only unget one '\n', and it must be the first ungotten char. */
        JS_ASSERT(ungetbuf.ptr == ungetbuf.limit);
        lineno--;
    }
    *(--ungetbuf.ptr) = (jschar)c;
    currbuf = &ungetbuf;
}

/*
 * Peek n chars ahead into ts.  Return true if n chars were read, false if
 * there weren't enough characters in the input stream.  This function cannot
 * be used to peek into or past a newline.
 */
JSBool
TokenStream::peekChars(intN n, jschar *cp)
{
    intN i, j;
    int32 c;

    for (i = 0; i < n; i++) {
        c = getChar();
        if (c == EOF)
            break;
        if (c == '\n') {
            ungetChar(c);
            break;
        }
        cp[i] = (jschar)c;
    }
    for (j = i - 1; j >= 0; j--)
        ungetChar(cp[j]);
    return i == n;
}

bool
TokenStream::reportCompileErrorNumberVA(JSParseNode *pn, uintN flags, uintN errorNumber,
                                        va_list ap)
{
    JSErrorReport report;
    char *message;
    size_t linelength;
    jschar *linechars;
    char *linebytes;
    bool warning;
    JSBool ok;
    TokenPos *tp;
    uintN index, i;
    JSErrorReporter onError;

    JS_ASSERT(linebuf.limit <= linebuf.base + LINE_LIMIT);

    if (JSREPORT_IS_STRICT(flags) && !JS_HAS_STRICT_OPTION(cx))
        return JS_TRUE;

    warning = JSREPORT_IS_WARNING(flags);
    if (warning && JS_HAS_WERROR_OPTION(cx)) {
        flags &= ~JSREPORT_WARNING;
        warning = false;
    }

    PodZero(&report);
    report.flags = flags;
    report.errorNumber = errorNumber;
    message = NULL;
    linechars = NULL;
    linebytes = NULL;

    MUST_FLOW_THROUGH("out");
    ok = js_ExpandErrorArguments(cx, js_GetErrorMessage, NULL,
                                 errorNumber, &message, &report,
                                 !(flags & JSREPORT_UC), ap);
    if (!ok) {
        warning = false;
        goto out;
    }

    report.filename = filename;

    if (pn) {
        report.lineno = pn->pn_pos.begin.lineno;
        if (report.lineno != lineno)
            goto report;
        tp = &pn->pn_pos;
    } else {
        /* Point to the current token, not the next one to get. */
        tp = &tokens[cursor].pos;
    }
    report.lineno = lineno;
    linelength = linebuf.limit - linebuf.base;
    linechars = (jschar *)cx->malloc((linelength + 1) * sizeof(jschar));
    if (!linechars) {
        warning = false;
        goto out;
    }
    memcpy(linechars, linebuf.base, linelength * sizeof(jschar));
    linechars[linelength] = 0;
    linebytes = js_DeflateString(cx, linechars, linelength);
    if (!linebytes) {
        warning = false;
        goto out;
    }
    report.linebuf = linebytes;

    /*
     * FIXME: What should instead happen here is that we should
     * find error-tokens in userbuf, if !file.  That will
     * allow us to deliver a more helpful error message, which
     * includes all or part of the bad string or bad token.  The
     * code here yields something that looks truncated.
     * See https://bugzilla.mozilla.org/show_bug.cgi?id=352970
     */
    index = 0;
    if (tp->begin.lineno == tp->end.lineno) {
        if (tp->begin.index < linepos)
            goto report;

        index = tp->begin.index - linepos;
    }

    report.tokenptr = report.linebuf + index;
    report.uclinebuf = linechars;
    report.uctokenptr = report.uclinebuf + index;

    /*
     * If there's a runtime exception type associated with this error
     * number, set that as the pending exception.  For errors occuring at
     * compile time, this is very likely to be a JSEXN_SYNTAXERR.
     *
     * If an exception is thrown but not caught, the JSREPORT_EXCEPTION
     * flag will be set in report.flags.  Proper behavior for an error
     * reporter is to ignore a report with this flag for all but top-level
     * compilation errors.  The exception will remain pending, and so long
     * as the non-top-level "load", "eval", or "compile" native function
     * returns false, the top-level reporter will eventually receive the
     * uncaught exception report.
     *
     * XXX it'd probably be best if there was only one call to this
     * function, but there seem to be two error reporter call points.
     */
  report:
    onError = cx->errorReporter;

    /*
     * Try to raise an exception only if there isn't one already set --
     * otherwise the exception will describe the last compile-time error,
     * which is likely spurious.
     */
    if (!(flags & TSF_ERROR)) {
        if (js_ErrorToException(cx, message, &report, NULL, NULL))
            onError = NULL;
    }

    /*
     * Suppress any compile-time errors that don't occur at the top level.
     * This may still fail, as interplevel may be zero in contexts where we
     * don't really want to call the error reporter, as when js is called
     * by other code which could catch the error.
     */
    if (cx->interpLevel != 0 && !JSREPORT_IS_WARNING(flags))
        onError = NULL;

    if (onError) {
        JSDebugErrorHook hook = cx->debugHooks->debugErrorHook;

        /*
         * If debugErrorHook is present then we give it a chance to veto
         * sending the error on to the regular error reporter.
         */
        if (hook && !hook(cx, message, &report,
                          cx->debugHooks->debugErrorHookData)) {
            onError = NULL;
        }
    }
    if (onError)
        (*onError)(cx, message, &report);

  out:
    if (linebytes)
        cx->free(linebytes);
    if (linechars)
        cx->free(linechars);
    if (message)
        cx->free(message);
    if (report.ucmessage)
        cx->free((void *)report.ucmessage);

    if (report.messageArgs) {
        if (!(flags & JSREPORT_UC)) {
            i = 0;
            while (report.messageArgs[i])
                cx->free((void *)report.messageArgs[i++]);
        }
        cx->free((void *)report.messageArgs);
    }

    if (!JSREPORT_IS_WARNING(flags)) {
        /* Set the error flag to suppress spurious reports. */
        flags |= TSF_ERROR;
    }

    return warning;
}

bool
js::ReportStrictModeError(JSContext *cx, TokenStream *ts, JSTreeContext *tc, JSParseNode *pn,
                          uintN errorNumber, ...)
{
    JS_ASSERT(ts || tc);
    JS_ASSERT(cx == ts->getContext());

    /* In strict mode code, this is an error, not just a warning. */
    uintN flags;
    if ((tc && tc->flags & TCF_STRICT_MODE_CODE) || (ts && ts->isStrictMode()))
        flags = JSREPORT_ERROR;
    else if (JS_HAS_STRICT_OPTION(cx))
        flags = JSREPORT_WARNING;
    else
        return true;

    va_list ap;
    va_start(ap, errorNumber);
    bool result = ts->reportCompileErrorNumberVA(pn, flags, errorNumber, ap);
    va_end(ap);

    return result;
}

bool
js::ReportCompileErrorNumber(JSContext *cx, TokenStream *ts, JSParseNode *pn,
                             uintN flags, uintN errorNumber, ...)
{
    va_list ap;

    /*
     * We don't accept a JSTreeContext argument, so we can't implement
     * JSREPORT_STRICT_MODE_ERROR here.  Use ReportStrictModeError instead,
     * or do the checks in the caller and pass plain old JSREPORT_ERROR.
     */
    JS_ASSERT(!(flags & JSREPORT_STRICT_MODE_ERROR));

    va_start(ap, errorNumber);
    JS_ASSERT(cx == ts->getContext());
    bool result = ts->reportCompileErrorNumberVA(pn, flags, errorNumber, ap);
    va_end(ap);

    return result;
}

#if JS_HAS_XML_SUPPORT

JSBool
TokenStream::getXMLEntity()
{
    ptrdiff_t offset, length, i;
    int c, d;
    JSBool ispair;
    jschar *bp, digit;
    char *bytes;
    JSErrNum msg;

    JSCharBuffer &tb = tokenbuf;

    /* Put the entity, including the '&' already scanned, in tokenbuf. */
    offset = tb.length();
    if (!tb.append('&'))
        return JS_FALSE;
    while ((c = getChar()) != ';') {
        if (c == EOF || c == '\n') {
            ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_END_OF_XML_ENTITY);
            return JS_FALSE;
        }
        if (!tb.append(c))
            return JS_FALSE;
    }

    /* Let length be the number of jschars after the '&', including the ';'. */
    length = tb.length() - offset;
    bp = tb.begin() + offset;
    c = d = 0;
    ispair = JS_FALSE;
    if (length > 2 && bp[1] == '#') {
        /* Match a well-formed XML Character Reference. */
        i = 2;
        if (length > 3 && JS_TOLOWER(bp[i]) == 'x') {
            if (length > 9)     /* at most 6 hex digits allowed */
                goto badncr;
            while (++i < length) {
                digit = bp[i];
                if (!JS7_ISHEX(digit))
                    goto badncr;
                c = (c << 4) + JS7_UNHEX(digit);
            }
        } else {
            while (i < length) {
                digit = bp[i++];
                if (!JS7_ISDEC(digit))
                    goto badncr;
                c = (c * 10) + JS7_UNDEC(digit);
                if (c < 0)
                    goto badncr;
            }
        }

        if (0x10000 <= c && c <= 0x10FFFF) {
            /* Form a surrogate pair (c, d) -- c is the high surrogate. */
            d = 0xDC00 + (c & 0x3FF);
            c = 0xD7C0 + (c >> 10);
            ispair = JS_TRUE;
        } else {
            /* Enforce the http://www.w3.org/TR/REC-xml/#wf-Legalchar WFC. */
            if (c != 0x9 && c != 0xA && c != 0xD &&
                !(0x20 <= c && c <= 0xD7FF) &&
                !(0xE000 <= c && c <= 0xFFFD)) {
                goto badncr;
            }
        }
    } else {
        /* Try to match one of the five XML 1.0 predefined entities. */
        switch (length) {
          case 3:
            if (bp[2] == 't') {
                if (bp[1] == 'l')
                    c = '<';
                else if (bp[1] == 'g')
                    c = '>';
            }
            break;
          case 4:
            if (bp[1] == 'a' && bp[2] == 'm' && bp[3] == 'p')
                c = '&';
            break;
          case 5:
            if (bp[3] == 'o') {
                if (bp[1] == 'a' && bp[2] == 'p' && bp[4] == 's')
                    c = '\'';
                else if (bp[1] == 'q' && bp[2] == 'u' && bp[4] == 't')
                    c = '"';
            }
            break;
        }
        if (c == 0) {
            msg = JSMSG_UNKNOWN_XML_ENTITY;
            goto bad;
        }
    }

    /* If we matched, retract tokenbuf and store the entity's value. */
    *bp++ = (jschar) c;
    if (ispair)
        *bp++ = (jschar) d;
    tb.shrinkBy(tb.end() - bp);
    return JS_TRUE;

  badncr:
    msg = JSMSG_BAD_XML_NCR;
  bad:
    /* No match: throw a TypeError per ECMA-357 10.3.2.1 step 8(a). */
    JS_ASSERT((tb.end() - bp) >= 1);
    bytes = js_DeflateString(cx, bp + 1, (tb.end() - bp) - 1);
    if (bytes) {
        ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, msg, bytes);
        cx->free(bytes);
    }
    return JS_FALSE;
}

#endif /* JS_HAS_XML_SUPPORT */

/*
 * We have encountered a '\': check for a Unicode escape sequence after it,
 * returning the character code value if we found a Unicode escape sequence.
 * Otherwise, non-destructively return the original '\'.
 */
int32
TokenStream::getUnicodeEscape()
{
    jschar cp[5];
    int32 c;

    if (peekChars(5, cp) && cp[0] == 'u' &&
        JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]) &&
        JS7_ISHEX(cp[3]) && JS7_ISHEX(cp[4]))
    {
        c = (((((JS7_UNHEX(cp[1]) << 4)
                + JS7_UNHEX(cp[2])) << 4)
              + JS7_UNHEX(cp[3])) << 4)
            + JS7_UNHEX(cp[4]);
        skipChars(5);
        return c;
    }
    return '\\';
}

Token *
TokenStream::newToken(ptrdiff_t adjust)
{
    cursor = (cursor + 1) & ntokensMask;
    Token *tp = &tokens[cursor];
    tp->ptr = linebuf.ptr + adjust;
    tp->pos.begin.index = linepos + (tp->ptr - linebuf.base) - (ungetbuf.limit - ungetbuf.ptr);
    tp->pos.begin.lineno = tp->pos.end.lineno = lineno;
    return tp;
}

static JS_ALWAYS_INLINE JSBool
ScanAsSpace(jschar c)
{
    /* Treat little- and big-endian BOMs as whitespace for compatibility. */
    if (JS_ISSPACE(c) || c == 0xfffe || c == 0xfeff)
        return JS_TRUE;
    return JS_FALSE;
}

static JS_ALWAYS_INLINE JSAtom *
atomize(JSContext *cx, JSCharBuffer &cb)
{
    return js_AtomizeChars(cx, cb.begin(), cb.length(), 0);
}

TokenKind
TokenStream::getTokenInternal()
{
    TokenKind tt;
    int c, qc;
    Token *tp;
    JSAtom *atom;
    JSBool hadUnicodeEscape;
    const struct keyword *kw;
#if JS_HAS_XML_SUPPORT
    JSBool inTarget;
    size_t targetLength;
    ptrdiff_t contentIndex;
#endif

#if JS_HAS_XML_SUPPORT
    if (flags & TSF_XMLTEXTMODE) {
        tt = TOK_XMLSPACE;      /* veto if non-space, return TOK_XMLTEXT */
        tp = newToken(0);
        tokenbuf.clear();
        qc = (flags & TSF_XMLONLYMODE) ? '<' : '{';

        while ((c = getChar()) != qc && c != '<' && c != EOF) {
            if (c == '&' && qc == '<') {
                if (!getXMLEntity())
                    goto error;
                tt = TOK_XMLTEXT;
                continue;
            }

            if (!JS_ISXMLSPACE(c))
                tt = TOK_XMLTEXT;
            if (!tokenbuf.append(c))
                goto error;
        }
        ungetChar(c);

        if (tokenbuf.empty()) {
            atom = NULL;
        } else {
            atom = atomize(cx, tokenbuf);
            if (!atom)
                goto error;
        }
        tp->pos.end.lineno = lineno;
        tp->t_op = JSOP_STRING;
        tp->t_atom = atom;
        goto out;
    }

    if (flags & TSF_XMLTAGMODE) {
        tp = newToken(0);
        c = getChar();
        if (JS_ISXMLSPACE(c)) {
            do {
                c = getChar();
            } while (JS_ISXMLSPACE(c));
            ungetChar(c);
            tt = TOK_XMLSPACE;
            goto out;
        }

        if (c == EOF) {
            tt = TOK_EOF;
            goto out;
        }

        tokenbuf.clear();
        if (JS_ISXMLNSSTART(c)) {
            JSBool sawColon = JS_FALSE;

            if (!tokenbuf.append(c))
                goto error;
            while ((c = getChar()) != EOF && JS_ISXMLNAME(c)) {
                if (c == ':') {
                    int nextc;

                    if (sawColon ||
                        (nextc = peekChar(),
                         ((flags & TSF_XMLONLYMODE) || nextc != '{') &&
                         !JS_ISXMLNAME(nextc))) {
                        ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                                 JSMSG_BAD_XML_QNAME);
                        goto error;
                    }
                    sawColon = JS_TRUE;
                }

                if (!tokenbuf.append(c))
                    goto error;
            }

            ungetChar(c);
            atom = atomize(cx, tokenbuf);
            if (!atom)
                goto error;
            tp->t_op = JSOP_STRING;
            tp->t_atom = atom;
            tt = TOK_XMLNAME;
            goto out;
        }

        switch (c) {
          case '{':
            if (flags & TSF_XMLONLYMODE)
                goto bad_xml_char;
            tt = TOK_LC;
            goto out;

          case '=':
            tt = TOK_ASSIGN;
            goto out;

          case '"':
          case '\'':
            qc = c;
            while ((c = getChar()) != qc) {
                if (c == EOF) {
                    ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                             JSMSG_UNTERMINATED_STRING);
                    goto error;
                }

                /*
                 * XML attribute values are double-quoted when pretty-printed,
                 * so escape " if it is expressed directly in a single-quoted
                 * attribute value.
                 */
                if (c == '"' && !(flags & TSF_XMLONLYMODE)) {
                    JS_ASSERT(qc == '\'');
                    if (!tokenbuf.append(js_quot_entity_str,
                                     strlen(js_quot_entity_str)))
                        goto error;
                    continue;
                }

                if (c == '&' && (flags & TSF_XMLONLYMODE)) {
                    if (!getXMLEntity())
                        goto error;
                    continue;
                }

                if (!tokenbuf.append(c))
                    goto error;
            }
            atom = atomize(cx, tokenbuf);
            if (!atom)
                goto error;
            tp->pos.end.lineno = lineno;
            tp->t_op = JSOP_STRING;
            tp->t_atom = atom;
            tt = TOK_XMLATTR;
            goto out;

          case '>':
            tt = TOK_XMLTAGC;
            goto out;

          case '/':
            if (matchChar('>')) {
                tt = TOK_XMLPTAGC;
                goto out;
            }
            /* FALL THROUGH */

          bad_xml_char:
          default:
            ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_BAD_XML_CHARACTER);
            goto error;
        }
        /* NOTREACHED */
    }
#endif /* JS_HAS_XML_SUPPORT */

  retry:
    do {
        c = getChar();
        if (c == '\n') {
            flags &= ~TSF_DIRTYLINE;
            if (flags & TSF_NEWLINES)
                break;
        }
    } while (ScanAsSpace((jschar)c));

    tp = newToken(-1);
    if (c == EOF) {
        tt = TOK_EOF;
        goto out;
    }

    hadUnicodeEscape = JS_FALSE;
    if (JS_ISIDSTART(c) ||
        (c == '\\' &&
         (qc = getUnicodeEscape(),
          hadUnicodeEscape = JS_ISIDSTART(qc)))) {
        if (hadUnicodeEscape)
            c = qc;
        tokenbuf.clear();
        for (;;) {
            if (!tokenbuf.append(c))
                goto error;
            c = getChar();
            if (c == '\\') {
                qc = getUnicodeEscape();
                if (!JS_ISIDENT(qc))
                    break;
                c = qc;
                hadUnicodeEscape = JS_TRUE;
            } else {
                if (!JS_ISIDENT(c))
                    break;
            }
        }
        ungetChar(c);

        /*
         * Check for keywords unless we saw Unicode escape or parser asks
         * to ignore keywords.
         */
        if (!hadUnicodeEscape &&
            !(flags & TSF_KEYWORD_IS_NAME) &&
            (kw = FindKeyword(tokenbuf.begin(), tokenbuf.length()))) {
            if (kw->tokentype == TOK_RESERVED) {
                if (!ReportCompileErrorNumber(cx, this, NULL, JSREPORT_WARNING | JSREPORT_STRICT,
                                              JSMSG_RESERVED_ID, kw->chars)) {
                    goto error;
                }
            } else if (kw->version <= VersionNumber(version)) {
                tt = kw->tokentype;
                tp->t_op = (JSOp) kw->op;
                goto out;
            }
        }

        atom = atomize(cx, tokenbuf);
        if (!atom)
            goto error;
        tp->t_op = JSOP_NAME;
        tp->t_atom = atom;
        tt = TOK_NAME;
        goto out;
    }

    if (JS7_ISDEC(c) || (c == '.' && JS7_ISDEC(peekChar()))) {
        int radix = 10;
        tokenbuf.clear();

        if (c == '0') {
            c = getChar();
            if (JS_TOLOWER(c) == 'x') {
                radix = 16;
                c = getChar();
                if (!JS7_ISHEX(c)) {
                    ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                             JSMSG_MISSING_HEXDIGITS);
                    goto error;
                }
            } else if (JS7_ISDEC(c)) {
                radix = 8;
            }
        }

        while (JS7_ISHEX(c)) {
            if (radix < 16) {
                if (JS7_ISLET(c))
                    break;

                if (radix == 8) {
                    /* Octal integer literals are not permitted in strict mode code. */
                    if (!ReportStrictModeError(cx, this, NULL, NULL, JSMSG_DEPRECATED_OCTAL))
                        goto error;

                    /*
                     * Outside strict mode, we permit 08 and 09 as decimal numbers, which
                     * makes our behaviour a superset of the ECMA numeric grammar. We
                     * might not always be so permissive, so we warn about it.
                     */
                    if (c >= '8') {
                        if (!ReportCompileErrorNumber(cx, this, NULL, JSREPORT_WARNING,
                                                      JSMSG_BAD_OCTAL, c == '8' ? "08" : "09")) {
                            goto error;
                        }
                        radix = 10;
                    }
                }
            }
            if (!tokenbuf.append(c))
                goto error;
            c = getChar();
        }

        if (radix == 10 && (c == '.' || JS_TOLOWER(c) == 'e')) {
            if (c == '.') {
                do {
                    if (!tokenbuf.append(c))
                        goto error;
                    c = getChar();
                } while (JS7_ISDEC(c));
            }
            if (JS_TOLOWER(c) == 'e') {
                if (!tokenbuf.append(c))
                    goto error;
                c = getChar();
                if (c == '+' || c == '-') {
                    if (!tokenbuf.append(c))
                        goto error;
                    c = getChar();
                }
                if (!JS7_ISDEC(c)) {
                    ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                             JSMSG_MISSING_EXPONENT);
                    goto error;
                }
                do {
                    if (!tokenbuf.append(c))
                        goto error;
                    c = getChar();
                } while (JS7_ISDEC(c));
            }
        }

        if (JS_ISIDSTART(c)) {
            ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_IDSTART_AFTER_NUMBER);
            goto error;
        }

        /* Put back the next char and NUL-terminate tokenbuf for js_strto*. */
        ungetChar(c);
        if (!tokenbuf.append(0))
            goto error;

        jsdouble dval;
        const jschar *dummy;
        if (radix == 10) {
            if (!js_strtod(cx, tokenbuf.begin(), tokenbuf.end(), &dummy, &dval))
                goto error;
        } else {
            if (!GetPrefixInteger(cx, tokenbuf.begin(), tokenbuf.end(), radix, &dummy, &dval))
                goto error;
        }
        tp->t_dval = dval;
        tt = TOK_NUMBER;
        goto out;
    }

    if (c == '"' || c == '\'') {
        qc = c;
        tokenbuf.clear();
        while (true) {
            c = getChar();
            /*
             * We need to detect any of these four chars:  " or ', \n, \\,
             * EOF.  We use maybeStrSpecial[] in a manner similar to
             * maybeEOL[], see above.
             */
            if (maybeStrSpecial[c & 0xff]) {
                if (c == qc) {
                    break;
                } else if (c == '\\') {
                    switch (c = getChar()) {
                      case 'b': c = '\b'; break;
                      case 'f': c = '\f'; break;
                      case 'n': c = '\n'; break;
                      case 'r': c = '\r'; break;
                      case 't': c = '\t'; break;
                      case 'v': c = '\v'; break;

                      default:
                        if ('0' <= c && c < '8') {
                            int32 val = JS7_UNDEC(c);

                            c = peekChar();
                            /* Strict mode code allows only \0, then a non-digit. */
                            if (val != 0 || JS7_ISDEC(c)) {
                                if (!ReportStrictModeError(cx, this, NULL, NULL,
                                                           JSMSG_DEPRECATED_OCTAL)) {
                                    goto error;
                                }
                            }
                            if ('0' <= c && c < '8') {
                                val = 8 * val + JS7_UNDEC(c);
                                getChar();
                                c = peekChar();
                                if ('0' <= c && c < '8') {
                                    int32 save = val;
                                    val = 8 * val + JS7_UNDEC(c);
                                    if (val <= 0377)
                                        getChar();
                                    else
                                        val = save;
                                }
                            }

                            c = (jschar)val;
                        } else if (c == 'u') {
                            jschar cp[4];
                            if (peekChars(4, cp) &&
                                JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) &&
                                JS7_ISHEX(cp[2]) && JS7_ISHEX(cp[3])) {
                                c = (((((JS7_UNHEX(cp[0]) << 4)
                                        + JS7_UNHEX(cp[1])) << 4)
                                      + JS7_UNHEX(cp[2])) << 4)
                                    + JS7_UNHEX(cp[3]);
                                skipChars(4);
                            }
                        } else if (c == 'x') {
                            jschar cp[2];
                            if (peekChars(2, cp) &&
                                JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1])) {
                                c = (JS7_UNHEX(cp[0]) << 4) + JS7_UNHEX(cp[1]);
                                skipChars(2);
                            }
                        } else if (c == '\n') {
                            /* ECMA follows C by removing escaped newlines. */
                            continue;
                        }
                        break;
                    }
                } else if (c == '\n' || c == EOF) {
                    ungetChar(c);
                    ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                             JSMSG_UNTERMINATED_STRING);
                    goto error;
                }
            }
            if (!tokenbuf.append(c))
                goto error;
        }
        atom = atomize(cx, tokenbuf);
        if (!atom)
            goto error;
        tp->pos.end.lineno = lineno;
        tp->t_op = JSOP_STRING;
        tp->t_atom = atom;
        tt = TOK_STRING;
        goto out;
    }

    switch (c) {
      case '\n': tt = TOK_EOL; goto eol_out;
      case ';':  tt = TOK_SEMI; break;
      case '[':  tt = TOK_LB; break;
      case ']':  tt = TOK_RB; break;
      case '{':  tt = TOK_LC; break;
      case '}':  tt = TOK_RC; break;
      case '(':  tt = TOK_LP; break;
      case ')':  tt = TOK_RP; break;
      case ',':  tt = TOK_COMMA; break;
      case '?':  tt = TOK_HOOK; break;

      case '.':
#if JS_HAS_XML_SUPPORT
        if (matchChar(c))
            tt = TOK_DBLDOT;
        else
#endif
            tt = TOK_DOT;
        break;

      case ':':
#if JS_HAS_XML_SUPPORT
        if (matchChar(c)) {
            tt = TOK_DBLCOLON;
            break;
        }
#endif
        /*
         * Default so compiler can modify to JSOP_GETTER if 'p getter: v' in an
         * object initializer, likewise for setter.
         */
        tp->t_op = JSOP_NOP;
        tt = TOK_COLON;
        break;

      case '|':
        if (matchChar(c)) {
            tt = TOK_OR;
        } else if (matchChar('=')) {
            tp->t_op = JSOP_BITOR;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITOR;
        }
        break;

      case '^':
        if (matchChar('=')) {
            tp->t_op = JSOP_BITXOR;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITXOR;
        }
        break;

      case '&':
        if (matchChar(c)) {
            tt = TOK_AND;
        } else if (matchChar('=')) {
            tp->t_op = JSOP_BITAND;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITAND;
        }
        break;

      case '=':
        if (matchChar(c)) {
            tp->t_op = matchChar(c) ? JSOP_STRICTEQ : JSOP_EQ;
            tt = TOK_EQOP;
        } else {
            tp->t_op = JSOP_NOP;
            tt = TOK_ASSIGN;
        }
        break;

      case '!':
        if (matchChar('=')) {
            tp->t_op = matchChar('=') ? JSOP_STRICTNE : JSOP_NE;
            tt = TOK_EQOP;
        } else {
            tp->t_op = JSOP_NOT;
            tt = TOK_UNARYOP;
        }
        break;

#if JS_HAS_XML_SUPPORT
      case '@':
        tt = TOK_AT;
        break;
#endif

      case '<':
#if JS_HAS_XML_SUPPORT
        /*
         * After much testing, it's clear that Postel's advice to protocol
         * designers ("be liberal in what you accept, and conservative in what
         * you send") invites a natural-law repercussion for JS as "protocol":
         *
         * "If you are liberal in what you accept, others will utterly fail to
         *  be conservative in what they send."
         *
         * Which means you will get <!-- comments to end of line in the middle
         * of .js files, and after if conditions whose then statements are on
         * the next line, and other wonders.  See at least the following bugs:
         * https://bugzilla.mozilla.org/show_bug.cgi?id=309242
         * https://bugzilla.mozilla.org/show_bug.cgi?id=309712
         * https://bugzilla.mozilla.org/show_bug.cgi?id=310993
         *
         * So without JSOPTION_XML, we changed around Firefox 1.5 never to scan
         * an XML comment or CDATA literal.  Instead, we always scan <! as the
         * start of an HTML comment hack to end of line, used since Netscape 2
         * to hide script tag content from script-unaware browsers.
         *
         * But this still leaves XML resources with certain internal structure
         * vulnerable to being loaded as script cross-origin, and some internal
         * data stolen, so for Firefox 3.5 and beyond, we reject programs whose
         * source consists only of XML literals. See:
         *
         * https://bugzilla.mozilla.org/show_bug.cgi?id=336551
         *
         * The check for this is in jsparse.cpp, Compiler::compileScript.
         */
        if ((flags & TSF_OPERAND) &&
            (VersionHasXML(version) || peekChar() != '!')) {
            /* Check for XML comment or CDATA section. */
            if (matchChar('!')) {
                tokenbuf.clear();

                /* Scan XML comment. */
                if (matchChar('-')) {
                    if (!matchChar('-'))
                        goto bad_xml_markup;
                    while ((c = getChar()) != '-' || !matchChar('-')) {
                        if (c == EOF)
                            goto bad_xml_markup;
                        if (!tokenbuf.append(c))
                            goto error;
                    }
                    tt = TOK_XMLCOMMENT;
                    tp->t_op = JSOP_XMLCOMMENT;
                    goto finish_xml_markup;
                }

                /* Scan CDATA section. */
                if (matchChar('[')) {
                    jschar cp[6];
                    if (peekChars(6, cp) &&
                        cp[0] == 'C' &&
                        cp[1] == 'D' &&
                        cp[2] == 'A' &&
                        cp[3] == 'T' &&
                        cp[4] == 'A' &&
                        cp[5] == '[') {
                        skipChars(6);
                        while ((c = getChar()) != ']' ||
                               !peekChars(2, cp) ||
                               cp[0] != ']' ||
                               cp[1] != '>') {
                            if (c == EOF)
                                goto bad_xml_markup;
                            if (!tokenbuf.append(c))
                                goto error;
                        }
                        getChar();            /* discard ] but not > */
                        tt = TOK_XMLCDATA;
                        tp->t_op = JSOP_XMLCDATA;
                        goto finish_xml_markup;
                    }
                    goto bad_xml_markup;
                }
            }

            /* Check for processing instruction. */
            if (matchChar('?')) {
                inTarget = JS_TRUE;
                targetLength = 0;
                contentIndex = -1;

                tokenbuf.clear();
                while ((c = getChar()) != '?' || peekChar() != '>') {
                    if (c == EOF)
                        goto bad_xml_markup;
                    if (inTarget) {
                        if (JS_ISXMLSPACE(c)) {
                            if (tokenbuf.empty())
                                goto bad_xml_markup;
                            inTarget = JS_FALSE;
                        } else {
                            if (!(tokenbuf.empty()
                                  ? JS_ISXMLNSSTART(c)
                                  : JS_ISXMLNS(c))) {
                                goto bad_xml_markup;
                            }
                            ++targetLength;
                        }
                    } else {
                        if (contentIndex < 0 && !JS_ISXMLSPACE(c))
                            contentIndex = tokenbuf.length();
                    }
                    if (!tokenbuf.append(c))
                        goto error;
                }
                if (targetLength == 0)
                    goto bad_xml_markup;
                if (contentIndex < 0) {
                    atom = cx->runtime->atomState.emptyAtom;
                } else {
                    atom = js_AtomizeChars(cx,
                                           tokenbuf.begin() + contentIndex,
                                           tokenbuf.length() - contentIndex,
                                           0);
                    if (!atom)
                        goto error;
                }
                tokenbuf.shrinkBy(tokenbuf.length() - targetLength);
                tp->t_atom2 = atom;
                tt = TOK_XMLPI;

        finish_xml_markup:
                if (!matchChar('>'))
                    goto bad_xml_markup;
                atom = atomize(cx, tokenbuf);
                if (!atom)
                    goto error;
                tp->t_atom = atom;
                tp->pos.end.lineno = lineno;
                goto out;
            }

            /* An XML start-of-tag character. */
            tt = matchChar('/') ? TOK_XMLETAGO : TOK_XMLSTAGO;
            goto out;

        bad_xml_markup:
            ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_BAD_XML_MARKUP);
            goto error;
        }
#endif /* JS_HAS_XML_SUPPORT */

        /* NB: treat HTML begin-comment as comment-till-end-of-line */
        if (matchChar('!')) {
            if (matchChar('-')) {
                if (matchChar('-')) {
                    flags |= TSF_IN_HTML_COMMENT;
                    goto skipline;
                }
                ungetChar('-');
            }
            ungetChar('!');
        }
        if (matchChar(c)) {
            tp->t_op = JSOP_LSH;
            tt = matchChar('=') ? TOK_ASSIGN : TOK_SHOP;
        } else {
            tp->t_op = matchChar('=') ? JSOP_LE : JSOP_LT;
            tt = TOK_RELOP;
        }
        break;

      case '>':
        if (matchChar(c)) {
            tp->t_op = matchChar(c) ? JSOP_URSH : JSOP_RSH;
            tt = matchChar('=') ? TOK_ASSIGN : TOK_SHOP;
        } else {
            tp->t_op = matchChar('=') ? JSOP_GE : JSOP_GT;
            tt = TOK_RELOP;
        }
        break;

      case '*':
        tp->t_op = JSOP_MUL;
        tt = matchChar('=') ? TOK_ASSIGN : TOK_STAR;
        break;

      case '/':
        if (matchChar('/')) {
            /*
             * Hack for source filters such as the Mozilla XUL preprocessor:
             * "//@line 123\n" sets the number of the *next* line after the
             * comment to 123.
             */
            if (JS_HAS_ATLINE_OPTION(cx)) {
                jschar cp[5];
                uintN i, line, temp;
                char filenameBuf[1024];

                if (peekChars(5, cp) &&
                    cp[0] == '@' &&
                    cp[1] == 'l' &&
                    cp[2] == 'i' &&
                    cp[3] == 'n' &&
                    cp[4] == 'e') {
                    skipChars(5);
                    while ((c = getChar()) != '\n' && ScanAsSpace((jschar)c))
                        continue;
                    if (JS7_ISDEC(c)) {
                        line = JS7_UNDEC(c);
                        while ((c = getChar()) != EOF && JS7_ISDEC(c)) {
                            temp = 10 * line + JS7_UNDEC(c);
                            if (temp < line) {
                                /* Ignore overlarge line numbers. */
                                goto skipline;
                            }
                            line = temp;
                        }
                        while (c != '\n' && ScanAsSpace((jschar)c))
                            c = getChar();
                        i = 0;
                        if (c == '"') {
                            while ((c = getChar()) != EOF && c != '"') {
                                if (c == '\n') {
                                    ungetChar(c);
                                    goto skipline;
                                }
                                if ((c >> 8) != 0 || i >= sizeof filenameBuf - 1)
                                    goto skipline;
                                filenameBuf[i++] = (char) c;
                            }
                            if (c == '"') {
                                while ((c = getChar()) != '\n' &&
                                       ScanAsSpace((jschar)c)) {
                                    continue;
                                }
                            }
                        }
                        filenameBuf[i] = '\0';
                        if (c == '\n') {
                            if (i > 0) {
                                if (flags & TSF_OWNFILENAME)
                                    cx->free((void *) filename);
                                filename = JS_strdup(cx, filenameBuf);
                                if (!filename)
                                    goto error;
                                flags |= TSF_OWNFILENAME;
                            }
                            lineno = line;
                        }
                    }
                    ungetChar(c);
                }
            }

  skipline:
            /* Optimize line skipping if we are not in an HTML comment. */
            if (flags & TSF_IN_HTML_COMMENT) {
                while ((c = getChar()) != EOF && c != '\n') {
                    if (c == '-' && matchChar('-') && matchChar('>'))
                        flags &= ~TSF_IN_HTML_COMMENT;
                }
            } else {
                while ((c = getChar()) != EOF && c != '\n')
                    continue;
            }
            ungetChar(c);
            cursor = (cursor - 1) & ntokensMask;
            goto retry;
        }

        if (matchChar('*')) {
            uintN linenoBefore = lineno;
            while ((c = getChar()) != EOF &&
                   !(c == '*' && matchChar('/'))) {
                /* Ignore all characters until comment close. */
            }
            if (c == EOF) {
                ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                         JSMSG_UNTERMINATED_COMMENT);
                goto error;
            }
            if ((flags & TSF_NEWLINES) && linenoBefore != lineno) {
                flags &= ~TSF_DIRTYLINE;
                tt = TOK_EOL;
                goto eol_out;
            }
            cursor = (cursor - 1) & ntokensMask;
            goto retry;
        }

        if (flags & TSF_OPERAND) {
            uintN reflags, length;
            JSBool inCharClass = JS_FALSE;

            tokenbuf.clear();
            for (;;) {
                c = getChar();
                if (c == '\n' || c == EOF) {
                    ungetChar(c);
                    ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR,
                                             JSMSG_UNTERMINATED_REGEXP);
                    goto error;
                }
                if (c == '\\') {
                    if (!tokenbuf.append(c))
                        goto error;
                    c = getChar();
                } else if (c == '[') {
                    inCharClass = JS_TRUE;
                } else if (c == ']') {
                    inCharClass = JS_FALSE;
                } else if (c == '/' && !inCharClass) {
                    /* For compat with IE, allow unescaped / in char classes. */
                    break;
                }
                if (!tokenbuf.append(c))
                    goto error;
            }
            for (reflags = 0, length = tokenbuf.length() + 1; ; length++) {
                c = peekChar();
                if (c == 'g' && !(reflags & JSREG_GLOB))
                    reflags |= JSREG_GLOB;
                else if (c == 'i' && !(reflags & JSREG_FOLD))
                    reflags |= JSREG_FOLD;
                else if (c == 'm' && !(reflags & JSREG_MULTILINE))
                    reflags |= JSREG_MULTILINE;
                else if (c == 'y' && !(reflags & JSREG_STICKY))
                    reflags |= JSREG_STICKY;
                else
                    break;
                getChar();
            }
            c = peekChar();
            if (JS7_ISLET(c)) {
                char buf[2] = { '\0' };
                tp->pos.begin.index += length + 1;
                buf[0] = (char)c;
                ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_BAD_REGEXP_FLAG,
                                         buf);
                (void) getChar();
                goto error;
            }
            tp->t_reflags = reflags;
            tt = TOK_REGEXP;
            break;
        }

        tp->t_op = JSOP_DIV;
        tt = matchChar('=') ? TOK_ASSIGN : TOK_DIVOP;
        break;

      case '%':
        tp->t_op = JSOP_MOD;
        tt = matchChar('=') ? TOK_ASSIGN : TOK_DIVOP;
        break;

      case '~':
        tp->t_op = JSOP_BITNOT;
        tt = TOK_UNARYOP;
        break;

      case '+':
        if (matchChar('=')) {
            tp->t_op = JSOP_ADD;
            tt = TOK_ASSIGN;
        } else if (matchChar(c)) {
            tt = TOK_INC;
        } else {
            tp->t_op = JSOP_POS;
            tt = TOK_PLUS;
        }
        break;

      case '-':
        if (matchChar('=')) {
            tp->t_op = JSOP_SUB;
            tt = TOK_ASSIGN;
        } else if (matchChar(c)) {
            if (peekChar() == '>' && !(flags & TSF_DIRTYLINE)) {
                flags &= ~TSF_IN_HTML_COMMENT;
                goto skipline;
            }
            tt = TOK_DEC;
        } else {
            tp->t_op = JSOP_NEG;
            tt = TOK_MINUS;
        }
        break;

#if JS_HAS_SHARP_VARS
      case '#':
      {
        uint32 n;

        c = getChar();
        if (!JS7_ISDEC(c)) {
            ungetChar(c);
            goto badchar;
        }
        n = (uint32)JS7_UNDEC(c);
        for (;;) {
            c = getChar();
            if (!JS7_ISDEC(c))
                break;
            n = 10 * n + JS7_UNDEC(c);
            if (n >= UINT16_LIMIT) {
                ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_SHARPVAR_TOO_BIG);
                goto error;
            }
        }
        tp->t_dval = (jsdouble) n;
        if (JS_HAS_STRICT_OPTION(cx) &&
            (c == '=' || c == '#')) {
            char buf[20];
            JS_snprintf(buf, sizeof buf, "#%u%c", n, c);
            if (!ReportCompileErrorNumber(cx, this, NULL, JSREPORT_WARNING | JSREPORT_STRICT,
                                          JSMSG_DEPRECATED_USAGE, buf)) {
                goto error;
            }
        }
        if (c == '=')
            tt = TOK_DEFSHARP;
        else if (c == '#')
            tt = TOK_USESHARP;
        else
            goto badchar;
        break;
      }
#endif /* JS_HAS_SHARP_VARS */

#if JS_HAS_SHARP_VARS || JS_HAS_XML_SUPPORT
      badchar:
#endif

      default:
        ReportCompileErrorNumber(cx, this, NULL, JSREPORT_ERROR, JSMSG_ILLEGAL_CHARACTER);
        goto error;
    }

  out:
    JS_ASSERT(tt != TOK_EOL);
    flags |= TSF_DIRTYLINE;

  eol_out:
    JS_ASSERT(tt < TOK_LIMIT);
    tp->pos.end.index = linepos + (linebuf.ptr - linebuf.base) - (ungetbuf.limit - ungetbuf.ptr);
    tp->type = tt;
    return tt;

  error:
    tt = TOK_ERROR;
    flags |= TSF_ERROR;
    goto out;
}

