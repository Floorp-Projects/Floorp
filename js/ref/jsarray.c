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
 * JS array class.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prprintf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"


/* Determine if the id represents an array index.
 * 
 * An id is an array index according to ECMA by (15.4):
 *
 * "Array objects give special treatment to a certain class of property names.
 * A property name P (in the form of a string value) is an array index if and
 * only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal 
 * to 2^32-1."
 * 
 * In our implementation, it would be sufficient to check for JSVAL_IS_INT(id)
 * except that by using signed 32-bit integers we miss the top half of the 
 * valid range. This function checks the string representation itself; note
 * that calling a standard conversion routine might allow strings such as 
 * "08" or "4.0" as array indices, which they are not.
 */
static JSBool
IdIsIndex(jsid id, jsuint *indexp)
{
    jsuint i;
    JSString *str;
    jschar *ep;

    if (JSVAL_IS_INT(id)) {
        i = JSVAL_TO_INT(id);
        if (i < 0)
            return JS_FALSE;
        *indexp = (jsuint)i;
        return JS_TRUE;
    }

    /* It must be a string. */
    str = JSVAL_TO_STRING(id);
    ep = str->chars;
    if (str->length == 0 || *ep < '1' || *ep > '9')
        return JS_FALSE;
    i = 0;
    while (*ep != 0 && JS7_ISDEC(*ep)) {
        jsuint i2 = i*10 + (*ep - '0');
        if (i2 < i)
            return JS_FALSE;    /* overflow */
        i = i2;
        ep++;
    }
    if (*ep != 0 || i == 4294967295)    /* 4294967295 == 2^32-1 */
        return JS_FALSE;
    *indexp = i;
    return JS_TRUE;
}


static JSBool
ValueIsLength(JSContext *cx, jsval v, jsuint *lengthp)
{
    jsint i;
    JSBool res = JS_FALSE;
    
    /* It's only an array length if it's an int or a double.  Some relevant
     * ECMA language is 15.4.2.2 - 'If the argument len is a number, then the
     * length property of the newly constructed object is set to ToUint32(len)
     * - I take 'is a number' to mean 'typeof len' returns 'number' in
     * javascript.
     */
    if (JSVAL_IS_INT(v)) {
        i = JSVAL_TO_INT(v);
        /* jsuint cast does ToUint32 */
	if (lengthp)
            *lengthp = (jsuint) i;
        res = JS_TRUE;
    } else if (JSVAL_IS_DOUBLE(v)) {
        /* XXXmccabe I'd love to add another check here, against
         * js_DoubleToInteger(d) != d), so that ValueIsLength matches
         * IdIsIndex, but it doesn't seem to follow from ECMA.
         * (seems to be appropriate for IdIsIndex, though).
	 */
	res = js_ValueToECMAUint32(cx, v, (uint32 *)lengthp);
    }
    return res;	
}


static JSBool
ValueToIndex(JSContext *cx, jsval v, jsuint *lengthp)
{
    jsint i;

    if (JSVAL_IS_INT(v)) {
	i = JSVAL_TO_INT(v);
        /* jsuint cast does ToUint32. */
	if (lengthp)
            *lengthp = (jsuint)i;
        return JS_TRUE;
    }
    return js_ValueToECMAUint32(cx, v, (uint32 *)lengthp);
}

JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    jsid id;
    jsint i;
    jsval v;

    id = (jsid) cx->runtime->atomState.lengthAtom;
    if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
	return JS_FALSE;

    /* Short-circuit, because js_ValueToECMAUint32 fails when
     * called during init time.
     */
    if (JSVAL_IS_INT(v)) {
        i = JSVAL_TO_INT(v);
        /* jsuint cast does ToUint32. */
        *lengthp = (jsuint)i;
        return JS_TRUE;
    }
    return js_ValueToECMAUint32(cx, v, (uint32 *)lengthp);
}

static JSBool
IndexToValue(JSContext *cx, jsuint length, jsval *vp)
{
    if (length <= JSVAL_INT_MAX) {
        *vp = INT_TO_JSVAL(length);
        return JS_TRUE;
    }
    return js_NewDoubleValue(cx, (jsdouble)length, vp);
}

