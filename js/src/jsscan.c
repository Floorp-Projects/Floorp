/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "jsstddef.h"
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
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsdtoa.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsemit.h"
#include "jsexn.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsregexp.h"
#include "jsscan.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#define RESERVE_JAVA_KEYWORDS
#define RESERVE_ECMA_KEYWORDS

static struct keyword {
    const char  *name;
    JSTokenType tokentype;      /* JSTokenType */
    JSOp        op;             /* JSOp */
    JSVersion   version;        /* JSVersion */
} keywords[] = {
    {"break",           TOK_BREAK,              JSOP_NOP,   JSVERSION_DEFAULT},
    {"case",            TOK_CASE,               JSOP_NOP,   JSVERSION_DEFAULT},
    {"continue",        TOK_CONTINUE,           JSOP_NOP,   JSVERSION_DEFAULT},
    {js_default_str,    TOK_DEFAULT,            JSOP_NOP,   JSVERSION_DEFAULT},
    {js_delete_str,     TOK_DELETE,             JSOP_NOP,   JSVERSION_DEFAULT},
    {"do",              TOK_DO,                 JSOP_NOP,   JSVERSION_DEFAULT},
    {"else",            TOK_ELSE,               JSOP_NOP,   JSVERSION_DEFAULT},
    {"export",          TOK_EXPORT,             JSOP_NOP,   JSVERSION_1_2},
    {js_false_str,      TOK_PRIMARY,            JSOP_FALSE, JSVERSION_DEFAULT},
    {"for",             TOK_FOR,                JSOP_NOP,   JSVERSION_DEFAULT},
    {js_function_str,   TOK_FUNCTION,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"if",              TOK_IF,                 JSOP_NOP,   JSVERSION_DEFAULT},
    {js_in_str,         TOK_IN,                 JSOP_IN,    JSVERSION_DEFAULT},
    {js_new_str,        TOK_NEW,                JSOP_NEW,   JSVERSION_DEFAULT},
    {js_null_str,       TOK_PRIMARY,            JSOP_NULL,  JSVERSION_DEFAULT},
    {"return",          TOK_RETURN,             JSOP_NOP,   JSVERSION_DEFAULT},
    {"switch",          TOK_SWITCH,             JSOP_NOP,   JSVERSION_DEFAULT},
    {js_this_str,       TOK_PRIMARY,            JSOP_THIS,  JSVERSION_DEFAULT},
    {js_true_str,       TOK_PRIMARY,            JSOP_TRUE,  JSVERSION_DEFAULT},
    {js_typeof_str,     TOK_UNARYOP,            JSOP_TYPEOF,JSVERSION_DEFAULT},
    {"var",             TOK_VAR,                JSOP_DEFVAR,JSVERSION_DEFAULT},
    {js_void_str,       TOK_UNARYOP,            JSOP_VOID,  JSVERSION_DEFAULT},
    {"while",           TOK_WHILE,              JSOP_NOP,   JSVERSION_DEFAULT},
    {"with",            TOK_WITH,               JSOP_NOP,   JSVERSION_DEFAULT},

#if JS_HAS_CONST
    {js_const_str,      TOK_VAR,                JSOP_DEFCONST,JSVERSION_DEFAULT},
#else
    {js_const_str,      TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
#endif

#if JS_HAS_EXCEPTIONS
    {"try",             TOK_TRY,                JSOP_NOP,   JSVERSION_DEFAULT},
    {"catch",           TOK_CATCH,              JSOP_NOP,   JSVERSION_DEFAULT},
    {"finally",         TOK_FINALLY,            JSOP_NOP,   JSVERSION_DEFAULT},
    {"throw",           TOK_THROW,              JSOP_NOP,   JSVERSION_DEFAULT},
#else
    {"try",             TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"catch",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"finally",         TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"throw",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
#endif

#if JS_HAS_INSTANCEOF
    {js_instanceof_str, TOK_INSTANCEOF,         JSOP_INSTANCEOF,JSVERSION_1_4},
#else
    {js_instanceof_str, TOK_RESERVED,           JSOP_NOP,   JSVERSION_1_4},
#endif

#ifdef RESERVE_JAVA_KEYWORDS
    {"abstract",        TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"boolean",         TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"byte",            TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"char",            TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"class",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"double",          TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"extends",         TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"final",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"float",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"goto",            TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"implements",      TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"import",          TOK_IMPORT,             JSOP_NOP,   JSVERSION_DEFAULT},
    {"int",             TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"interface",       TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"long",            TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"native",          TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"package",         TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"private",         TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"protected",       TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"public",          TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"short",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"static",          TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"super",           TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"synchronized",    TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"throws",          TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"transient",       TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"volatile",        TOK_RESERVED,           JSOP_NOP,   JSVERSION_DEFAULT},
#endif

#ifdef RESERVE_ECMA_KEYWORDS
    {"enum",           TOK_RESERVED,            JSOP_NOP,   JSVERSION_1_3},
#endif

#if JS_HAS_DEBUGGER_KEYWORD
    {"debugger",       TOK_DEBUGGER,            JSOP_NOP,   JSVERSION_1_3},
#elif defined(RESERVE_ECMA_KEYWORDS)
    {"debugger",       TOK_RESERVED,            JSOP_NOP,   JSVERSION_1_3},
#endif
    {0,                TOK_EOF,                 JSOP_NOP,   JSVERSION_DEFAULT}
};

JSBool
js_InitScanner(JSContext *cx)
{
    struct keyword *kw;
    JSAtom *atom;

    for (kw = keywords; kw->name; kw++) {
        atom = js_Atomize(cx, kw->name, strlen(kw->name), ATOM_PINNED);
        if (!atom)
            return JS_FALSE;
        ATOM_SET_KEYWORD(atom, kw);
    }
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_MapKeywords(void (*mapfun)(const char *))
{
    struct keyword *kw;

    for (kw = keywords; kw->name; kw++)
        mapfun(kw->name);
}

JSTokenStream *
js_NewTokenStream(JSContext *cx, const jschar *base, size_t length,
                  const char *filename, uintN lineno,
                  JSPrincipals *principals)
{
    JSTokenStream *ts;

    ts = js_NewBufferTokenStream(cx, base, length);
    if (!ts)
        return NULL;
    ts->filename = filename;
    ts->lineno = lineno;
    if (principals)
        JSPRINCIPALS_HOLD(cx, principals);
    ts->principals = principals;
    return ts;
}

#define TBMIN   64

static JSBool
GrowTokenBuf(JSStringBuffer *sb, size_t newlength)
{
    JSContext *cx;
    jschar *base;
    ptrdiff_t offset, length;
    size_t tbsize;
    JSArenaPool *pool;

    cx = sb->data;
    base = sb->base;
    offset = PTRDIFF(sb->ptr, base, jschar);
    pool = &cx->tempPool;
    if (!base) {
        tbsize = TBMIN * sizeof(jschar);
        length = TBMIN - 1;
        JS_ARENA_ALLOCATE_CAST(base, jschar *, pool, tbsize);
    } else {
        length = PTRDIFF(sb->limit, base, jschar);
        tbsize = (length + 1) * sizeof(jschar);
        length += length + 1;
        JS_ARENA_GROW_CAST(base, jschar *, pool, tbsize, tbsize);
    }
    if (!base) {
        JS_ReportOutOfMemory(cx);
        sb->base = STRING_BUFFER_ERROR_BASE;
        return JS_FALSE;
    }
    sb->base = base;
    sb->limit = base + length;
    sb->ptr = base + offset;
    return JS_TRUE;
}

JS_FRIEND_API(JSTokenStream *)
js_NewBufferTokenStream(JSContext *cx, const jschar *base, size_t length)
{
    size_t nb;
    JSTokenStream *ts;

    nb = sizeof(JSTokenStream) + JS_LINE_LIMIT * sizeof(jschar);
    JS_ARENA_ALLOCATE_CAST(ts, JSTokenStream *, &cx->tempPool, nb);
    if (!ts) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    memset(ts, 0, nb);
    ts->lineno = 1;
    ts->linebuf.base = ts->linebuf.limit = ts->linebuf.ptr = (jschar *)(ts + 1);
    ts->userbuf.base = (jschar *)base;
    ts->userbuf.limit = (jschar *)base + length;
    ts->userbuf.ptr = (jschar *)base;
    ts->tokenbuf.grow = GrowTokenBuf;
    ts->tokenbuf.data = cx;
    ts->listener = cx->runtime->sourceHandler;
    ts->listenerData = cx->runtime->sourceHandlerData;
    return ts;
}

JS_FRIEND_API(JSTokenStream *)
js_NewFileTokenStream(JSContext *cx, const char *filename, FILE *defaultfp)
{
    jschar *base;
    JSTokenStream *ts;
    FILE *file;

    JS_ARENA_ALLOCATE_CAST(base, jschar *, &cx->tempPool,
                           JS_LINE_LIMIT * sizeof(jschar));
    if (!base)
        return NULL;
    ts = js_NewBufferTokenStream(cx, base, JS_LINE_LIMIT);
    if (!ts)
        return NULL;
    if (!filename || strcmp(filename, "-") == 0) {
        file = defaultfp;
    } else {
        file = fopen(filename, "r");
        if (!file) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_OPEN,
                                 filename, "No such file or directory");
            return NULL;
        }
    }
    ts->userbuf.ptr = ts->userbuf.limit;
    ts->file = file;
    ts->filename = filename;
    return ts;
}

JS_FRIEND_API(JSBool)
js_CloseTokenStream(JSContext *cx, JSTokenStream *ts)
{
    if (ts->flags & TSF_OWNFILENAME)
        JS_free(cx, (void *) ts->filename);
    if (ts->principals)
        JSPRINCIPALS_DROP(cx, ts->principals);
    return !ts->file || fclose(ts->file) == 0;
}

static int
my_fgets(char *buf, int size, FILE *file)
{
    int n, i, c;
    JSBool crflag;

    n = size - 1;
    if (n < 0)
        return -1;

    crflag = JS_FALSE;
    for (i = 0; i < n && (c = getc(file)) != EOF; i++) {
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

static int32
GetChar(JSTokenStream *ts)
{
    int32 c;
    ptrdiff_t i, j, len, olen;
    JSBool crflag;
    char cbuf[JS_LINE_LIMIT];
    jschar *ubuf, *nl;

    if (ts->ungetpos != 0) {
        c = ts->ungetbuf[--ts->ungetpos];
    } else {
        do {
            if (ts->linebuf.ptr == ts->linebuf.limit) {
                len = PTRDIFF(ts->userbuf.limit, ts->userbuf.ptr, jschar);
                if (len <= 0) {
                    if (!ts->file) {
                        ts->flags |= TSF_EOF;
                        return EOF;
                    }

                    /* Fill ts->userbuf so that \r and \r\n convert to \n. */
                    crflag = (ts->flags & TSF_CRFLAG) != 0;
                    len = my_fgets(cbuf, JS_LINE_LIMIT - crflag, ts->file);
                    if (len <= 0) {
                        ts->flags |= TSF_EOF;
                        return EOF;
                    }
                    olen = len;
                    ubuf = ts->userbuf.base;
                    i = 0;
                    if (crflag) {
                        ts->flags &= ~TSF_CRFLAG;
                        if (cbuf[0] != '\n') {
                            ubuf[i++] = '\n';
                            len++;
                            ts->linepos--;
                        }
                    }
                    for (j = 0; i < len; i++, j++)
                        ubuf[i] = (jschar) (unsigned char) cbuf[j];
                    ts->userbuf.limit = ubuf + len;
                    ts->userbuf.ptr = ubuf;
                }
                if (ts->listener) {
                    ts->listener(ts->filename, ts->lineno, ts->userbuf.ptr, len,
                                 &ts->listenerTSData, ts->listenerData);
                }

                nl = ts->saveEOL;
                if (!nl) {
                    /*
                     * Any one of \n, \r, or \r\n ends a line (the longest
                     * match wins).  Also allow the Unicode line and paragraph
                     * separators.
                     */
                    for (nl = ts->userbuf.ptr; nl < ts->userbuf.limit; nl++) {
                        /*
                         * Try to prevent value-testing on most characters by
                         * filtering out characters that aren't 000x or 202x.
                         */
                        if ((*nl & 0xDFD0) == 0) {
                            if (*nl == '\n')
                                break;
                            if (*nl == '\r') {
                                if (nl + 1 < ts->userbuf.limit && nl[1] == '\n')
                                    nl++;
                                break;
                            }
                            if (*nl == LINE_SEPARATOR || *nl == PARA_SEPARATOR)
                                break;
                        }
                    }
                }

                /*
                 * If there was a line terminator, copy thru it into linebuf.
                 * Else copy JS_LINE_LIMIT-1 bytes into linebuf.
                 */
                if (nl < ts->userbuf.limit)
                    len = PTRDIFF(nl, ts->userbuf.ptr, jschar) + 1;
                if (len >= JS_LINE_LIMIT) {
                    len = JS_LINE_LIMIT - 1;
                    ts->saveEOL = nl;
                } else {
                    ts->saveEOL = NULL;
                }
                js_strncpy(ts->linebuf.base, ts->userbuf.ptr, len);
                ts->userbuf.ptr += len;
                olen = len;

                /*
                 * Make sure linebuf contains \n for EOL (don't do this in
                 * userbuf because the user's string might be readonly).
                 */
                if (nl < ts->userbuf.limit) {
                    if (*nl == '\r') {
                        if (ts->linebuf.base[len-1] == '\r') {
                            /*
                             * Does the line segment end in \r?  We must check
                             * for a \n at the front of the next segment before
                             * storing a \n into linebuf.  This case matters
                             * only when we're reading from a file.
                             */
                            if (nl + 1 == ts->userbuf.limit && ts->file) {
                                len--;
                                ts->flags |= TSF_CRFLAG; /* clear NLFLAG? */
                                if (len == 0) {
                                    /*
                                     * This can happen when a segment ends in
                                     * \r\r.  Start over.  ptr == limit in this
                                     * case, so we'll fall into buffer-filling
                                     * code.
                                     */
                                    return GetChar(ts);
                                }
                            } else {
                                ts->linebuf.base[len-1] = '\n';
                            }
                        }
                    } else if (*nl == '\n') {
                        if (nl > ts->userbuf.base &&
                            nl[-1] == '\r' &&
                            ts->linebuf.base[len-2] == '\r') {
                            len--;
                            JS_ASSERT(ts->linebuf.base[len] == '\n');
                            ts->linebuf.base[len-1] = '\n';
                        }
                    } else if (*nl == LINE_SEPARATOR || *nl == PARA_SEPARATOR) {
                        ts->linebuf.base[len-1] = '\n';
                    }
                }

                /* Reset linebuf based on adjusted segment length. */
                ts->linebuf.limit = ts->linebuf.base + len;
                ts->linebuf.ptr = ts->linebuf.base;

                /* Update position of linebuf within physical userbuf line. */
                if (!(ts->flags & TSF_NLFLAG))
                    ts->linepos += ts->linelen;
                else
                    ts->linepos = 0;
                if (ts->linebuf.limit[-1] == '\n')
                    ts->flags |= TSF_NLFLAG;
                else
                    ts->flags &= ~TSF_NLFLAG;

                /* Update linelen from original segment length. */
                ts->linelen = olen;
            }
            c = *ts->linebuf.ptr++;
        } while (JS_ISFORMAT(c));
    }
    if (c == '\n')
        ts->lineno++;
    return c;
}

static void
UngetChar(JSTokenStream *ts, int32 c)
{
    if (c == EOF)
        return;
    JS_ASSERT(ts->ungetpos < sizeof ts->ungetbuf / sizeof ts->ungetbuf[0]);
    if (c == '\n')
        ts->lineno--;
    ts->ungetbuf[ts->ungetpos++] = (jschar)c;
}

static int32
PeekChar(JSTokenStream *ts)
{
    int32 c;

    c = GetChar(ts);
    UngetChar(ts, c);
    return c;
}

static JSBool
PeekChars(JSTokenStream *ts, intN n, jschar *cp)
{
    intN i, j;
    int32 c;

    for (i = 0; i < n; i++) {
        c = GetChar(ts);
        if (c == EOF)
            break;
        cp[i] = (jschar)c;
    }
    for (j = i - 1; j >= 0; j--)
        UngetChar(ts, cp[j]);
    return i == n;
}

static void
SkipChars(JSTokenStream *ts, intN n)
{
    while (--n >= 0)
        GetChar(ts);
}

static JSBool
MatchChar(JSTokenStream *ts, int32 expect)
{
    int32 c;

    c = GetChar(ts);
    if (c == expect)
        return JS_TRUE;
    UngetChar(ts, c);
    return JS_FALSE;
}

JSBool
js_ReportCompileErrorNumber(JSContext *cx, JSTokenStream *ts,
                            JSCodeGenerator *cg, uintN flags,
                            const uintN errorNumber, ...)
{
    va_list ap;
    JSErrorReporter onError;
    JSErrorReport report;
    jschar *tokenptr;
    JSString *linestr = NULL;
    char *message;
    JSBool warning;

    if ((flags & JSREPORT_STRICT) && !JS_HAS_STRICT_OPTION(cx))
        return JS_TRUE;

    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = flags;
    report.errorNumber = errorNumber;
    message = NULL;

    va_start(ap, errorNumber);
    if (!js_ExpandErrorArguments(cx, js_GetErrorMessage, NULL,
                                 errorNumber, &message, &report, &warning,
                                 JS_TRUE, ap)) {
        return JS_FALSE;
    }
    va_end(ap);

    js_AddRoot(cx, &linestr, "error line buffer");

    JS_ASSERT(!ts || ts->linebuf.limit < ts->linebuf.base + JS_LINE_LIMIT);
    onError = cx->errorReporter;
    if (onError) {
        /*
         * We are typically called with non-null ts and null cg from jsparse.c.
         * We can be called with null ts from the regexp compilation functions.
         * The code generator (jsemit.c) may pass null ts and non-null cg.
         */
        if (ts) {
            report.filename = ts->filename;
            report.lineno = ts->lineno;
            linestr = js_NewStringCopyN(cx, ts->linebuf.base,
                                        ts->linebuf.limit - ts->linebuf.base,
                                        0);
            report.linebuf = linestr
                ? JS_GetStringBytes(linestr)
                : NULL;
            tokenptr =
                ts->tokens[(ts->cursor + ts->lookahead) & NTOKENS_MASK].ptr;
            report.tokenptr = linestr
                ? report.linebuf + (tokenptr - ts->linebuf.base)
                : NULL;
            report.uclinebuf = linestr
                ? JS_GetStringChars(linestr)
                : NULL;
            report.uctokenptr = linestr
                ? report.uclinebuf + (tokenptr - ts->linebuf.base)
                : NULL;
        } else if (cg) {
            report.filename = cg->filename;
            report.lineno = CG_CURRENT_LINE(cg);
        }

#if JS_HAS_ERROR_EXCEPTIONS
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

        /*
         * Only try to raise an exception if there isn't one already set -
         * otherwise the exception will describe only the last compile error,
         * which is likely spurious.
         */
        if (!(ts && (ts->flags & TSF_ERROR)))
            if (js_ErrorToException(cx, message, &report))
                onError = NULL;

        /*
         * Suppress any compiletime errors that don't occur at the top level.
         * This may still fail, as interplevel may be zero in contexts where we
         * don't really want to call the error reporter, as when js is called
         * by other code which could catch the error.
         */
        if (cx->interpLevel != 0)
            onError = NULL;
#endif
        if (cx->runtime->debugErrorHook && onError) {
            JSDebugErrorHook hook = cx->runtime->debugErrorHook;
            /* test local in case debugErrorHook changed on another thread */
            if (hook && !hook(cx, message, &report,
                              cx->runtime->debugErrorHookData)) {
                onError = NULL;
            }
        }
        if (onError)
            (*onError)(cx, message, &report);
    }
    if (message)
        JS_free(cx, message);
    if (report.messageArgs) {
        int i = 0;
        while (report.messageArgs[i])
            JS_free(cx, (void *)report.messageArgs[i++]);
        JS_free(cx, (void *)report.messageArgs);
    }
    if (report.ucmessage)
        JS_free(cx, (void *)report.ucmessage);

    js_RemoveRoot(cx->runtime, &linestr);

    if (ts && !JSREPORT_IS_WARNING(flags)) {
        /* Set the error flag to suppress spurious reports. */
        ts->flags |= TSF_ERROR;
    }
    return warning;
}

static JSBool
GrowStringBuffer(JSStringBuffer *sb, size_t newlength)
{
    ptrdiff_t offset;
    jschar *bp;

    offset = PTRDIFF(sb->ptr, sb->base, jschar);
    newlength += offset;
    bp = (jschar *) realloc(sb->base, (newlength + 1) * sizeof(jschar));
    if (!bp) {
        free(sb->base);
        sb->base = STRING_BUFFER_ERROR_BASE;
        return JS_FALSE;
    }
    sb->base = bp;
    sb->ptr = bp + offset;
    sb->limit = bp + newlength;
    return JS_TRUE;
}

static void
FreeStringBuffer(JSStringBuffer *sb)
{
    if (sb->base)
        free(sb->base);
}

void
js_InitStringBuffer(JSStringBuffer *sb)
{
    sb->base = sb->limit = sb->ptr = NULL;
    sb->grow = GrowStringBuffer;
    sb->free = FreeStringBuffer;
    sb->data = NULL;
}

void
js_FinishStringBuffer(JSStringBuffer *sb)
{
    sb->free(sb);
}

#define ENSURE_STRING_BUFFER(sb,n) \
    ((sb)->ptr + (n) <= (sb)->limit || sb->grow(sb, n))

void
js_AppendChar(JSStringBuffer *sb, jschar c)
{
    jschar *bp;

    if (!STRING_BUFFER_OK(sb))
        return;
    if (!ENSURE_STRING_BUFFER(sb, 1))
        return;
    bp = sb->ptr;
    *bp++ = c;
    *bp = 0;
    sb->ptr = bp;
}

#if JS_HAS_XML_SUPPORT

void
js_RepeatChar(JSStringBuffer *sb, jschar c, uintN count)
{
    jschar *bp;

    if (!STRING_BUFFER_OK(sb) || count == 0)
        return;
    if (!ENSURE_STRING_BUFFER(sb, count))
        return;
    for (bp = sb->ptr; count; --count)
        *bp++ = c;
    *bp = 0;
    sb->ptr = bp;
}

void
js_AppendCString(JSStringBuffer *sb, const char *asciiz)
{
    size_t length;
    jschar *bp;

    if (!STRING_BUFFER_OK(sb))
        return;
    length = strlen(asciiz);
    if (!ENSURE_STRING_BUFFER(sb, length))
        return;
    for (bp = sb->ptr; length; --length)
        *bp++ = (jschar) *asciiz++;
    *bp = 0;
    sb->ptr = bp;
}

void
js_AppendJSString(JSStringBuffer *sb, JSString *str)
{
    size_t length;
    jschar *bp;

    if (!STRING_BUFFER_OK(sb))
        return;
    length = JSSTRING_LENGTH(str);
    if (!ENSURE_STRING_BUFFER(sb, length))
        return;
    bp = sb->ptr;
    js_strncpy(bp, JSSTRING_CHARS(str), length);
    bp += length;
    *bp = 0;
    sb->ptr = bp;
}

#endif /* JS_HAS_XML_SUPPORT */

JSTokenType
js_PeekToken(JSContext *cx, JSTokenStream *ts)
{
    JSTokenType tt;

    if (ts->lookahead != 0) {
        tt = ts->tokens[(ts->cursor + ts->lookahead) & NTOKENS_MASK].type;
    } else {
        tt = js_GetToken(cx, ts);
        js_UngetToken(ts);
    }
    return tt;
}

JSTokenType
js_PeekTokenSameLine(JSContext *cx, JSTokenStream *ts)
{
    JSTokenType tt;

    JS_ASSERT(ts->lookahead == 0 ||
              ON_CURRENT_LINE(ts, CURRENT_TOKEN(ts).pos));
    ts->flags |= TSF_NEWLINES;
    tt = js_PeekToken(cx, ts);
    ts->flags &= ~TSF_NEWLINES;
    return tt;
}

/*
 * We have encountered a '\': check for a Unicode escape sequence after it,
 * returning the character code value if we found a Unicode escape sequence.
 * Otherwise, non-destructively return the original '\'.
 */
static int32
GetUnicodeEscape(JSTokenStream *ts)
{
    jschar cp[5];
    int32 c;

    if (PeekChars(ts, 5, cp) && cp[0] == 'u' &&
        JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]) &&
        JS7_ISHEX(cp[3]) && JS7_ISHEX(cp[4]))
    {
        c = (((((JS7_UNHEX(cp[1]) << 4)
                + JS7_UNHEX(cp[2])) << 4)
              + JS7_UNHEX(cp[3])) << 4)
            + JS7_UNHEX(cp[4]);
        SkipChars(ts, 5);
        return c;
    }
    return '\\';
}

static JSToken *
NewToken(JSTokenStream *ts)
{
    JSToken *tp;

    ts->cursor = (ts->cursor + 1) & NTOKENS_MASK;
    tp = &CURRENT_TOKEN(ts);
    tp->ptr = ts->linebuf.ptr - 1;
    tp->pos.begin.index = ts->linepos + (tp->ptr - ts->linebuf.base);
    tp->pos.begin.lineno = tp->pos.end.lineno = (uint16)ts->lineno;
    return tp;
}

JSTokenType
js_GetToken(JSContext *cx, JSTokenStream *ts)
{
    JSTokenType tt;
    int32 c, qc;
    JSToken *tp;
    JSAtom *atom;
    JSBool hadUnicodeEscape;

#define INIT_TOKENBUF()     (ts->tokenbuf.ptr = ts->tokenbuf.base)
#define TRIM_TOKENBUF(i)    (ts->tokenbuf.ptr = ts->tokenbuf.base + i)
#define TOKENBUF_LENGTH()   (ts->tokenbuf.ptr - ts->tokenbuf.base)
#define TOKENBUF_BASE()     (ts->tokenbuf.base)
#define TOKENBUF_CHAR(i)    (ts->tokenbuf.base[i])
#define TOKENBUF_TO_ATOM()  (js_AtomizeChars(cx,                              \
                                             TOKENBUF_BASE(),                 \
                                             TOKENBUF_LENGTH(),               \
                                             0))

