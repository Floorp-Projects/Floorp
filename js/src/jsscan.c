/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JS lexical scanner.
 */
#include "jsstddef.h"
#include <stdio.h>	/* first to avoid trouble on some systems */
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

/* Unicode separators that are treated as line terminators, in addition to \n, \r */
#define LINE_SEPARATOR  (0x2028)
#define PARA_SEPARATOR  (0x2029)

#define RESERVE_JAVA_KEYWORDS
#define RESERVE_ECMA_KEYWORDS

static struct keyword {
    char        *name;
    JSTokenType tokentype;      /* JSTokenType */
    JSOp        op;             /* JSOp */
    JSVersion   version;        /* JSVersion */
} keywords[] = {
    {"break",           TOK_BREAK,              JSOP_NOP,   JSVERSION_DEFAULT},
    {"case",            TOK_CASE,               JSOP_NOP,   JSVERSION_DEFAULT},
    {"continue",        TOK_CONTINUE,           JSOP_NOP,   JSVERSION_DEFAULT},
    {"default",         TOK_DEFAULT,            JSOP_NOP,   JSVERSION_DEFAULT},
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
    if (ts->principals)
	JSPRINCIPALS_DROP(cx, ts->principals);
    return !ts->file || fclose(ts->file) == 0;
}

static int32
GetChar(JSTokenStream *ts)
{
    int32 c;
    ptrdiff_t len, olen;
    jschar *nl;

    if (ts->ungetpos != 0) {
	c = ts->ungetbuf[--ts->ungetpos];
    } else {
        do {
	    if (ts->linebuf.ptr == ts->linebuf.limit) {
	        len = PTRDIFF(ts->userbuf.limit, ts->userbuf.ptr, jschar);
	        if (len <= 0) {
		    /* Fill ts->userbuf so that \r and \r\n convert to \n. */
		    if (ts->file) {
		        JSBool crflag;
		        char cbuf[JS_LINE_LIMIT];
		        jschar *ubuf;
		        ptrdiff_t i, j;

		        crflag = (ts->flags & TSF_CRFLAG) != 0;
		        if (!fgets(cbuf, JS_LINE_LIMIT - crflag, ts->file)) {
			    ts->flags |= TSF_EOF;
			    return EOF;
		        }
		        len = olen = strlen(cbuf);
		        JS_ASSERT(len > 0);
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
		    } else {
		        ts->flags |= TSF_EOF;
		        return EOF;
		    }
	        }
                if (ts->listener) {
                    ts->listener(ts->filename, ts->lineno, ts->userbuf.ptr, len,
                                 &ts->listenerTSData, ts->listenerData);
                }

	        /*
	         * Any one of \n, \r, or \r\n ends a line (longest match wins).
                 * Also allow the Unicode line and paragraph separators.
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

	        /*
	         * If there was a line terminator, copy thru it into linebuf.
	         * Else copy JS_LINE_LIMIT-1 bytes into linebuf.
	         */
	        if (nl < ts->userbuf.limit)
		    len = PTRDIFF(nl, ts->userbuf.ptr, jschar) + 1;
	        if (len >= JS_LINE_LIMIT)
		    len = JS_LINE_LIMIT - 1;
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
			    } else
                                ts->linebuf.base[len-1] = '\n';
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
            report.lineno = cg->currentLine;
        }

#if JS_HAS_ERROR_EXCEPTIONS
	/*
	 * If there's a runtime exception type associated with this error
	 * number, set that as the pending exception.  For errors occuring at
	 * compile time, this is very likely to be a JSEXN_SYNTAXERR.  If an
	 * exception is thrown, then the JSREPORT_EXCEPTION flag will be set in
	 * report.flags.  Proper behavior for error reporters is probably to
	 * ignore this for all but toplevel compilation errors.
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
            (void) js_ErrorToException(cx, message, &report);

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
              CURRENT_TOKEN(ts).pos.end.lineno == ts->lineno);
    ts->flags |= TSF_NEWLINES;
    tt = js_PeekToken(cx, ts);
    ts->flags &= ~TSF_NEWLINES;
    return tt;
}

#define TBMIN   64

static JSBool
GrowTokenBuf(JSContext *cx, JSTokenBuf *tb)
{
    jschar *base;
    ptrdiff_t offset, length;
    size_t tbsize;
    JSArenaPool *pool;

    base = tb->base;
    offset = PTRDIFF(tb->ptr, base, jschar);
    pool = &cx->tempPool;
    if (!base) {
        tbsize = TBMIN * sizeof(jschar);
        length = TBMIN;
        JS_ARENA_ALLOCATE_CAST(base, jschar *, pool, tbsize);
    } else {
        length = PTRDIFF(tb->limit, base, jschar);
        tbsize = length * sizeof(jschar);
        length <<= 1;
        JS_ARENA_GROW_CAST(base, jschar *, pool, tbsize, tbsize);
    }
    if (!base) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    tb->base = base;
    tb->limit = base + length;
    tb->ptr = base + offset;
    return JS_TRUE;
}

static JSBool
AddToTokenBuf(JSContext *cx, JSTokenBuf *tb, jschar c)
{
    if (tb->ptr == tb->limit && !GrowTokenBuf(cx, tb))
	return JS_FALSE;
    *tb->ptr++ = c;
    return JS_TRUE;
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

JSTokenType
js_GetToken(JSContext *cx, JSTokenStream *ts)
{
    JSTokenType tt;
    JSToken *tp;
    int32 c;
    JSAtom *atom;
    JSBool hadUnicodeEscape;

#define INIT_TOKENBUF(tb)   ((tb)->ptr = (tb)->base)
#define FINISH_TOKENBUF(tb) if (!AddToTokenBuf(cx, tb, 0)) RETURN(TOK_ERROR)
#define TOKEN_LENGTH(tb)    ((tb)->ptr - (tb)->base - 1)
#define RETURN(tt)          { if (tt == TOK_ERROR) ts->flags |= TSF_ERROR;    \
			      tp->pos.end.index = ts->linepos +               \
				  (ts->linebuf.ptr - ts->linebuf.base) -      \
				  ts->ungetpos;                               \
			      return (tp->type = tt); }

    /* If there was a fatal error, keep returning TOK_ERROR. */
    if (ts->flags & TSF_ERROR)
	return TOK_ERROR;

    /* Check for a pushed-back token resulting from mismatching lookahead. */
    while (ts->lookahead != 0) {
        ts->lookahead--;
        ts->cursor = (ts->cursor + 1) & NTOKENS_MASK;
        tt = CURRENT_TOKEN(ts).type;
        if (tt != TOK_EOL || (ts->flags & TSF_NEWLINES))
            return tt;
    }

retry:
    do {
	c = GetChar(ts);
        if (c == '\n') {
            ts->flags &= ~TSF_DIRTYLINE;
            if (ts->flags & TSF_NEWLINES)
	        break;
        }
    } while (JS_ISSPACE(c));

    ts->cursor = (ts->cursor + 1) & NTOKENS_MASK;
    tp = &CURRENT_TOKEN(ts);
    tp->ptr = ts->linebuf.ptr - 1;
    tp->pos.begin.index = ts->linepos + (tp->ptr - ts->linebuf.base);
    tp->pos.begin.lineno = tp->pos.end.lineno = ts->lineno;

    if (c == EOF)
	RETURN(TOK_EOF);
    if (c != '-' && c != '\n')
        ts->flags |= TSF_DIRTYLINE;

    hadUnicodeEscape = JS_FALSE;
    if (JS_ISIDENT_START(c) ||
        (c == '\\' &&
         (c = GetUnicodeEscape(ts),
          hadUnicodeEscape = JS_ISIDENT_START(c)))) {
	INIT_TOKENBUF(&ts->tokenbuf);
        for (;;) {
	    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		RETURN(TOK_ERROR);
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
	FINISH_TOKENBUF(&ts->tokenbuf);

	atom = js_AtomizeChars(cx,
			       ts->tokenbuf.base,
			       TOKEN_LENGTH(&ts->tokenbuf),
			       0);
	if (!atom)
	    RETURN(TOK_ERROR);
        if (!hadUnicodeEscape && ATOM_KEYWORD(atom)) {
            struct keyword *kw = ATOM_KEYWORD(atom);

            if (JSVERSION_IS_ECMA(cx->version) || kw->version <= cx->version) {
                tp->t_op = (JSOp) kw->op;
                RETURN(kw->tokentype);
            }
        }
	tp->t_op = JSOP_NAME;
	tp->t_atom = atom;
	RETURN(TOK_NAME);
    }

    if (JS7_ISDEC(c) || (c == '.' && JS7_ISDEC(PeekChar(ts)))) {
	jsint radix;
	const jschar *endptr;
	jsdouble dval;

	radix = 10;
	INIT_TOKENBUF(&ts->tokenbuf);

	if (c == '0') {
	    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		RETURN(TOK_ERROR);
	    c = GetChar(ts);
	    if (JS_TOLOWER(c) == 'x') {
		if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		    RETURN(TOK_ERROR);
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
                        RETURN(TOK_ERROR);
                    }
		    radix = 10;
		}
            }
	    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		RETURN(TOK_ERROR);
	    c = GetChar(ts);
	}

	if (radix == 10 && (c == '.' || JS_TOLOWER(c) == 'e')) {
	    if (c == '.') {
		do {
		    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
			RETURN(TOK_ERROR);
		    c = GetChar(ts);
		} while (JS7_ISDEC(c));
	    }
	    if (JS_TOLOWER(c) == 'e') {
		if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		    RETURN(TOK_ERROR);
		c = GetChar(ts);
		if (c == '+' || c == '-') {
		    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
			RETURN(TOK_ERROR);
		    c = GetChar(ts);
		}
		if (!JS7_ISDEC(c)) {
		    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_MISSING_EXPONENT);
		    RETURN(TOK_ERROR);
		}
		do {
		    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
			RETURN(TOK_ERROR);
		    c = GetChar(ts);
		} while (JS7_ISDEC(c));
	    }
	}

	UngetChar(ts, c);
	FINISH_TOKENBUF(&ts->tokenbuf);

	if (radix == 10) {
	    if (!js_strtod(cx, ts->tokenbuf.base, &endptr, &dval)) {
		js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_OUT_OF_MEMORY);
		RETURN(TOK_ERROR);
	    }
	} else {
	    if (!js_strtointeger(cx, ts->tokenbuf.base, &endptr, radix, &dval)) {
		js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_OUT_OF_MEMORY);
		RETURN(TOK_ERROR);
	    }
	}
	tp->t_dval = dval;
	RETURN(TOK_NUMBER);
    }

    if (c == '"' || c == '\'') {
	int32 val, qc = c;

	INIT_TOKENBUF(&ts->tokenbuf);
	while ((c = GetChar(ts)) != qc) {
	    if (c == '\n' || c == EOF) {
		UngetChar(ts, c);
		js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_UNTERMINATED_STRING);
		RETURN(TOK_ERROR);
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
			val = JS7_UNDEC(c);
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
	    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		RETURN(TOK_ERROR);
	}
	FINISH_TOKENBUF(&ts->tokenbuf);
	atom = js_AtomizeChars(cx,
			       ts->tokenbuf.base,
			       TOKEN_LENGTH(&ts->tokenbuf),
			       0);
	if (!atom)
	    RETURN(TOK_ERROR);
	tp->pos.end.lineno = ts->lineno;
	tp->t_op = JSOP_STRING;
	tp->t_atom = atom;
	RETURN(TOK_STRING);
    }

    switch (c) {
      case '\n': 
        c = TOK_EOL; 
        break;

      case ';': c = TOK_SEMI; break;
      case '.': c = TOK_DOT; break;
      case '[': c = TOK_LB; break;
      case ']': c = TOK_RB; break;
      case '{': c = TOK_LC; break;
      case '}': c = TOK_RC; break;
      case '(': c = TOK_LP; break;
      case ')': c = TOK_RP; break;
      case ',': c = TOK_COMMA; break;
      case '?': c = TOK_HOOK; break;

      case ':':
        /*
         * Default so compiler can modify to JSOP_GETTER if 'p getter: v' in an
         * object initializer, likewise for setter.
         */
        tp->t_op = JSOP_NOP;
        c = TOK_COLON;
        break;

      case '|':
	if (MatchChar(ts, c)) {
	    c = TOK_OR;
	} else if (MatchChar(ts, '=')) {
	    tp->t_op = JSOP_BITOR;
	    c = TOK_ASSIGN;
	} else {
	    c = TOK_BITOR;
	}
	break;

      case '^':
	if (MatchChar(ts, '=')) {
	    tp->t_op = JSOP_BITXOR;
	    c = TOK_ASSIGN;
	} else {
	    c = TOK_BITXOR;
	}
	break;

      case '&':
	if (MatchChar(ts, c)) {
	    c = TOK_AND;
	} else if (MatchChar(ts, '=')) {
	    tp->t_op = JSOP_BITAND;
	    c = TOK_ASSIGN;
	} else {
	    c = TOK_BITAND;
	}
	break;

      case '=':
	if (MatchChar(ts, c)) {
#if JS_HAS_TRIPLE_EQOPS
	    tp->t_op = MatchChar(ts, c) ? JSOP_NEW_EQ : (JSOp)cx->jsop_eq;
#else
	    tp->t_op = cx->jsop_eq;
#endif
	    c = TOK_EQOP;
	} else {
	    tp->t_op = JSOP_NOP;
	    c = TOK_ASSIGN;
	}
	break;

      case '!':
	if (MatchChar(ts, '=')) {
#if JS_HAS_TRIPLE_EQOPS
	    tp->t_op = MatchChar(ts, '=') ? JSOP_NEW_NE : (JSOp)cx->jsop_ne;
#else
	    tp->t_op = cx->jsop_ne;
#endif
	    c = TOK_EQOP;
	} else {
	    tp->t_op = JSOP_NOT;
	    c = TOK_UNARYOP;
	}
	break;

      case '<':
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
	    c = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_SHOP;
	} else {
	    tp->t_op = MatchChar(ts, '=') ? JSOP_LE : JSOP_LT;
	    c = TOK_RELOP;
	}
	break;

      case '>':
	if (MatchChar(ts, c)) {
	    tp->t_op = MatchChar(ts, c) ? JSOP_URSH : JSOP_RSH;
	    c = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_SHOP;
	} else {
	    tp->t_op = MatchChar(ts, '=') ? JSOP_GE : JSOP_GT;
	    c = TOK_RELOP;
	}
	break;

      case '*':
	tp->t_op = JSOP_MUL;
	c = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_STAR;
	break;

      case '/':
	if (MatchChar(ts, '/')) {
skipline:
	    while ((c = GetChar(ts)) != EOF && c != '\n')
		/* skip to end of line */;
	    UngetChar(ts, c);
	    goto retry;
	}
	if (MatchChar(ts, '*')) {
	    while ((c = GetChar(ts)) != EOF &&
		   !(c == '*' && MatchChar(ts, '/'))) {
		if (c == '/' && MatchChar(ts, '*')) {
		    if (MatchChar(ts, '/'))
			goto retry;
		    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_NESTED_COMMENT);
		}
	    }
	    if (c == EOF) {
		js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                            JSMSG_UNTERMINATED_COMMENT);
		RETURN(TOK_ERROR);
	    }
	    goto retry;
	}