static JSBool
IndexToId(JSContext *cx, jsuint length, jsid *idp)
{
    JSString *str;
    JSAtom *atom;

    if (length <= JSVAL_INT_MAX) {
	*idp = (jsid) INT_TO_JSVAL(length);
    } else {
	str = js_NumberToString(cx, (jsdouble)length);
	if (!str)
	    return JS_FALSE;
	atom = js_AtomizeString(cx, str, 0);
	if (!atom)
	    return JS_FALSE;
    	*idp = (jsid)atom;

    }
    return JS_TRUE;
}

JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsuint length)
{
    jsval v;
    jsid id;

    if (!IndexToValue(cx, length, &v))
    	return JS_FALSE;
    id = (jsid) cx->runtime->atomState.lengthAtom;
    return OBJ_SET_PROPERTY(cx, obj, id, &v);
}

JSBool
js_HasLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    JSErrorReporter older;
    jsid id;
    JSBool ok;
    jsval v;

    older = JS_SetErrorReporter(cx, NULL);
    id = (jsid) cx->runtime->atomState.lengthAtom;
    ok = OBJ_GET_PROPERTY(cx, obj, id, &v);
    JS_SetErrorReporter(cx, older);
    if (!ok)
        return JS_FALSE;
    return ValueIsLength(cx, v, lengthp);
}

/*
 * This get function is specific to Array.prototype.length and other array
 * instance length properties.  It calls back through the class get function
 * in case some magic happens there (see call_getProperty in jsfun.c).
 */
static JSBool
array_length_getter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return OBJ_GET_CLASS(cx, obj)->getProperty(cx, obj, id, vp);
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint newlen, oldlen, slot;
    jsid id2;
    jsval junk;

    if (!ValueToIndex(cx, *vp, &newlen))
	return JS_FALSE;
    if (!js_GetLengthProperty(cx, obj, &oldlen))
	return JS_FALSE;
    for (slot = newlen; slot < oldlen; slot++) {
	if (!IndexToId(cx, slot, &id2))
	    return JS_FALSE;
	if (!OBJ_DELETE_PROPERTY(cx, obj, id2, &junk))
	    return JS_FALSE;
    }
    return IndexToValue(cx, newlen, vp);
}

static JSBool
array_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsuint index, length;

    if (!(IdIsIndex(id, &index)))
	return JS_TRUE;
    if (!js_GetLengthProperty(cx, obj, &length))
    	return JS_FALSE;
    if (index >= length) {
	length = index + 1;
	return js_SetLengthProperty(cx, obj, length);
    }
    return JS_TRUE;
}

static JSBool
array_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    jsuint length;

    if (cx->version == JSVERSION_1_2) {
	if (!js_GetLengthProperty(cx, obj, &length))
	    return JS_FALSE;
	switch (type) {
	  case JSTYPE_NUMBER:
	    return IndexToValue(cx, length, vp);
	  case JSTYPE_BOOLEAN:
	    *vp = BOOLEAN_TO_JSVAL(length > 0);
	    return JS_TRUE;
	  default:
	    return JS_TRUE;
	}
    }
    js_TryValueOf(cx, obj, type, vp);
    return JS_TRUE;
}

JSClass js_ArrayClass = {
    "Array",
    0,
    array_addProperty, JS_PropertyStub,   JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    array_convert,     JS_FinalizeStub
};

