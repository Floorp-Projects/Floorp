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
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "prprf.h"
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
#include "jsopcode.h"
#include "jsscope.h"
#include "jsstr.h"

static JSBool
ValueToLength(JSContext *cx, jsval v, jsint *lengthp)
{
    jsint i;
    jsdouble d;

    if (JSVAL_IS_INT(v)) {
	i = JSVAL_TO_INT(v);
	*lengthp = (i < 0) ? 0 : i;
    } else {
	if (JSVAL_IS_DOUBLE(v)) {
	    d = *JSVAL_TO_DOUBLE(v);
	} else {
	    if (!js_ValueToNumber(cx, v, &d))
		return JS_FALSE;
	}
	*lengthp = (d < 0 || d > JSVAL_INT_MAX || JSDOUBLE_IS_NaN(d))
		   ? 0
		   : (jsint)d;
    }
    return JS_TRUE;
}

static JSProperty *
GetLengthProperty(JSContext *cx, JSObject *obj, jsint *lengthp)
{
    JSProperty *prop;
    jsval v;

    PR_ASSERT(JS_IS_LOCKED(cx));
    prop = js_GetProperty(cx, obj,
			  (jsval)cx->runtime->atomState.lengthAtom,
			  &v);
    if (!prop || !ValueToLength(cx, v, lengthp))
	return NULL;
    return prop;
}

static JSProperty *
SetLengthProperty(JSContext *cx, JSObject *obj, jsint length)
{
    jsval v;

    PR_ASSERT(JS_IS_LOCKED(cx));
    v = INT_TO_JSVAL(length);
    return js_SetProperty(cx, obj,
			  (jsval)cx->runtime->atomState.lengthAtom,
			  &v);
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok;
    jsint newlen, oldlen, slot;
    jsval junk;

    JS_LOCK(cx);
    ok = ValueToLength(cx, *vp, &newlen);
    if (!ok)
	goto out;
    if (!GetLengthProperty(cx, obj, &oldlen)) {
	ok = JS_FALSE;
	goto out;
    }
    for (slot = newlen; slot < oldlen; slot++) {
	ok = js_DeleteProperty(cx, obj, INT_TO_JSVAL(slot), &junk);
	if (!ok)
	    break;
    }
out:
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
array_addProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint index, length;
    JSProperty *prop;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    index = JSVAL_TO_INT(id);
    JS_LOCK(cx);
    prop = GetLengthProperty(cx, obj, &length);
    if (prop && index >= length) {
	length = index + 1;
	if (prop->object == obj && prop->setter == array_length_setter)
	    obj->slots[prop->slot] = INT_TO_JSVAL(length);
	else
	    prop = SetLengthProperty(cx, obj, length);
    }
    JS_UNLOCK(cx);
    return (prop != NULL);
}

static JSBool
array_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint index, length;
    JSProperty *prop;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    index = JSVAL_TO_INT(id);
    JS_LOCK(cx);
    prop = GetLengthProperty(cx, obj, &length);
    if (prop && index == length - 1) {
	length = index;
	if (prop->object == obj && prop->setter == array_length_setter)
	    obj->slots[prop->slot] = INT_TO_JSVAL(length);
	else
	    prop = SetLengthProperty(cx, obj, length);
    }
    JS_UNLOCK(cx);
    return (prop != NULL);
}

static JSBool
array_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSProperty *prop;
    jsint length;

    JS_LOCK_VOID(cx, prop = GetLengthProperty(cx, obj, &length));
    if (!prop)
	return JS_FALSE;
    switch (type) {
      case JSTYPE_NUMBER:
	*vp = INT_TO_JSVAL(length);
	return JS_TRUE;
      case JSTYPE_BOOLEAN:
	*vp = BOOLEAN_TO_JSVAL(length > 0);
	return JS_TRUE;
      default:
	return JS_TRUE;
    }
}

JSClass js_ArrayClass = {
    "Array",
    0,
    array_addProperty, array_delProperty, JS_PropertyStub,   JS_PropertyStub,
    JS_EnumerateStub,  JS_ResolveStub,    array_convert,     JS_FinalizeStub
};

