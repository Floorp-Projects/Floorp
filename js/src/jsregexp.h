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

#ifndef jsregexp_h___
#define jsregexp_h___
/*
 * JS regular expression interface.
 */
#include <stddef.h>
#include "jspubtd.h"
#include "jsstr.h"

struct JSRegExpStatics {
    JSString    *input;         /* input string to match (perl $_, GC root) */
    JSBool      multiline;      /* whether input contains newlines (perl $*) */
    uintN       parenCount;     /* number of valid elements in parens[] */
    uintN       moreLength;     /* number of allocated elements in moreParens */
    JSSubString parens[9];      /* last set of parens matched (perl $1, $2) */
    JSSubString *moreParens;    /* null or realloc'd vector for $10, etc. */
    JSSubString lastMatch;      /* last string matched (perl $&) */
    JSSubString lastParen;      /* last paren matched (perl $+) */
    JSSubString leftContext;    /* input to left of last match (perl $`) */
    JSSubString rightContext;   /* input to right of last match (perl $') */
};

/*
 * This macro is safe because moreParens is guaranteed to be allocated and big
 * enough to hold parenCount, or else be null when parenCount is 0.
 */
#define REGEXP_PAREN_SUBSTRING(res, num)                                      \
    (((jsuint)(num) < (jsuint)(res)->parenCount)                              \
     ? ((jsuint)(num) < 9)                                                    \
       ? &(res)->parens[num]                                                  \
       : &(res)->moreParens[(num) - 9]                                        \
     : &js_EmptySubString)

struct JSRegExp {
    jsrefcount  nrefs;          /* reference count */
    JSString    *source;        /* locked source string, sans // */
    uintN       lastIndex;      /* index after last match, for //g iterator */
    uintN       parenCount;     /* number of parenthesized submatches */
    uint8       flags;          /* flags, see jsapi.h */
    struct RENode *ren;         /* regular expression tree root */
};

extern JSRegExp *
js_NewRegExp(JSContext *cx, JSTokenStream *ts,
             JSString *str, uintN flags, JSBool flat);

extern JSRegExp *
js_NewRegExpOpt(JSContext *cx, JSTokenStream *ts,
                JSString *str, JSString *opt, JSBool flat);

extern void
js_DestroyRegExp(JSContext *cx, JSRegExp *re);

/*
 * Execute re on input str at *indexp, returning null in *rval on mismatch.
 * On match, return true if test is true, otherwise return an array object.
 * Update *indexp and cx->regExpStatics always on match.
 */
extern JSBool
js_ExecuteRegExp(JSContext *cx, JSRegExp *re, JSString *str, size_t *indexp,
		 JSBool test, jsval *rval);

/*
 * These two add and remove GC roots, respectively, so their calls must be
 * well-ordered.
 */
extern JSBool
js_InitRegExpStatics(JSContext *cx, JSRegExpStatics *res);

extern void
js_FreeRegExpStatics(JSContext *cx, JSRegExpStatics *res);

#define JSVAL_IS_REGEXP(cx, v)                                                \
    (JSVAL_IS_OBJECT(v) && JSVAL_TO_OBJECT(v) &&                              \
     OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v)) == &js_RegExpClass)

extern JSClass js_RegExpClass;

extern JSObject *
js_InitRegExpClass(JSContext *cx, JSObject *obj);

/*
 * Create a new RegExp object.
 */
extern JSObject *
js_NewRegExpObject(JSContext *cx, JSTokenStream *ts,
                   jschar *chars, size_t length, uintN flags);

extern JSBool
js_XDRRegExp(JSXDRState *xdr, JSObject **objp);

#endif /* jsregexp_h___ */