static JSBool
array_join_sub(JSContext *cx, JSObject *obj, JSString *sep, JSBool literalize,
	       jsval *rval)
{
    JSBool ok;
    jsval v;
    jsuint length, index;
    jschar *chars, *ochars;
    size_t nchars, growth, seplen;
    const jschar *sepstr;
    JSString *str;
    PRHashEntry *he;

    ok = js_GetLengthProperty(cx, obj, &length);
    if (!ok)
	return JS_FALSE;
    ok = JS_TRUE;

    if (literalize) {
	he = js_EnterSharpObject(cx, obj, NULL, &chars);
	if (!he)
	    return JS_FALSE;
	if (IS_SHARP(he)) {
#if JS_HAS_SHARP_VARS
	    nchars = js_strlen(chars);
#else
	    chars[0] = '[';
	    chars[1] = ']';
	    chars[2] = 0;
	    nchars = 2;
#endif
	    goto make_string;
	}

	/*
	 * Allocate 1 + 3 + 1 for "[", the worst-case closing ", ]", and the
	 * terminating 0.
	 */
	growth = (1 + 3 + 1) * sizeof(jschar);
	if (!chars) {
	    nchars = 0;
	    chars = malloc(growth);
	    if (!chars)
		goto done;
	} else {
	    MAKE_SHARP(he);
	    nchars = js_strlen(chars);
	    chars = realloc((ochars = chars), nchars * sizeof(jschar) + growth);
	    if (!chars) {
	    	free(ochars);
		goto done;
	    }
	}
	chars[nchars++] = '[';
    } else {
	if (length == 0) {
	    *rval = JS_GetEmptyStringValue(cx);
	    return ok;
	}
	chars = NULL;
	nchars = 0;
    }
    sepstr = NULL;
    seplen = sep->length;

    v = JSVAL_NULL;
    for (index = 0; index < length; index++) {
	ok = JS_GetElement(cx, obj, index, &v);
	if (!ok)
	    goto done;

	if (JSVAL_IS_VOID(v) || JSVAL_IS_NULL(v)) {
	    str = cx->runtime->emptyString;
	} else {
	    str = (literalize ? js_ValueToSource : js_ValueToString)(cx, v);
	    if (!str) {
		ok = JS_FALSE;
		goto done;
	    }
	}

	/* Allocate 3 + 1 at end for ", ", closing bracket, and zero. */
	growth = (nchars + (sepstr ? seplen : 0) +
		  str->length +
		  3 + 1) * sizeof(jschar);
	if (!chars) {
	    chars = malloc(growth);
	    if (!chars)
		goto done;
	} else {
	    chars = realloc((ochars = chars), growth);
	    if (!chars) {
		free(ochars);
		goto done;
	    }
	}

	if (sepstr) {
	    js_strncpy(&chars[nchars], sepstr, seplen);
	    nchars += seplen;
	}
	sepstr = sep->chars;

	js_strncpy(&chars[nchars], str->chars, str->length);
	nchars += str->length;
    }

  done:
    if (literalize) {
	if (chars) {
	    if (JSVAL_IS_VOID(v)) {
		chars[nchars++] = ',';
		chars[nchars++] = ' ';
	    }
	    chars[nchars++] = ']';
	}
	js_LeaveSharpObject(cx, NULL);
    }
    if (!ok) {
	if (chars)
	    free(chars);
	return ok;
    }

  make_string:
    if (!chars) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
    chars[nchars] = 0;
    str = js_NewString(cx, chars, nchars, 0);
    if (!str) {
	free(chars);
	return JS_FALSE;
    }
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static jschar   comma_space_ucstr[] = {',', ' ', 0};
static jschar   comma_ucstr[]       = {',', 0};
static JSString comma_space         = {2, comma_space_ucstr};
static JSString comma               = {1, comma_ucstr};

#if JS_HAS_TOSOURCE
static JSBool
array_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    return array_join_sub(cx, obj, &comma_space, JS_TRUE, rval);
}
#endif

static JSBool
array_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSBool literalize;

    /*
     * JS1.2 arrays convert to array literals, with a comma followed by a space
     * between each element.
     */
    literalize = (cx->version == JSVERSION_1_2);
    return array_join_sub(cx, obj, literalize ? &comma_space : &comma,
			  literalize, rval);
}

static JSBool
array_join(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;

    if (argc == 0)
	return array_join_sub(cx, obj, &comma, JS_FALSE, rval);
    str = js_ValueToString(cx, argv[0]);
    if (!str)
	return JS_FALSE;
    argv[0] = STRING_TO_JSVAL(str);
    return array_join_sub(cx, obj, str, JS_FALSE, rval);
}

#if !JS_HAS_MORE_PERL_FUN
static JSBool
array_nyi(JSContext *cx, const char *what)
{
    JS_ReportError(cx, "sorry, Array.prototype.%s is not yet implemented",
		   what);
    return JS_FALSE;
}
#endif