#define ADD_TO_TOKENBUF(c)  js_AppendChar(&ts->tokenbuf, (jschar) (c))

    /* If there was a fatal error, keep returning TOK_ERROR. */
    if (ts->flags & TSF_ERROR)
        return TOK_ERROR;

    /* Check for a pushed-back token resulting from mismatching lookahead. */
    while (ts->lookahead != 0) {
        JS_ASSERT(!(ts->flags & TSF_XMLTEXTMODE));
        ts->lookahead--;
        ts->cursor = (ts->cursor + 1) & NTOKENS_MASK;
        tt = CURRENT_TOKEN(ts).type;
        if (tt != TOK_EOL || (ts->flags & TSF_NEWLINES))
            return tt;
    }

#if JS_HAS_XML_SUPPORT
    if (ts->flags & TSF_XMLTEXTMODE) {
        tp = NewToken(ts);
        INIT_TOKENBUF();
        qc = (ts->flags & TSF_XMLONLYMODE) ? '<' : '{';
        while ((c = GetChar(ts)) != qc && c != '<') {
            if (c == EOF) {
                tt = TOK_EOF;
                goto out;
            }

            if (c == '&') {
                /* XXXbe handle built-in XML 1.0 entities */
            }

            ADD_TO_TOKENBUF(c);
        }
        UngetChar(ts, c);

        if (TOKENBUF_LENGTH() == 0) {
            atom = NULL;
        } else {
            atom = TOKENBUF_TO_ATOM();
            if (!atom)
                goto error;
        }
        tp->pos.end.lineno = (uint16)ts->lineno;
        tp->t_op = JSOP_STRING;
        tp->t_atom = atom;
        tt = TOK_XMLTEXT;
        goto out;
    }

    if (ts->flags & TSF_XMLTAGMODE) {
        tp = NewToken(ts);
        c = GetChar(ts);
        if (JS_ISXMLSPACE(c)) {
            do {
                c = GetChar(ts);
            } while (JS_ISXMLSPACE(c));
            UngetChar(ts, c);
            tt = TOK_XMLSPACE;
            goto out;
        }

        if (c == EOF) {
            tt = TOK_EOF;
            goto out;
        }

        INIT_TOKENBUF();
        if (JS_ISXMLNAMESTART(c)) {
            ADD_TO_TOKENBUF(c);
            while ((c = GetChar(ts)) != EOF && JS_ISXMLNAME(c))
                ADD_TO_TOKENBUF(c);

            UngetChar(ts, c);
            atom = TOKENBUF_TO_ATOM();
            if (!atom)
                goto error;
            tp->t_op = JSOP_STRING;
            tp->t_atom = atom;
            tt = TOK_XMLNAME;
            goto out;
        }

        switch (c) {
          case '{':
            if (ts->flags & TSF_XMLONLYMODE)
                goto bad_xml_char;
            tt = TOK_LC;
            goto out;

          case '=':
            tt = TOK_ASSIGN;
            goto out;

          case '"':
          case '\'':
            qc = c;
            while ((c = GetChar(ts)) != qc) {
                if (c == EOF) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_UNTERMINATED_STRING);
                    goto error;
                }

                /*
                 * XML attribute values are double-quoted when pretty-printed
                 * so escape " if it is expressed directly in a single-quoted
                 * attribute value.
                 */
                if (c == '"') {
                    JS_ASSERT(qc == '\'');
                    js_AppendCString(&ts->tokenbuf, js_quot_entity_str);
                    continue;
                }

                if (c == '&') {
                    /* XXXbe handle built-in XML 1.0 entities */
                    continue;
                }

                ADD_TO_TOKENBUF(c);
            }
            atom = TOKENBUF_TO_ATOM();
            if (!atom)
                goto error;
            tp->pos.end.lineno = (uint16)ts->lineno;
            tp->t_op = JSOP_STRING;
            tp->t_atom = atom;
            tt = TOK_XMLATTR;
            goto out;

          case '>':
            tt = TOK_XMLTAGC;
            goto out;

          case '/':
            if (MatchChar(ts, '>')) {
                tt = TOK_XMLPTAGC;
                goto out;
            }
            /* FALL THROUGH */

          bad_xml_char:
          default:
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_BAD_XML_CHARACTER);
            goto error;
        }
        /* NOTREACHED */
    }