static JSBool
array_join_sub(JSContext *cx, JSObject *obj, JSString *sep, JSBool literalize,
	       jsval *rval)
{
    JSProperty *prop;
    jsval v;
    jsint length, index;
    jschar *chars;
    size_t nchars, growth, seplen;
    const jschar *sepstr;
    const char *quote;
    JSBool ok;
    JSString *str;
#if JS_HAS_SHARP_VARS
    PRHashEntry *he;
#else
    jsval lenval;
#endif

    PR_ASSERT(JS_IS_LOCKED(cx));
    prop = GetLengthProperty(cx, obj, &length);
    if (!prop)
	return JS_FALSE;
#if !JS_HAS_SHARP_VARS
    lenval = INT_TO_JSVAL(length);
#endif

    ok = JS_TRUE;
    if (literalize) {
#if JS_HAS_SHARP_VARS
	he = js_EnterSharpObject(cx, obj, &chars);
	if (!he)
	    return JS_FALSE;
	if (IS_SHARP(he)) {
	    nchars = js_strlen(chars);
	    goto make_string;
	}
	MAKE_SHARP(he);
#else
	chars = NULL;
#endif
	/*
	 * Allocate 1 + 3 + 1 for "[", the worst-case closing ", ]", and the
	 * terminating 0.
	 */
	growth = (1 + 3 + 1) * sizeof(jschar);
	if (!chars) {
	    nchars = 0;
	    chars = malloc(growth);
	} else {
	    nchars = js_strlen(chars);
	    chars = realloc(chars, nchars * sizeof(jschar) + growth);
	}
	if (!chars)
	    goto done;
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

	/* If v is a string, it needs to be quoted and escaped. */
	quote = NULL;
	if (JSVAL_IS_VOID(v)) {
	    str = cx->runtime->emptyString;
	} else {
#if !JS_HAS_SHARP_VARS
	    /* Avoid recursive divergence by setting length to 0. */
	    obj->slots[prop->slot] = JSVAL_ZERO;
#endif
	    str = js_ValueToString(cx, v);
#if !JS_HAS_SHARP_VARS
	    obj->slots[prop->slot] = lenval;
#endif
	    if (!str) {
		ok = JS_FALSE;
		goto done;
	    }
	    if (literalize && JSVAL_IS_STRING(v)) {
		quote = "\"";
		str = js_EscapeString(cx, str, *quote);
		if (!str) {
		    ok = JS_FALSE;
		    goto done;
		}
	    }
	}

	/* Allocate 3 + 1 at end for ", ", closing bracket, and zero. */
	growth = (nchars + (sepstr ? seplen : 0) +
		  (quote ? 2 : 0) + str->length +
		  3 + 1) * sizeof(jschar);
	chars = chars ? realloc(chars, growth) : malloc(growth);
	if (!chars)
	    goto done;

	if (sepstr) {
	    js_strncpy(&chars[nchars], sepstr, seplen);
	    nchars += seplen;
	}
	sepstr = sep->chars;

	if (quote)
	    chars[nchars++] = *quote;
	js_strncpy(&chars[nchars], str->chars, str->length);
	nchars += str->length;
	if (quote)
	    chars[nchars++] = *quote;
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
#if JS_HAS_SHARP_VARS
	js_LeaveSharpObject(cx);
#endif
    }
    if (!ok) {
	if (chars)
	    free(chars);
	return ok;
    }

#if JS_HAS_SHARP_VARS
  make_string:
#endif
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

static JSBool
array_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	       jsval *rval)
{
    JSBool literalize;

    /*
     * JS1.2 arrays convert to array literals, with a comma followed by a space
     * between each element.
     */
    literalize = (cx->version >= JSVERSION_1_2);
    JS_LOCK_AND_RETURN_BOOL(cx,
	array_join_sub(cx, obj,
		       literalize ? &comma_space : &comma,
		       literalize, rval));
}