#if JS_HAS_REGEXPS
	if (ts->flags & TSF_REGEXP) {
	    JSObject *obj;
	    uintN flags;

	    INIT_TOKENBUF(&ts->tokenbuf);
	    while ((c = GetChar(ts)) != '/') {
		if (c == '\n' || c == EOF) {
		    UngetChar(ts, c);
		    js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                                JSMSG_UNTERMINATED_REGEXP);
		    RETURN(TOK_ERROR);
		}
		if (c == '\\') {
		    if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
			RETURN(TOK_ERROR);
		    c = GetChar(ts);
		}
		if (!AddToTokenBuf(cx, &ts->tokenbuf, (jschar)c))
		    RETURN(TOK_ERROR);
	    }
	    FINISH_TOKENBUF(&ts->tokenbuf);
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
		RETURN(TOK_ERROR);
	    }
	    obj = js_NewRegExpObject(cx, ts,
				     ts->tokenbuf.base,
				     TOKEN_LENGTH(&ts->tokenbuf),
				     flags);
            if (!obj)
                RETURN(TOK_ERROR);
	    atom = js_AtomizeObject(cx, obj, 0);
	    if (!atom)
		RETURN(TOK_ERROR);
	    tp->t_op = JSOP_OBJECT;
	    tp->t_atom = atom;
	    RETURN(TOK_OBJECT);
	}