#endif /* JS_HAS_XML_SUPPORT */

retry:
    do {
        c = GetChar(ts);
        if (c == '\n') {
            ts->flags &= ~TSF_DIRTYLINE;
            if (ts->flags & TSF_NEWLINES)
                break;
        }
    } while (JS_ISSPACE(c));

    tp = NewToken(ts);
    if (c == EOF) {
        tt = TOK_EOF;
        goto out;
    }

    if (c != '-' && c != '\n')
        ts->flags |= TSF_DIRTYLINE;

    hadUnicodeEscape = JS_FALSE;
    if (JS_ISIDSTART(c) ||
        (c == '\\' &&
         (c = GetUnicodeEscape(ts),
          hadUnicodeEscape = JS_ISIDSTART(c)))) {
        INIT_TOKENBUF();
        for (;;) {
            ADD_TO_TOKENBUF(c);
            c = GetChar(ts);
            if (c == '\\') {
                c = GetUnicodeEscape(ts);
                if (!JS_ISIDENT(c))
                    break;
                hadUnicodeEscape = JS_TRUE;
            } else {
                if (!JS_ISIDENT(c))
                    break;
            }
        }
        UngetChar(ts, c);

        atom = TOKENBUF_TO_ATOM();
        if (!atom)
            goto error;
        if (!hadUnicodeEscape && ATOM_KEYWORD(atom)) {
            struct keyword *kw = ATOM_KEYWORD(atom);

            if (JSVERSION_IS_ECMA(cx->version) || kw->version <= cx->version) {
                tp->t_op = (JSOp) kw->op;
                tt = kw->tokentype;
                goto out;
            }
        }
        tp->t_op = JSOP_NAME;
        tp->t_atom = atom;
        tt = TOK_NAME;
        goto out;
    }

    if (JS7_ISDEC(c) || (c == '.' && JS7_ISDEC(PeekChar(ts)))) {
        jsint radix;
        const jschar *endptr;
        jsdouble dval;

        radix = 10;
        INIT_TOKENBUF();

        if (c == '0') {
            ADD_TO_TOKENBUF(c);
            c = GetChar(ts);
            if (JS_TOLOWER(c) == 'x') {
                ADD_TO_TOKENBUF(c);
                c = GetChar(ts);
                radix = 16;
            } else if (JS7_ISDEC(c)) {
                radix = 8;
            }
        }

        while (JS7_ISHEX(c)) {
            if (radix < 16) {
                if (JS7_ISLET(c))
                    break;

                /*
                 * We permit 08 and 09 as decimal numbers, which makes our
                 * behaviour a superset of the ECMA numeric grammar.  We might
                 * not always be so permissive, so we warn about it.
                 */
                if (radix == 8 && c >= '8') {
                    if (!js_ReportCompileErrorNumber(cx, ts, NULL,
                                                     JSREPORT_WARNING,
                                                     JSMSG_BAD_OCTAL,
                                                     c == '8' ? "08" : "09")) {
                        goto error;
                    }
                    radix = 10;
                }
            }
            ADD_TO_TOKENBUF(c);
            c = GetChar(ts);
        }

        if (radix == 10 && (c == '.' || JS_TOLOWER(c) == 'e')) {
            if (c == '.') {
                do {
                    ADD_TO_TOKENBUF(c);
                    c = GetChar(ts);
                } while (JS7_ISDEC(c));
            }
            if (JS_TOLOWER(c) == 'e') {
                ADD_TO_TOKENBUF(c);
                c = GetChar(ts);
                if (c == '+' || c == '-') {
                    ADD_TO_TOKENBUF(c);
                    c = GetChar(ts);
                }
                if (!JS7_ISDEC(c)) {
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_MISSING_EXPONENT);
                    goto error;
                }
                do {
                    ADD_TO_TOKENBUF(c);
                    c = GetChar(ts);
                } while (JS7_ISDEC(c));
            }
        }

        /* Put back the next char and NUL-terminate tokenbuf for js_strto*. */
        UngetChar(ts, c);
        ADD_TO_TOKENBUF(0);

        if (radix == 10) {
            if (!js_strtod(cx, TOKENBUF_BASE(), &endptr, &dval)) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_OUT_OF_MEMORY);
                goto error;
            }
        } else {
            if (!js_strtointeger(cx, TOKENBUF_BASE(), &endptr, radix, &dval)) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_OUT_OF_MEMORY);
                goto error;
            }
        }
        tp->t_dval = dval;
        tt = TOK_NUMBER;
        goto out;
    }

    if (c == '"' || c == '\'') {
        qc = c;
        INIT_TOKENBUF();
        while ((c = GetChar(ts)) != qc) {
            if (c == '\n' || c == EOF) {
                UngetChar(ts, c);
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_UNTERMINATED_STRING);
                goto error;
            }
            if (c == '\\') {
                switch (c = GetChar(ts)) {
                  case 'b': c = '\b'; break;
                  case 'f': c = '\f'; break;
                  case 'n': c = '\n'; break;
                  case 'r': c = '\r'; break;
                  case 't': c = '\t'; break;
                  case 'v': c = '\v'; break;

                  default:
                    if ('0' <= c && c < '8') {
                        int32 val = JS7_UNDEC(c);

                        c = PeekChar(ts);
                        if ('0' <= c && c < '8') {
                            val = 8 * val + JS7_UNDEC(c);
                            GetChar(ts);
                            c = PeekChar(ts);
                            if ('0' <= c && c < '8') {
                                int32 save = val;
                                val = 8 * val + JS7_UNDEC(c);
                                if (val <= 0377)
                                    GetChar(ts);
                                else
                                    val = save;
                            }
                        }

                        c = (jschar)val;
                    } else if (c == 'u') {
                        jschar cp[4];
                        if (PeekChars(ts, 4, cp) &&
                            JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) &&
                            JS7_ISHEX(cp[2]) && JS7_ISHEX(cp[3])) {
                            c = (((((JS7_UNHEX(cp[0]) << 4)
                                    + JS7_UNHEX(cp[1])) << 4)
                                  + JS7_UNHEX(cp[2])) << 4)
                                + JS7_UNHEX(cp[3]);
                            SkipChars(ts, 4);
                        }
                    } else if (c == 'x') {
                        jschar cp[2];
                        if (PeekChars(ts, 2, cp) &&
                            JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1])) {
                            c = (JS7_UNHEX(cp[0]) << 4) + JS7_UNHEX(cp[1]);
                            SkipChars(ts, 2);
                        }
                    } else if (c == '\n' && JSVERSION_IS_ECMA(cx->version)) {
                        /* ECMA follows C by removing escaped newlines. */
                        continue;
                    }
                    break;
                }
            }
            ADD_TO_TOKENBUF(c);
        }
        atom = TOKENBUF_TO_ATOM();
        if (!atom)
            goto error;
        tp->pos.end.lineno = (uint16)ts->lineno;
        tp->t_op = JSOP_STRING;
        tp->t_atom = atom;
        tt = TOK_STRING;
        goto out;
    }

    switch (c) {
      case '\n':
        tt = TOK_EOL;
        break;

      case ';': tt = TOK_SEMI; break;
      case '[': tt = TOK_LB; break;
      case ']': tt = TOK_RB; break;
      case '{': tt = TOK_LC; break;
      case '}': tt = TOK_RC; break;
      case '(': tt = TOK_LP; break;
      case ')': tt = TOK_RP; break;
      case ',': tt = TOK_COMMA; break;
      case '?': tt = TOK_HOOK; break;

      case '.':
#if JS_HAS_XML_SUPPORT
        if (MatchChar(ts, c))
            tt = TOK_DBLDOT;
        else
#endif
            tt = TOK_DOT;
        break;

      case ':':
#if JS_HAS_XML_SUPPORT
        if (JS_HAS_XML_OPTION(cx) && MatchChar(ts, c)) {
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
        if (MatchChar(ts, c)) {
            tt = TOK_OR;
        } else if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_BITOR;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITOR;
        }
        break;

      case '^':
        if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_BITXOR;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITXOR;
        }
        break;

      case '&':
        if (MatchChar(ts, c)) {
            tt = TOK_AND;
        } else if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_BITAND;
            tt = TOK_ASSIGN;
        } else {
            tt = TOK_BITAND;
        }
        break;

      case '=':
        if (MatchChar(ts, c)) {
#if JS_HAS_TRIPLE_EQOPS
            tp->t_op = MatchChar(ts, c) ? JSOP_NEW_EQ : (JSOp)cx->jsop_eq;
#else
            tp->t_op = cx->jsop_eq;
#endif
            tt = TOK_EQOP;
        } else {
            tp->t_op = JSOP_NOP;
            tt = TOK_ASSIGN;
        }
        break;

      case '!':
        if (MatchChar(ts, '=')) {
#if JS_HAS_TRIPLE_EQOPS
            tp->t_op = MatchChar(ts, '=') ? JSOP_NEW_NE : (JSOp)cx->jsop_ne;
#else
            tp->t_op = cx->jsop_ne;
#endif
            tt = TOK_EQOP;
        } else {
            tp->t_op = JSOP_NOT;
            tt = TOK_UNARYOP;
        }
        break;