static JSBool
array_join(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    JSBool ok;

    JS_LOCK(cx);
    if (argc == 0) {
	ok = array_join_sub(cx, obj, &comma, JS_FALSE, rval);
    } else {
	str = js_ValueToString(cx, argv[0]);
	if (!str) {
	    ok = JS_FALSE;
	} else {
	    argv[0] = STRING_TO_JSVAL(str);
	    ok = array_join_sub(cx, obj, str, JS_FALSE, rval);
	}
    }
    JS_UNLOCK(cx);
    return ok;
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
InitArrayObject(JSContext *cx, JSObject *obj, jsint length, jsval *vector)
{
    jsint index;

    PR_ASSERT(JS_IS_LOCKED(cx));
    if (!js_DefineProperty(cx, obj,
			   (jsval)cx->runtime->atomState.lengthAtom,
			   INT_TO_JSVAL(length),
			   JS_PropertyStub, array_length_setter,
			   JSPROP_PERMANENT)) {
	return JS_FALSE;
    }
    if (!vector)
	return JS_TRUE;
    for (index = 0; index < length; index++) {
	if (!js_DefineProperty(cx, obj, INT_TO_JSVAL(index), vector[index],
			       JS_PropertyStub, JS_PropertyStub,
			       JSPROP_ENUMERATE)) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
array_reverse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    jsint len, i;
    jsval *vec, *vp;
    JSBool ok;
    JSProperty *prop;

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &len) ||
	!(vec = JS_malloc(cx, (size_t) len * sizeof *vec))) {
	ok = JS_FALSE;
	goto out;
    }

    for (i = 0; i < len; i++)
	vec[i] = JSVAL_VOID;
    for (prop = obj->map->props; prop; prop = prop->next) {
	if (!(prop->flags & JSPROP_TINYIDHACK) &&
	    JSVAL_IS_INT(prop->id) &&
	    (jsuint)(i = JSVAL_TO_INT(prop->id)) < (jsuint)len) {
	    vp = &vec[len - i - 1];
	    *vp = prop->object->slots[prop->slot];
	}
    }

    ok = InitArrayObject(cx, obj, len, vec);
    if (ok)
	*rval = OBJECT_TO_JSVAL(obj);
    JS_free(cx, vec);
out:
    JS_UNLOCK(cx);
    return ok;
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
    const jsval *avp = a, *bvp = b;
    CompareArgs *ca = arg;
    JSContext *cx = ca->context;
    jsdouble cmp = -1;
    jsval fval, argv[2], rval;
    JSBool ok;

    fval = ca->fval;
    if (fval == JSVAL_NULL) {
	JSString *astr, *bstr;

	if (*avp == *bvp) {
	    cmp = 0;
	} else if (*avp == JSVAL_VOID || *bvp == JSVAL_VOID) {
	    /* Put undefined properties at the end. */
	    cmp = (*avp == JSVAL_VOID) ? 1 : -1;
	} else if ((astr = js_ValueToString(cx, *avp)) &&
		   (bstr = js_ValueToString(cx, *bvp))) {
	    cmp = js_CompareStrings(astr, bstr);
	} else {
	    ca->status = JS_FALSE;
	}
    } else {
	argv[0] = *avp;
	argv[1] = *bvp;
	ok = js_Call(cx, OBJ_GET_PARENT(JSVAL_TO_OBJECT(fval)), fval, 2, argv,
		     &rval);
	if (ok)
	    ok = js_ValueToNumber(cx, rval, &cmp);
	if (!ok)
	    ca->status = ok;
    }
    return (int)cmp;
}

static JSBool
array_sort(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval;
    CompareArgs ca;
    jsint len, i;
    jsval *vec;
    JSProperty *prop;

    if (argc > 0) {
	if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_FUNCTION) {
	    /* XXX JS_ConvertValue shouldn't convert closure */
	    if (!JS_ConvertValue(cx, argv[0], JSTYPE_FUNCTION, &argv[0]))
		return JS_FALSE;
	}
	fval = argv[0];
    } else {
	fval = JSVAL_NULL;
    }

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &len) ||
	!(vec = JS_malloc(cx, (size_t) len * sizeof *vec))) {
	ca.status = JS_FALSE;
	goto out;
    }

    for (i = 0; i < len; i++)
	vec[i] = JSVAL_VOID;
    for (prop = obj->map->props; prop; prop = prop->next) {
	if (!(prop->flags & JSPROP_TINYIDHACK) &&
	    JSVAL_IS_INT(prop->id) &&
	    (jsuint)(i = JSVAL_TO_INT(prop->id)) < (jsuint)len) {
	    vec[i] = prop->object->slots[prop->slot];
	}
    }

    ca.context = cx;
    ca.fval = fval;
    ca.status = JS_TRUE;
    if (!js_qsort(vec, (size_t) len, (size_t) sizeof *vec, sort_compare, &ca)) {
	JS_ReportOutOfMemory(cx);
	ca.status = JS_FALSE;
    }

    if (ca.status) {
	ca.status = InitArrayObject(cx, obj, len, vec);
	if (ca.status)
	    *rval = OBJECT_TO_JSVAL(obj);
    }
    JS_free(cx, vec);