#endif /* JS_HAS_REGEXPS */

	tp->t_op = JSOP_DIV;
	c = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_DIVOP;
	break;

      case '%':
	tp->t_op = JSOP_MOD;
	c = MatchChar(ts, '=') ? TOK_ASSIGN : TOK_DIVOP;
	break;

      case '~':
	tp->t_op = JSOP_BITNOT;
	c = TOK_UNARYOP;
	break;

      case '+':
	if (MatchChar(ts, '=')) {
	    tp->t_op = JSOP_ADD;
	    c = TOK_ASSIGN;
	} else if (MatchChar(ts, c)) {
	    c = TOK_INC;
	} else {
	    tp->t_op = JSOP_POS;
	    c = TOK_PLUS;
	}
	break;

      case '-':
        if (MatchChar(ts, '=')) {
            tp->t_op = JSOP_SUB;
            c = TOK_ASSIGN;
        } else if (MatchChar(ts, c)) {
            if (PeekChar(ts) == '>' && !(ts->flags & TSF_DIRTYLINE))
                goto skipline;
            c = TOK_DEC;
        } else {
            tp->t_op = JSOP_NEG;
            c = TOK_MINUS;
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
		RETURN(TOK_ERROR);
	    }
	}
	tp->t_dval = (jsdouble) n;
        if (JS_HAS_STRICT_OPTION(cx) &&
            (c == '=' || c == '#')) {
            char buf[20];
            JS_snprintf(buf, sizeof buf, "#%u%c", n, c);
            if (!JS_ReportErrorFlagsAndNumber(cx,
                                              JSREPORT_WARNING |
                                              JSREPORT_STRICT,
                                              js_GetErrorMessage, NULL,
                                              JSMSG_DEPRECATED_USAGE,
                                              buf)) {
                RETURN(TOK_ERROR);
            }
        }
	if (c == '=')
	    RETURN(TOK_DEFSHARP);
	if (c == '#')
	    RETURN(TOK_USESHARP);
	goto badchar;
      }

      badchar:
#endif /* JS_HAS_SHARP_VARS */

      default:
	js_ReportCompileErrorNumber(cx, ts, NULL, JSREPORT_ERROR,
                                    JSMSG_ILLEGAL_CHARACTER);
	RETURN(TOK_ERROR);
    }

    JS_ASSERT(c < TOK_LIMIT);
    RETURN((JSTokenType)c);

#undef INIT_TOKENBUF
#undef FINISH_TOKENBUF
#undef TOKEN_LENGTH
#undef RETURN
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