#if JS_HAS_XML_SUPPORT
      case '@':
        if (JS_HAS_XML_OPTION(cx)) {
            tt = TOK_AT;
            break;
        }
        goto badchar;
#endif

      case '<':
#if JS_HAS_XML_SUPPORT
        if (JS_HAS_XML_OPTION(cx) && (ts->flags & TSF_OPERAND)) {
            /* Check for XML comment or CDATA section. */
            if (MatchChar(ts, '!')) {
                INIT_TOKENBUF();

                /* Scan XML comment. */
                if (MatchChar(ts, '-')) {
                    if (!MatchChar(ts, '-'))
                        goto bad_xml_markup;
                    while ((c = GetChar(ts)) != '-' || !MatchChar(ts, '-')) {
                        if (c == EOF)
                            goto bad_xml_markup;
                        ADD_TO_TOKENBUF(c);
                    }
                    tt = TOK_XMLCOMMENT;
                    tp->t_op = JSOP_XMLCOMMENT;
                    goto finish_xml_markup;
                }

                /* Scan CDATA section. */
                if (MatchChar(ts, '[')) {
                    jschar cp[6];
                    if (PeekChars(ts, 6, cp) &&
                        cp[0] == 'C' &&
                        cp[1] == 'D' &&
                        cp[2] == 'A' &&
                        cp[3] == 'T' &&
                        cp[4] == 'A' &&
                        cp[5] == '[') {
                        SkipChars(ts, 6);
                        while ((c = GetChar(ts)) != ']' ||
                               !MatchChar(ts, ']')) {
                            if (c == EOF)
                                goto bad_xml_markup;
                            ADD_TO_TOKENBUF(c);
                        }
                        tt = TOK_XMLCDATA;
                        tp->t_op = JSOP_XMLCDATA;
                        goto finish_xml_markup;
                    }
                    goto bad_xml_markup;
                }
            }

            /* Check for processing instruction. */
            if (MatchChar(ts, '?')) {
                JSBool inTarget = JS_TRUE;
                size_t targetLength = 0;
                ptrdiff_t contentIndex = -1;

                INIT_TOKENBUF();
                while ((c = GetChar(ts)) != '?' || PeekChar(ts) != '>') {
                    if (c == EOF)
                        goto bad_xml_markup;
                    if (inTarget) {
                        if (JS_ISXMLSPACE(c)) {
                            if (TOKENBUF_LENGTH() == 0)
                                goto bad_xml_markup;
                            inTarget = JS_FALSE;
                        } else {
                            if (!((TOKENBUF_LENGTH() == 0)
                                  ? JS_ISXMLNAMESTART(c)
                                  : JS_ISXMLNAME(c))) {
                                goto bad_xml_markup;
                            }
                            ++targetLength;
                        }
                    } else {
                        if (contentIndex < 0 && !JS_ISXMLSPACE(c))
                            contentIndex = TOKENBUF_LENGTH();
                    }
                    ADD_TO_TOKENBUF(c);
                }
                if (contentIndex < 0) {
                    tp->t_atom2 = NULL;
                } else {
                    atom = js_AtomizeChars(cx,
                                           &TOKENBUF_CHAR(contentIndex),
                                           TOKENBUF_LENGTH() - contentIndex,
                                           0);
                    if (!atom)
                        goto error;
                    tp->t_atom2 = atom;
                    TRIM_TOKENBUF(targetLength);
                }
                tt = TOK_XMLPI;

        finish_xml_markup:
                if (!MatchChar(ts, '>'))
                    goto bad_xml_markup;
                atom = TOKENBUF_TO_ATOM();
                if (!atom)
                    goto error;
                tp->t_atom = atom;
                tp->pos.end.lineno = (uint16)ts->lineno;
                goto out;
            }

            /* An XML start-of-tag character. */
            tt = MatchChar(ts, '/') ? TOK_XMLETAGO : TOK_XMLSTAGO;
            goto out;

        bad_xml_markup:
            js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                        JSMSG_BAD_XML_MARKUP);
            goto error;
        }