static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, jsuint length, jsval *vector)
{
    jsval v;
    jsid id;
    jsuint index;

    if (!IndexToValue(cx, length, &v))
    	return JS_FALSE;
    id = (jsid) cx->runtime->atomState.lengthAtom;
    if (!js_DefineProperty(cx, obj, id, v,
			   array_length_getter, array_length_setter,
			   JSPROP_PERMANENT,
			   NULL)) {
	  return JS_FALSE;
    }
    if (!vector)
	return JS_TRUE;
    for (index = 0; index < length; index++) {
	if (!IndexToId(cx, index, &id))
	    return JS_FALSE;
	if (!js_DefineProperty(cx, obj, id, vector[index],
			       JS_PropertyStub, JS_PropertyStub,
			       JSPROP_ENUMERATE,
			       NULL)) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
array_reverse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    jsuint len, half, i;
    jsid id, id2;
    jsval v, v2;

    if (!js_GetLengthProperty(cx, obj, &len))
	return JS_FALSE;

    half = len / 2;
    for (i = 0; i < half; i++) {
	if (!IndexToId(cx, i, &id))
	    return JS_FALSE;
	if (!IndexToId(cx, len - i - 1, &id2))
	    return JS_FALSE;
	if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
	    return JS_FALSE;
	if (!OBJ_GET_PROPERTY(cx, obj, id2, &v2))
	    return JS_FALSE;
    	if (!OBJ_SET_PROPERTY(cx, obj, id, &v2))
	    return JS_FALSE;
    	if (!OBJ_SET_PROPERTY(cx, obj, id2, &v))
	    return JS_FALSE;
    }

    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

typedef struct QSortArgs {
    void         *vec;
    size_t       elsize;
    void         *pivot;
    JSComparator cmp;
    void         *arg;
} QSortArgs;

static void
js_qsort_r(QSortArgs *qa, int lo, int hi)
{
    void *pivot, *a, *b;
    int i, j;

    pivot = qa->pivot;
    while (lo < hi) {
	i = lo;
	j = hi;
	a = (char *)qa->vec + i * qa->elsize;
	memmove(pivot, a, qa->elsize);
	while (i < j) {
	    for (;;) {
		b = (char *)qa->vec + j * qa->elsize;
		if ((*qa->cmp)(b, pivot, qa->arg) <= 0)
		    break;
		j--;
	    }
	    memmove(a, b, qa->elsize);
	    while (i < j && (*qa->cmp)(a, pivot, qa->arg) <= 0) {
		i++;
		a = (char *)qa->vec + i * qa->elsize;
	    }
	    memmove(b, a, qa->elsize);
	}
	memmove(a, pivot, qa->elsize);
	if (i - lo < hi - i) {
	    js_qsort_r(qa, lo, i - 1);
	    lo = i + 1;
	} else {
	    js_qsort_r(qa, i + 1, hi);
	    hi = i - 1;
	}
    }
}

PRBool
js_qsort(void *vec, size_t nel, size_t elsize, JSComparator cmp, void *arg)
{
    void *pivot;
    QSortArgs qa;

    pivot = malloc(elsize);
    if (!pivot)
	return PR_FALSE;
    qa.vec = vec;
    qa.elsize = elsize;
    qa.pivot = pivot;
    qa.cmp = cmp;
    qa.arg = arg;
    js_qsort_r(&qa, 0, (int)(nel - 1));
    free(pivot);
    return PR_TRUE;
}

typedef struct CompareArgs {
    JSContext  *context;
    jsval      fval;
    JSBool     status;
} CompareArgs;

static int
sort_compare(const void *a, const void *b, void *arg)
{
    jsval av = *(const jsval *)a, bv = *(const jsval *)b;
    CompareArgs *ca = arg;
    JSContext *cx = ca->context;
    jsdouble cmp = -1;
    jsval fval, argv[2], rval;
    JSBool ok;

    fval = ca->fval;
    if (fval == JSVAL_NULL) {
	JSString *astr, *bstr;

	if (av == bv) {
	    cmp = 0;
	} else if (av == JSVAL_VOID || bv == JSVAL_VOID) {
	    /* Put undefined properties at the end. */
	    cmp = (av == JSVAL_VOID) ? 1 : -1;
	} else if ((astr = js_ValueToString(cx, av)) != NULL &&
		   (bstr = js_ValueToString(cx, bv)) != NULL) {
	    cmp = js_CompareStrings(astr, bstr);
	} else {
	    ca->status = JS_FALSE;
	}
    } else {
	argv[0] = av;
	argv[1] = bv;
	ok = js_CallFunctionValue(cx,
				  OBJ_GET_PARENT(cx, JSVAL_TO_OBJECT(fval)),
				  fval, 2, argv, &rval);
	if (ok) {
	    ok = js_ValueToNumber(cx, rval, &cmp);
            /* Clamp cmp to -1, 0, 1. */
            if (JSDOUBLE_IS_NaN(cmp)) {
                /* XXX report some kind of error here?  ECMA talks about
                 * 'consistent compare functions' that don't return NaN, but is
                 * silent about what the result should be.  So we currently
                 * ignore it.
                 */
                cmp = 0;
            } else if (cmp != 0) {
                cmp = cmp > 0 ? 1 : -1;
            }
        } else {
	    ca->status = ok;
        }
    }
    return (int)cmp;
}

/* XXXmccabe do the sort helper functions need to take int?  (Or can we claim
 * that 2^32 * 32 is too large to worry about?)  Something dumps when I change
 * to unsigned int; is qsort using -1 as a fencepost?
 */
static JSBool
array_sort(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval;
    CompareArgs ca;
    jsuint len, i;
    jsval *vec;
    jsid id;

    if (argc > 0) {
	if (JSVAL_IS_PRIMITIVE(argv[0])) {
	    JS_ReportError(cx, "invalid Array.prototype.sort argument");
	    return JS_FALSE;
	}
	fval = argv[0];
    } else {
	fval = JSVAL_NULL;
    }

    if (!js_GetLengthProperty(cx, obj, &len))
    	return JS_FALSE;
    vec = JS_malloc(cx, (size_t) len * sizeof(jsval));
    if (!vec)
	return JS_FALSE;

    for (i = 0; i < len; i++) {
	ca.status = IndexToId(cx, i, &id);
	if (!ca.status)
	    goto out;
	ca.status = OBJ_GET_PROPERTY(cx, obj, id, &vec[i]);
	if (!ca.status)
	    goto out;
    }

    ca.context = cx;
    ca.fval = fval;
    ca.status = JS_TRUE;
    if (!js_qsort(vec, (size_t) len, sizeof(jsval), sort_compare, &ca)) {
	JS_ReportOutOfMemory(cx);
	ca.status = JS_FALSE;
    }

    if (ca.status) {
	ca.status = InitArrayObject(cx, obj, len, vec);
	if (ca.status)
	    *rval = OBJECT_TO_JSVAL(obj);
    }
out:
    if (vec)
	JS_free(cx, vec);
    return ca.status;
}

#ifdef NOTYET
/*
 * From "Programming perl", Larry Wall and Randall L. Schwartz, Copyright XXX
 * O'Reilly & Associates, Inc., but with Java primitive type sizes for i, l,
 * and so on:
 *
 *  a   An ASCII string, will be null padded.
 *  A   An ASCII string, will be space padded.
 *  b   A bit string, low-to-high order.
 *  B   A bit string, high-to-low order.
 *  h   A hexadecimal string, low nybble first.
 *  H   A hexadecimal string, high nybble first.
 *  c   A signed char value.
 *  C   An unsigned char value.
 *  s   A signed short (16-bit) value.
 *  S   An unsigned short (16-bit) value.
 *  i   A signed integer (32-bit) value.
 *  I   An unsigned integer (32-bit) value.
 *  l   A signed long (64-bit) value.
 *  L   An unsigned long (64-bit) value.
 *  n   A short in "network" byte order.
 *  N   An integer in "network" byte order.
 *  f   A single-precision float in IEEE format.
 *  d   A double-precision float in IEEE format.
 *  p   A pointer to a string.
 *  x   A null byte.
 *  X   Back up one byte.
 *  @   Null-fill to absolute position.
 *  u   A uuencoded string.
 *
 * Each letter may be followed by a number giving the repeat count.  Together
 * the letter and repeat count make a field specifier.  Field specifiers may
 * be separated by whitespace, which will be ignored.
 */
static JSBool
array_pack(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
#else
    return array_nyi(cx, "pack");
#endif
}
#endif /* NOTYET */

static JSBool
array_push(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsuint length;
    uintN i;
    jsid id;

    if (!js_GetLengthProperty(cx, obj, &length))
	return JS_FALSE;
    for (i = 0; i < argc; i++) {
	if (!IndexToId(cx, length + i, &id))
	    return JS_FALSE;
	if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
	    return JS_FALSE;
    }

    /*
     * If JS1.2, follow Perl4 by returning the last thing pushed.  Otherwise,
     * return the new array length.
     */
    length += argc;
    if (cx->version == JSVERSION_1_2) {
	*rval = argc ? argv[argc-1] : JSVAL_VOID;
    } else {
	if (!IndexToValue(cx, length, rval))
	    return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
#else
    return array_nyi(cx, "push");
#endif
}

static JSBool
array_pop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsuint index;
    jsid id;
    jsval junk;

    if (!js_GetLengthProperty(cx, obj, &index))
    	return JS_FALSE;
    if (index > 0) {
	index--;
	if (!IndexToId(cx, index, &id))
	    return JS_FALSE;

	/* Get the to-be-deleted property's value into rval. */
	if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
	    return JS_FALSE;

	if (!OBJ_DELETE_PROPERTY(cx, obj, id, &junk))
	    return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, index);
#else
    return array_nyi(cx, "pop");
#endif
}

static JSBool
array_shift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsuint length, i;
    jsid id, id2;
    jsval v, junk;

    if (!js_GetLengthProperty(cx, obj, &length))
    	return JS_FALSE;
    if (length > 0) {
	length--;
	id = JSVAL_ZERO;

	/* Get the to-be-deleted property's value into rval ASAP. */
	if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
	    return JS_FALSE;

	/*
	 * Slide down the array above the first element.
	 */
	if (length > 0) {
	    for (i = 1; i <= length; i++) {
		if (!IndexToId(cx, i, &id))
		    return JS_FALSE;
		if (!IndexToId(cx, i - 1, &id2))
		    return JS_FALSE;
		if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
		    return JS_FALSE;
		if (!OBJ_SET_PROPERTY(cx, obj, id2, &v))
		    return JS_FALSE;
	    }
	}

	/* Delete the only or last element. */
	if (!OBJ_DELETE_PROPERTY(cx, obj, id, &junk))
	    return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
#else
    return array_nyi(cx, "shift");
#endif
}

static JSBool
array_unshift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsuint length, last;
    uintN i;
    jsid id, id2;
    jsval v;

    if (!js_GetLengthProperty(cx, obj, &length))
	return JS_FALSE;
    if (argc > 0) {
	/* Slide up the array to make room for argc at the bottom. */
	if (length > 0) {
            last = length;
            while (last--) {
		if (!IndexToId(cx, last, &id))
		    return JS_FALSE;
		if (!IndexToId(cx, last + argc, &id2))
		    return JS_FALSE;
		if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
		    return JS_FALSE;
		if (!OBJ_SET_PROPERTY(cx, obj, id2, &v))
		    return JS_FALSE;
            }
	}

	/* Copy from argv to the bottom of the array. */
	for (i = 0; i < argc; i++) {
	    if (!IndexToId(cx, i, &id))
		return JS_FALSE;
	    if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
		return JS_FALSE;
	}

	/* Follow Perl by returning the new array length. */
	length += argc;
	if (!js_SetLengthProperty(cx, obj, length))
	    return JS_FALSE;
    }
    return IndexToValue(cx, length, rval);
#else
    return array_nyi(cx, "unshift");
#endif
}

static JSBool
array_splice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsuint length, begin, end, count, delta, last;
    uintN i;
    jsdouble d;
    jsid id, id2;
    jsval v;
    JSObject *obj2;

    /* Nothing to do if no args.  Otherwise lock and load length. */
    if (argc == 0)
	return JS_TRUE;
    if (!js_GetLengthProperty(cx, obj, &length))
	return JS_FALSE;

    /* Convert the first argument into a starting index. */
    if (!js_ValueToNumber(cx, *argv, &d))
	return JS_FALSE;
    d = js_DoubleToInteger(d);
    if (d < 0) {
	d += length;
	if (d < 0)
	    d = 0;
    } else if (d > length) {
	d = length;
    }
    begin = (jsuint)d; /* d has been clamped to uint32 */
    argc--;
    argv++;

    /* Convert the second argument from a count into a fencepost index. */
    delta = length - begin;
    if (argc == 0) {
	count = delta;
	end = length;
    } else {
	if (!js_ValueToNumber(cx, *argv, &d))
	    return JS_FALSE;
	d = js_DoubleToInteger(d);
	if (d < 0)
	    d = 0;
	else if (d > delta)
	    d = delta;
	count = (jsuint)d;
	end = begin + count;
	argc--;
	argv++;
    }

    if (count == 1 && cx->version == JSVERSION_1_2) {
	/*
	 * JS lacks "list context", whereby in Perl one turns the single
	 * scalar that's spliced out into an array just by assigning it to
	 * @single instead of $single, or by using it as Perl push's first
	 * argument, for instance.
	 *
	 * JS1.2 emulated Perl too closely and returned a non-Array for
	 * the single-splice-out case, requiring callers to test and wrap
	 * in [] if necessary.  So JS1.3, default, and other versions all
	 * return an array of length 1 for uniformity.
	 */
	if (!IndexToId(cx, begin, &id))
	    return JS_FALSE;
	if (!OBJ_GET_PROPERTY(cx, obj, id, rval))
	    return JS_FALSE;
    } else {
	if (cx->version != JSVERSION_1_2 || count > 0) {
	    /*
	     * Create a new array value to return.  Our ECMA v2 proposal specs
	     * that splice always returns an array value, even when given no
	     * arguments.  We think this is best because it eliminates the need
	     * for callers to do an extra test to handle the empty splice case.
	     */
	    obj2 = js_NewArrayObject(cx, 0, NULL);
	    if (!obj2)
		return JS_FALSE;
	    *rval = OBJECT_TO_JSVAL(obj2);
	}

	/* If there are elements to remove, put them into the return value. */
	if (count > 0) {
	    for (last = begin; last < end; last++) {
		if (!IndexToId(cx, last, &id))
		    return JS_FALSE;
		if (!IndexToId(cx, last - begin, &id2))
		    return JS_FALSE;
		if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
		    return JS_FALSE;
		if (!OBJ_SET_PROPERTY(cx, obj2, id2, &v))
		    return JS_FALSE;
	    }
	}
    }

    /* Find the direction (up or down) to copy and make way for argv. */
    if (argc > count) {
        delta = (jsuint)argc - count;
        last = length;
        /* (uint) end could be 0, so can't use vanilla >= test */
        while (last-- > end) {
	    if (!IndexToId(cx, last, &id))
		return JS_FALSE;
	    if (!IndexToId(cx, last + delta, &id2))
		return JS_FALSE;
	    if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
		return JS_FALSE;
	    if (!OBJ_SET_PROPERTY(cx, obj, id2, &v))
		return JS_FALSE;
	}
        length += delta;
    } else if (argc < count) {
        delta = count - (jsuint)argc;
	for (last = end; last < length; last++) {
	    if (!IndexToId(cx, last, &id))
		return JS_FALSE;
	    if (!IndexToId(cx, last - delta, &id2))
		return JS_FALSE;
	    if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
		return JS_FALSE;
	    if (!OBJ_SET_PROPERTY(cx, obj, id2, &v))
		return JS_FALSE;
	}
        length -= delta;
    }

    /* Copy from argv into the hole to complete the splice. */
    for (i = 0; i < argc; i++) {
	if (!IndexToId(cx, begin + i, &id))
	    return JS_FALSE;
	if (!OBJ_SET_PROPERTY(cx, obj, id, &argv[i]))
	    return JS_FALSE;
    }

    /* Update length in case we deleted elements from the end. */
    return js_SetLengthProperty(cx, obj, length);
#else
    return array_nyi(cx, "splice");
#endif
}