out:
    JS_UNLOCK(cx);
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
    jsint length;
    uintN i;
    jsval id;
    JSBool ok;

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length))
	return JS_FALSE;
    for (i = 0; i < argc; i++) {
	id = INT_TO_JSVAL(length + i);
	if (!js_SetProperty(cx, obj, id, &argv[i])) {
	    ok = JS_FALSE;
	    goto out;
	}
    }

    /*
     * If JS1.2, follow Perl4 by returning the last thing pushed.  Otherwise,
     * return the new array length.
     */
    length += argc;
    *rval = (cx->version == JSVERSION_1_2)
	    ? (argc ? argv[argc-1] : JSVAL_VOID)
	    : INT_TO_JSVAL(length);
    ok = SetLengthProperty(cx, obj, length) != NULL;
out:
    JS_UNLOCK(cx);
    return ok;
#else
    return array_nyi(cx, "push");
#endif
}

static JSBool
array_pop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsint index;
    JSBool ok;
    jsval id, junk;
    JSProperty *prop;

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &index)) {
	ok = JS_FALSE;
    } else if (index == 0) {
	ok = JS_TRUE;
    } else {
	index--;
	id = INT_TO_JSVAL(index);

	/* Get the to-be-deleted property's value into rval ASAP. */
	prop = js_GetProperty(cx, obj, id, rval);
	if (!prop) {
	    ok = JS_FALSE;
	    goto out;
	}

	ok = js_DeleteProperty2(cx, obj, prop, id, &junk);
	if (!ok)
	    goto out;
	if (!SetLengthProperty(cx, obj, index))
	    ok = JS_FALSE;
    }
out:
    JS_UNLOCK(cx);
    return ok;
#else
    return array_nyi(cx, "pop");
#endif
}

static JSBool
array_shift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsint length, i;
    JSBool ok;
    jsval id, id2, v, junk;
    JSProperty *prop;

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length)) {
	ok = JS_FALSE;
    } else if (length == 0) {
	ok = JS_TRUE;
    } else {
	length--;
	id = JSVAL_ZERO;

	/* Get the to-be-deleted property's value into rval ASAP. */
	prop = js_GetProperty(cx, obj, id, rval);
	if (!prop) {
	    ok = JS_FALSE;
	    goto out;
	}

	/*
	 * Slide down the array above the first element.  Leave prop and
	 * id set to point to the last element.
	 */
	if (length > 0) {
	    for (i = 1; i <= length; i++) {
		id = INT_TO_JSVAL(i);
		id2 = INT_TO_JSVAL(i - 1);
		prop = js_GetProperty(cx, obj, id, &v);
		if (!prop || !js_SetProperty(cx, obj, id2, &v)) {
		    ok = JS_FALSE;
		    goto out;
		}
	    }
	}

	/* Delete the only or last element. */
	ok = js_DeleteProperty2(cx, obj, prop, id, &junk);
	if (!ok)
	    goto out;
	if (!SetLengthProperty(cx, obj, length))
	    ok = JS_FALSE;
    }
out:
    JS_UNLOCK(cx);
    return ok;
#else
    return array_nyi(cx, "shift");
#endif
}

static JSBool
array_unshift(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsint length, last;
    uintN i;
    jsval id, id2, v;
    JSProperty *prop;
    JSBool ok;

    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length)) {
	ok = JS_FALSE;
	goto out;
    }
    if (argc > 0) {
	/* Slide up the array to make room for argc at the bottom. */
	if (length > 0) {
	    for (last = length - 1; last >= 0; last--) {
		id = INT_TO_JSVAL(last);
		id2 = INT_TO_JSVAL(last + argc);
		prop = js_GetProperty(cx, obj, id, &v);
		if (!prop || !js_SetProperty(cx, obj, id2, &v)) {
		    ok = JS_FALSE;
		    goto out;
		}
	    }
	}

	/* Copy from argv to the bottom of the array. */
	for (i = 0; i < argc; i++) {
	    id = INT_TO_JSVAL(i);
	    if (!js_SetProperty(cx, obj, id, &argv[i])) {
		ok = JS_FALSE;
		goto out;
	    }
	}

	/* Follow Perl by returning the new array length. */
	length += argc;
	if (!SetLengthProperty(cx, obj, length)) {
	    ok = JS_FALSE;
	    goto out;
	}
    }
    *rval = INT_TO_JSVAL(length);
    ok = JS_TRUE;
out:
    JS_UNLOCK(cx);
    return ok;
#else
    return array_nyi(cx, "unshift");
#endif
}