#endif /* JS_HAS_XML_SUPPORT */

        /* NB: treat HTML begin-comment as comment-till-end-of-line */
        if (MatchChar(ts, '!')) {
            if (MatchChar(ts, '-')) {
                if (MatchChar(ts, '-'))
                    goto skipline;
                UngetChar(ts, '-');
            }
            UngetChar(ts, '!');
        }
        if (MatchChar(ts, c)) {
            tp->t_op = JSOP_LSH;
            tt = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_SHOP;
        } else {
            tp->t_op = MatchChar(ts, '=') ? JSOP_LE : JSOP_LT;
            tt = TOK_RELOP;
        }
        break;

      case '>':
        if (MatchChar(ts, c)) {
            tp->t_op = MatchChar(ts, c) ? JSOP_URSH : JSOP_RSH;
            tt = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_SHOP;
        } else {
            tp->t_op = MatchChar(ts, '=') ? JSOP_GE : JSOP_GT;
            tt = TOK_RELOP;
        }
        break;

      case '*':
        tp->t_op = JSOP_MUL;
        tt = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_STAR;
        break;

      case '/':
        if (MatchChar(ts, '/')) {
            /*
             * Hack for source filters such as the Mozilla XUL preprocessor:
             * "//@line 123\n" sets the number of the *next* line after the
             * comment to 123.
             */
            if (JS_HAS_ATLINE_OPTION(cx)) {
                jschar cp[5];
                uintN i, line, temp;
                char filename[1024];

                if (PeekChars(ts, 5, cp) &&
                    cp[0] == '@' &&
                    cp[1] == 'l' &&
                    cp[2] == 'i' &&
                    cp[3] == 'n' &&
                    cp[4] == 'e') {
                    SkipChars(ts, 5);
                    while ((c = GetChar(ts)) != '\n' && JS_ISSPACE(c))
                        continue;
                    if (JS7_ISDEC(c)) {
                        line = JS7_UNDEC(c);
                        while ((c = GetChar(ts)) != EOF && JS7_ISDEC(c)) {
                            temp = 10 * line + JS7_UNDEC(c);
                            if (temp < line) {
                                /* Ignore overlarge line numbers. */
                                goto skipline;
                            }
                            line = temp;
                        }
                        while (c != '\n' && JS_ISSPACE(c))
                            c = GetChar(ts);
                        i = 0;
                        if (c == '"') {
                            while ((c = GetChar(ts)) != EOF && c != '"') {
                                if (c == '\n') {
                                    UngetChar(ts, c);
                                    goto skipline;
                                }
                                if ((c >> 8) != 0 || i >= sizeof filename - 1)
                                    goto skipline;
                                filename[i++] = (char) c;
                            }
                            if (c == '"') {
                                while ((c = GetChar(ts)) != '\n' &&
                                       JS_ISSPACE(c)) {
                                    continue;
                                }
                            }
                        }
                        filename[i] = '\0';
                        if (c == '\n') {
                            if (i > 0) {
                                if (ts->flags & TSF_OWNFILENAME)
                                    JS_free(cx, (void *) ts->filename);
                                ts->filename = JS_strdup(cx, filename);
                                if (!ts->filename)
                                    goto error;
                                ts->flags |= TSF_OWNFILENAME;
                            }
                            ts->lineno = line;
                        }
                    }
                    UngetChar(ts, c);
                }
            }

skipline:
            while ((c = GetChar(ts)) != EOF && c != '\n')
                continue;
            UngetChar(ts, c);
            goto retry;
        }
        if (MatchChar(ts, '*')) {
            while ((c = GetChar(ts)) != EOF &&
                   !(c == '*' && MatchChar(ts, '/'))) {
                /* Ignore all characters until comment close. */
            }
            if (c == EOF) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_UNTERMINATED_COMMENT);
                goto error;
            }
            goto retry;
        }