#if JS_HAS_SEQUENCE_OPS
/*
 * Python-esque sequence operations.
 */
static JSBool
array_concat(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *nobj, *aobj;
    jsuint slot, length, alength;
    jsid id, id2;
    jsval v;
    uintN i;

    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
	return JS_FALSE;

    /* Only add the first element as an array if it looks like one.  Treat the
     * target the same way as the arguments.
     */

    /* XXXmccabe Might make sense to recast all of this as a do-while. */
    if (js_HasLengthProperty(cx, obj, &length)) {
        for (slot = 0; slot < length; slot++) {
            if (!IndexToId(cx, slot, &id))
                return JS_FALSE;
            if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
                return JS_FALSE;
            if (!OBJ_SET_PROPERTY(cx, nobj, id, &v))
                return JS_FALSE;
        }
    } else {
        length = 1;
        v = OBJECT_TO_JSVAL(obj);
        if (!OBJ_SET_PROPERTY(cx, nobj, JSVAL_ZERO, &v))
            return JS_FALSE;
    }

    for (i = 0; i < argc; i++) {
	v = argv[i];
	if (JSVAL_IS_OBJECT(v)) {
	    aobj = JSVAL_TO_OBJECT(v);
	    if (aobj && js_HasLengthProperty(cx, aobj, &alength)) {
		for (slot = 0; slot < alength; slot++) {
		    if (!IndexToId(cx, slot, &id))
			return JS_FALSE;
		    if (!IndexToId(cx, length + slot, &id2))
			return JS_FALSE;
		    if (!OBJ_GET_PROPERTY(cx, aobj, id, &v))
			return JS_FALSE;
		    if (!OBJ_SET_PROPERTY(cx, nobj, id2, &v))
			return JS_FALSE;
		}
		length += alength;
		continue;
	    }
	}

	if (!IndexToId(cx, length, &id))
	    return JS_FALSE;
	if (!OBJ_SET_PROPERTY(cx, nobj, id, &v))
	    return JS_FALSE;
	length++;
    }

    *rval = OBJECT_TO_JSVAL(nobj);
    return JS_TRUE;
}