static JSBool
array_splice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#if JS_HAS_MORE_PERL_FUN
    jsint length, begin, end, count, delta, last;
    uintN i;
    JSBool ok;
    jsdouble d;
    jsval id, id2, v;
    JSObject *obj2;
    JSProperty *prop;

    /* Nothing to do if no args.  Otherwise lock and load length. */
    if (argc == 0)
	return JS_TRUE;
    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length)) {
	ok = JS_FALSE;
	goto out;
    }

    /* Convert the first argument into a starting index. */
    ok = js_ValueToNumber(cx, *argv, &d);
    if (!ok)
	goto out;
    begin = (jsint)d;
    if (begin < 0) {
	begin += length;
	if (begin < 0)
	    begin = 0;
    } else if (begin > length) {
	begin = length;
    }
    argc--;
    argv++;

    /* Convert the second argument from a count into a fencepost index. */
    delta = length - begin;
    if (argc == 0) {
	count = delta;
	end = length;
    } else {
	ok = js_ValueToNumber(cx, *argv, &d);
	if (!ok)
	    goto out;
	count = (jsint)d;
	if (count < 0)
	    count = 0;
	else if (count > delta)
	    count = delta;
	end = begin + count;
	argc--;
	argv++;
    }

    /* If there are elements to remove, put them into the return value. */
    if (count > 0) {
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
	    id = INT_TO_JSVAL(begin);
	    if (!js_GetProperty(cx, obj, id, rval)) {
		ok = JS_FALSE;
		goto out;
	    }
	} else {
	    obj2 = js_NewArrayObject(cx, 0, NULL);
	    if (!obj2) {
		ok = JS_FALSE;
		goto out;
	    }
	    *rval = OBJECT_TO_JSVAL(obj2);
	    for (last = begin; last < end; last++) {
		id = INT_TO_JSVAL(last);
		id2 = INT_TO_JSVAL(last - begin);
		if (!js_GetProperty(cx, obj, id, &v) ||
		    !js_SetProperty(cx, obj2, id2, &v)) {
		    ok = JS_FALSE;
		    goto out;
		}
	    }
	}
    }

    /* Find the direction (up or down) to copy and make way for argv. */
    delta = (jsint)argc - count;
    if (delta > 0) {
	for (last = length - 1; last >= end; last--) {
	    id = INT_TO_JSVAL(last);
	    id2 = INT_TO_JSVAL(last + delta);
	    prop = js_GetProperty(cx, obj, id, &v);
	    if (!prop || !js_SetProperty(cx, obj, id2, &v)) {
		ok = JS_FALSE;
		goto out;
	    }
	}
    } else if (delta < 0) {
	for (last = end; last < length; last++) {
	    id = INT_TO_JSVAL(last);
	    id2 = INT_TO_JSVAL(last + delta);
	    prop = js_GetProperty(cx, obj, id, &v);
	    if (!prop || !js_SetProperty(cx, obj, id2, &v)) {
		ok = JS_FALSE;
		goto out;
	    }
	}
    }

    /* Copy from argv into the hole to complete the splice. */
    for (i = 0; i < argc; i++) {
	id = INT_TO_JSVAL(begin + i);
	if (!js_SetProperty(cx, obj, id, &argv[i])) {
	    ok = JS_FALSE;
	    goto out;
	}
    }

    /* Update length in case we deleted elements from the end. */
    if (!SetLengthProperty(cx, obj, length + delta))
	ok = JS_FALSE;
out:
    JS_UNLOCK(cx);
    return ok;
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
    JSBool ok;
    jsint slot, length, alength;
    jsval v, id, id2, lv, *vp;
    JSProperty *prop;
    uintN i;

    nobj = JS_NewArrayObject(cx, 0, NULL);
    if (!nobj)
	return JS_FALSE;

    ok = JS_TRUE;
    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length)) {
	ok = JS_FALSE;
	goto out;
    }
    for (slot = 0; slot < length; slot++) {
	id = INT_TO_JSVAL(slot);
	prop = js_GetProperty(cx, obj, id, &v);
	if (!prop || !js_SetProperty(cx, nobj, id, &v)) {
	    ok = JS_FALSE;
	    goto out;
	}
    }

    for (i = 0; i < argc; i++) {
	v = argv[i];
	if (JSVAL_IS_OBJECT(v)) {
	    aobj = JSVAL_TO_OBJECT(v);
	    if (aobj && (prop = js_HasLengthProperty(cx, aobj))) {
		vp = &aobj->slots[prop->slot];
		lv = *vp;
		ok = prop->getter(cx, aobj,
				  (jsval)cx->runtime->atomState.lengthAtom,
				  &lv);
		if (!ok)
		    goto out;
		*vp = lv;
		ok = ValueToLength(cx, lv, &alength);
		if (!ok)
		    goto out;
		for (slot = 0; slot < alength; slot++) {
		    id  = INT_TO_JSVAL(slot);
		    id2 = INT_TO_JSVAL(length + slot);
		    prop = js_GetProperty(cx, aobj, id, &v);
		    if (!prop || !js_SetProperty(cx, nobj, id2, &v)) {
			ok = JS_FALSE;
			goto out;
		    }
		}
		length += alength;
		continue;
	    }
	}

	id = INT_TO_JSVAL(length);
	if (!js_SetProperty(cx, nobj, id, &v)) {
	    ok = JS_FALSE;
	    goto out;
	}
	length++;
    }

    *rval = OBJECT_TO_JSVAL(nobj);