#if JS_HAS_REGEXPS
        if (ts->flags & TSF_OPERAND) {
            JSObject *obj;
            uintN flags;

            INIT_TOKENBUF();
            while ((c = GetChar(ts)) != '/') {
                if (c == '\n' || c == EOF) {
                    UngetChar(ts, c);
                    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_UNTERMINATED_REGEXP);
                    goto error;
                }
                if (c == '\\') {
                    ADD_TO_TOKENBUF(c);
                    c = GetChar(ts);
                }
                ADD_TO_TOKENBUF(c);
            }
            for (flags = 0; ; ) {
                if (MatchChar(ts, 'g'))
                    flags |= JSREG_GLOB;
                else if (MatchChar(ts, 'i'))
                    flags |= JSREG_FOLD;
                else if (MatchChar(ts, 'm'))
                    flags |= JSREG_MULTILINE;
                else
                    break;
            }
            c = PeekChar(ts);
            if (JS7_ISLET(c)) {
                tp->ptr = ts->linebuf.ptr - 1;
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_BAD_REGEXP_FLAG);
                (void) GetChar(ts);
                goto error;
            }
            obj = js_NewRegExpObject(cx, ts,
                                     TOKENBUF_BASE(),
                                     TOKENBUF_LENGTH(),
                                     flags);
            if (!obj)
                goto error;
            atom = js_AtomizeObject(cx, obj, 0);
            if (!atom)
                goto error;

            /*
             * If the regexp's script is one-shot, we can avoid the extra
             * fork-on-exec costs of JSOP_REGEXP by selecting JSOP_OBJECT.
             * Otherwise, to avoid incorrect proto, parent, and lastIndex
             * sharing among threads and sequentially across re-execution,
             * select JSOP_REGEXP.
             */
            tp->t_op = (cx->fp->flags & (JSFRAME_EVAL | JSFRAME_COMPILE_N_GO))
                       ? JSOP_OBJECT
                       : JSOP_REGEXP;
            tp->t_atom = atom;
            tt = TOK_OBJECT;
            break;
        }