static JSBool
array_slice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *nobj;
    jsuint length, begin, end, slot;
    jsdouble d;
    jsid id, id2;
    jsval v;

    nobj = js_NewArrayObject(cx, 0, NULL);
    if (!nobj)
	return JS_FALSE;

    if (!js_GetLengthProperty(cx, obj, &length))
	return JS_FALSE;
    begin = 0;
    end = length;

    if (argc > 0) {
	if (!js_ValueToNumber(cx, argv[0], &d))
	    return JS_FALSE;
	d = js_DoubleToInteger(d);
	if (d < 0) {
	    d += length;
	    if (d < 0)
		d = 0;
	} else if (d > length) {
	    d = length;
	}
	begin = (jsuint)d;

	if (argc > 1) {
	    if (!js_ValueToNumber(cx, argv[1], &d))
		return JS_FALSE;
	    d = js_DoubleToInteger(d);
	    if (d < 0) {
		d += length;
		if (d < 0)
		    d = 0;
	    } else if (d > length) {
		d = length;
	    }
	    end = (jsuint)d;
	}
    }

    for (slot = begin; slot < end; slot++) {
	if (!IndexToId(cx, slot, &id))
	    return JS_FALSE;
	if (!IndexToId(cx, slot - begin, &id2))
	    return JS_FALSE;
	if (!OBJ_GET_PROPERTY(cx, obj, id, &v))
	    return JS_FALSE;
	if (!OBJ_SET_PROPERTY(cx, nobj, id2, &v))
	    return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(nobj);
    return JS_TRUE;
}
#endif /* JS_HAS_SEQUENCE_OPS */

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   array_toSource,         0},
#endif
    {js_toString_str,   array_toString,         0},

    /* Perl-ish methods. */
    {"join",            array_join,             1},
    {"reverse",         array_reverse,          0},
    {"sort",            array_sort,             1},