out:
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
array_slice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *nobj;
    JSBool ok;
    jsint length, begin, end, slot;
    jsdouble d;
    jsval v, id, id2;
    JSProperty *prop;

    nobj = JS_NewArrayObject(cx, 0, NULL);
    if (!nobj)
	return JS_FALSE;

    ok = JS_TRUE;
    JS_LOCK(cx);
    if (!GetLengthProperty(cx, obj, &length)) {
	ok = JS_FALSE;
	goto out;
    }
    begin = 0;
    end = length;

    if (argc > 0) {
	ok = js_ValueToNumber(cx, argv[0], &d);
	if (!ok)
	    goto out;
	begin = (jsint)d;
	if (begin < 0) {
	    begin += length;
	    if (begin < 0)
		begin = 0;
	} else if (begin > length) {
	    begin = length;
	}

	if (argc > 1) {
	    ok = js_ValueToNumber(cx, argv[1], &d);
	    if (!ok)
		goto out;
	    end = (jsint)d;
	    if (end < 0) {
		end += length;
		if (end < 0)
		    end = 0;
	    } else if (end > length) {
		end = length;
	    }
	}
    }

    for (slot = begin; slot < end; slot++) {
	id  = INT_TO_JSVAL(slot);
	id2 = INT_TO_JSVAL(slot - begin);
	prop = js_GetProperty(cx, obj, id, &v);
	if (!prop || !js_SetProperty(cx, nobj, id2, &v)) {
	    ok = JS_FALSE;
	    goto out;
	}
    }
    *rval = OBJECT_TO_JSVAL(nobj);
out:
    JS_UNLOCK(cx);
    return ok;
}
#endif /* JS_HAS_SEQUENCE_OPS */

static JSFunctionSpec array_methods[] = {
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
    jsint length;
    jsval *vector;
    JSBool ok;

    if (argc == 0) {
	length = 0;
	vector = NULL;
    } else if (cx->version < JSVERSION_1_2 &&
	       argc == 1 && JSVAL_IS_INT(argv[0])) {
	length = JSVAL_TO_INT(argv[0]);
	vector = NULL;
    } else {
	length = argc;
	vector = argv;
    }
    JS_LOCK_VOID(cx, ok = InitArrayObject(cx, obj, length, vector));
    if (ok)
	*rval = OBJECT_TO_JSVAL(obj);
    return ok;
}

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    return JS_InitClass(cx, obj, NULL, &js_ArrayClass, Array, 1,
			NULL, array_methods, NULL, NULL);
}

JSObject *
js_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
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

JSBool
js_GetArrayLength(JSContext *cx, JSObject *obj, jsint *lengthp)
{
    return GetLengthProperty(cx, obj, lengthp) != NULL;
}

JSBool
js_SetArrayLength(JSContext *cx, JSObject *obj, jsint length)
{
    return SetLengthProperty(cx, obj, length) != NULL;
}

JSProperty *
js_HasLengthProperty(JSContext *cx, JSObject *obj)
{
    JSErrorReporter older;
    JSObject *pobj;
    JSProperty *prop;
    JSBool ok;

    PR_ASSERT(JS_IS_LOCKED(cx));
    older = JS_SetErrorReporter(cx, NULL);
    pobj = NULL;
    ok = js_LookupProperty(cx, obj, (jsval)cx->runtime->atomState.lengthAtom,
			   &pobj, &prop);
    JS_SetErrorReporter(cx, older);
    return (ok && prop && prop->object == obj) ? prop : NULL;
}