#endif /* JS_HAS_REGEXPS */

        tp->t_op = JSOP_DIV;
        tt = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_DIVOP;
        break;

      case '%':
        tp->t_op = JSOP_MOD;
        tt = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_DIVOP;
        break;

      case '~':
        tp->t_op = JSOP_BITNOT;
        tt = TOK_UNARYOP;
        break;

      case '+':
        if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_ADD;
            tt = TOK_ASSIGN;
        } else if (MatchChar(ts, c)) {
            tt = TOK_INC;
        } else {
            tp->t_op = JSOP_POS;
            tt = TOK_PLUS;
        }
        break;

      case '-':
        if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_SUB;
            tt = TOK_ASSIGN;
        } else if (MatchChar(ts, c)) {
            if (PeekChar(ts) == '>' && !(ts->flags & TSF_DIRTYLINE))
                goto skipline;
            tt = TOK_DEC;
        } else {
            tp->t_op = JSOP_NEG;
            tt = TOK_MINUS;
        }
        ts->flags |= TSF_DIRTYLINE;
        break;

#if JS_HAS_SHARP_VARS
      case '#':
      {
        uint32 n;

        c = GetChar(ts);
        if (!JS7_ISDEC(c)) {
            UngetChar(ts, c);
            goto badchar;
        }
        n = (uint32)JS7_UNDEC(c);
        for (;;) {
            c = GetChar(ts);
            if (!JS7_ISDEC(c))
                break;
            n = 10 * n + JS7_UNDEC(c);
            if (n >= ATOM_INDEX_LIMIT) {
                js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_SHARPVAR_TOO_BIG);
                goto error;
            }
        }
        tp->t_dval = (jsdouble) n;
        if (JS_HAS_STRICT_OPTION(cx) &&
            (c == '=' || c == '#')) {
            char buf[20];
            JS_snprintf(buf, sizeof buf, "#%u%c", n, c);
            if (!js_ReportCompileErrorNumber(cx, ts, NULL,
                                             JSREPORT_WARNING |
                                             JSREPORT_STRICT,
                                             JSMSG_DEPRECATED_USAGE,
                                             buf)) {
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
        js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_ILLEGAL_CHARACTER);
        goto error;
    }