#ifdef NOTYET
    {"pack",            array_pack,             1},
#endif
    {"push",            array_push,             1},
    {"pop",             array_pop,              0},
    {"shift",           array_shift,            0},
    {"unshift",         array_unshift,          1},
    {"splice",          array_splice,           1},

    /* Python-esque sequence methods. */
#if JS_HAS_SEQUENCE_OPS
    {"concat",          array_concat,           0},
    {"slice",           array_slice,            0},
#endif

    {0}
};

static JSBool
Array(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsuint length;
    jsval *vector;

    /* If called without new, replace obj with a new Array object. */
    if (!cx->fp->constructing) {
    	obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
    	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }

    if (argc == 0) {
	length = 0;
	vector = NULL;
    } else if (cx->version != JSVERSION_1_2 &&
	       argc == 1 && ValueIsLength(cx, argv[0], &length))
    {
	/*
	 * Only use 1 arg as first element for version 1.2; for any other
	 * version (including 1.3), follow ECMA and use it as a length.
         */
	vector = NULL;
    } else {
	length = (jsuint) argc;
	vector = argv;
    }
    return InitArrayObject(cx, obj, length, vector);
}

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &js_ArrayClass, Array, 1,
                         NULL, array_methods, NULL, NULL);

    /* Initialize the Array prototype object so it gets a length property. */
    if (!proto || !InitArrayObject(cx, proto, 0, NULL))
        return NULL;
    return proto;
}

JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, jsval *vector)
{
    JSObject *obj;

    obj = js_NewObject(cx, &js_ArrayClass, NULL, NULL);
    if (!obj)
	return NULL;
    if (!InitArrayObject(cx, obj, length, vector)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    return obj;
}