out:
    if (!STRING_BUFFER_OK(&ts->tokenbuf))
        tt = TOK_ERROR;
    JS_ASSERT(tt < TOK_LIMIT);
    tp->pos.end.index = ts->linepos +
                        (ts->linebuf.ptr - ts->linebuf.base) -
                        ts->ungetpos;
    tp->type = tt;
    return tt;

error:
    tt = TOK_ERROR;
    ts->flags |= TSF_ERROR;
    goto out;

#undef INIT_TOKENBUF
#undef TRIM_TOKENBUF
#undef TOKENBUF_LENGTH
#undef TOKENBUF_BASE
#undef TOKENBUF_CHAR
#undef TOKENBUF_TO_ATOM
#undef ADD_TO_TOKENBUF
}

void
js_UngetToken(JSTokenStream *ts)
{
    JS_ASSERT(ts->lookahead < NTOKENS_MASK);
    if (ts->flags & TSF_ERROR)
        return;
    ts->lookahead++;
    ts->cursor = (ts->cursor - 1) & NTOKENS_MASK;
}

JSBool
js_MatchToken(JSContext *cx, JSTokenStream *ts, JSTokenType tt)
{
    if (js_GetToken(cx, ts) == tt)
        return JS_TRUE;
    js_UngetToken(ts);
    return JS_FALSE;
}
