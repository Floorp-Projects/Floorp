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
 * JS atom table.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prhash.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsstr.h"

/*
 * Keep this in sync with jspubtd.h -- an assertion below will insist that
 * its length match the JSType enum's JSTYPE_LIMIT limit value.
 */
char *js_type_str[] = {
    "undefined",
    "object",
    "function",
    "string",
    "number",
    "boolean",
};

char *js_boolean_str[] = {
    js_false_str,
    js_true_str
};

char   js_Array_str[]             = "Array";
char   js_Math_str[]              = "Math";
char   js_Object_str[]            = "Object";
char   js_anonymous_str[]         = "anonymous";
char   js_arguments_str[]         = "arguments";
char   js_arity_str[]             = "arity";
char   js_assign_str[]            = "assign";
char   js_callee_str[]            = "callee";
char   js_caller_str[]            = "caller";
char   js_class_prototype_str[]   = "prototype";
char   js_constructor_str[]       = "constructor";
char   js_count_str[]             = "__count__";
char   js_eval_str[]              = "eval";
char   js_index_str[]             = "index";
char   js_input_str[]             = "input";
char   js_length_str[]            = "length";
char   js_name_str[]              = "name";
char   js_parent_str[]            = "__parent__";
char   js_proto_str[]             = "__proto__";
char   js_toSource_str[]          = "toSource";
char   js_toString_str[]          = "toString";
char   js_valueOf_str[]           = "valueOf";

#define HASH_OBJECT(o)  ((prhashcode)(o) >> JSVAL_TAGBITS)
#define HASH_INT(i)     ((prhashcode)(i))
#define HASH_DOUBLE(dp) ((prhashcode)(((uint32*)(dp))[0] ^ ((uint32*)(dp))[1]))
#define HASH_BOOLEAN(b) ((prhashcode)(b))

PR_STATIC_CALLBACK(prhashcode)
js_hash_atom_key(const void *key)
{
    jsval v;
    jsdouble *dp;

    /* Order JSVAL_IS_* tests by likelihood of success. */
    v = (jsval)key;
    if (JSVAL_IS_STRING(v))
	return js_HashString(JSVAL_TO_STRING(v));
    if (JSVAL_IS_INT(v))
	return HASH_INT(JSVAL_TO_INT(v));
    if (JSVAL_IS_DOUBLE(v)) {
	dp = JSVAL_TO_DOUBLE(v);
	return HASH_DOUBLE(dp);
    }
    if (JSVAL_IS_OBJECT(v))
	return HASH_OBJECT(JSVAL_TO_OBJECT(v));
    if (JSVAL_IS_BOOLEAN(v))
	return HASH_BOOLEAN(JSVAL_TO_BOOLEAN(v));
    return (prhashcode)v;
}

PR_STATIC_CALLBACK(intN)
js_compare_atom_keys(const void *k1, const void *k2)
{
    jsval v1, v2;

    v1 = (jsval)k1, v2 = (jsval)k2;
    if (JSVAL_IS_STRING(v1) && JSVAL_IS_STRING(v2))
	return !js_CompareStrings(JSVAL_TO_STRING(v1), JSVAL_TO_STRING(v2));
    if (JSVAL_IS_DOUBLE(v1) && JSVAL_IS_DOUBLE(v2)) {
        double d1 = *JSVAL_TO_DOUBLE(v1);
        double d2 = *JSVAL_TO_DOUBLE(v2);
#ifdef XP_PC
	/* XXX MSVC miscompiles such that (NaN == 0) */
	if (JSDOUBLE_IS_NaN(d1))
	    return JSDOUBLE_IS_NaN(d2);
	else if (JSDOUBLE_IS_NaN(d2))
            return JS_FALSE;
#endif
	return d1 == d2;
    }
    return v1 == v2;
}

PR_STATIC_CALLBACK(int)
js_compare_stub(const void *v1, const void *v2)
{
    return 1;
}

PR_STATIC_CALLBACK(void *)
js_alloc_atom_space(void *priv, size_t size)
{
    return malloc(size);
}

PR_STATIC_CALLBACK(void)
js_free_atom_space(void *priv, void *item)
{
    free(item);
}

PR_STATIC_CALLBACK(PRHashEntry *)
js_alloc_atom(void *priv, const void *key)
{
    JSAtomState *state = priv;
    JSAtom *atom;

    atom = malloc(sizeof(JSAtom));
    if (!atom)
	return NULL;
#ifdef JS_THREADSAFE
    state->tablegen++;
#endif
    atom->entry.key = key;
    atom->entry.value = NULL;
    atom->flags = 0;
    atom->kwindex = -1;
    atom->number = state->number++;
    return &atom->entry;
}

PR_STATIC_CALLBACK(void)
js_free_atom(void *priv, PRHashEntry *he, uintN flag)
{
    if (flag != HT_FREE_ENTRY)
	return;
#ifdef JS_THREADSAFE
    ((JSAtomState *)priv)->tablegen++;
#endif
    free(he);
}

static PRHashAllocOps atom_alloc_ops = {
    js_alloc_atom_space,    js_free_atom_space,
    js_alloc_atom,          js_free_atom
};

#define JS_ATOM_HASH_SIZE   1024

JSBool
js_InitAtomState(JSContext *cx, JSAtomState *state)
{
    uintN i;

    state->runtime = cx->runtime;
    state->number = 0;
    state->table = PR_NewHashTable(JS_ATOM_HASH_SIZE, js_hash_atom_key,
				   js_compare_atom_keys, js_compare_stub,
				   &atom_alloc_ops, state);
    if (!state->table) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }
#ifdef JS_THREADSAFE
    js_NewLock(&state->lock);
    state->tablegen = 0;
#endif

#define FROB(lval,str) {                                                      \
    if (!(state->lval = js_Atomize(cx, str, strlen(str), ATOM_PINNED))) {     \
	js_FreeAtomState(cx, state);                                          \
	return JS_FALSE;                                                      \
    }                                                                         \
}

    PR_ASSERT(sizeof js_type_str / sizeof js_type_str[0] == JSTYPE_LIMIT);
    for (i = 0; i < JSTYPE_LIMIT; i++)
	FROB(typeAtoms[i],        js_type_str[i]);

    FROB(booleanAtoms[0],         js_false_str);
    FROB(booleanAtoms[1],         js_true_str);
    FROB(nullAtom,                js_null_str);

    FROB(ArrayAtom,               js_Array_str);
    FROB(MathAtom,                js_Math_str);
    FROB(ObjectAtom,              js_Object_str);
    FROB(anonymousAtom,           js_anonymous_str);
    FROB(argumentsAtom,           js_arguments_str);
    FROB(arityAtom,               js_arity_str);
    FROB(assignAtom,              js_assign_str);
    FROB(calleeAtom,              js_callee_str);
    FROB(callerAtom,              js_caller_str);
    FROB(classPrototypeAtom,      js_class_prototype_str);
    FROB(constructorAtom,         js_constructor_str);
    FROB(countAtom,               js_count_str);
    FROB(indexAtom,               js_index_str);
    FROB(inputAtom,               js_input_str);
    FROB(lengthAtom,              js_length_str);
    FROB(nameAtom,                js_name_str);
    FROB(parentAtom,              js_parent_str);
    FROB(protoAtom,               js_proto_str);
    FROB(toSourceAtom,            js_toSource_str);
    FROB(toStringAtom,            js_toString_str);
    FROB(valueOfAtom,             js_valueOf_str);

#undef FROB

    return JS_TRUE;
}

void
js_FreeAtomState(JSContext *cx, JSAtomState *state)
{
    state->runtime = NULL;
    PR_HashTableDestroy(state->table);
    state->table = NULL;
    state->number = 0;
#ifdef JS_THREADSAFE
    js_DestroyLock(&state->lock);
#endif
}

typedef struct MarkArgs {
    JSRuntime       *runtime;
    JSGCThingMarker mark;
} MarkArgs;

PR_STATIC_CALLBACK(intN)
js_atom_marker(PRHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;
    jsval key;
    MarkArgs *args;

    atom = (JSAtom *)he;
    if (atom->flags & ATOM_PINNED) {
	atom->flags |= ATOM_MARK;
	key = ATOM_KEY(atom);
	if (JSVAL_IS_GCTHING(key)) {
	    args = arg;
	    args->mark(args->runtime, JSVAL_TO_GCTHING(key));
	}
    }
    return HT_ENUMERATE_NEXT;
}

void
js_MarkAtomState(JSAtomState *state, JSGCThingMarker mark)
{
    MarkArgs args;

    args.runtime = state->runtime;
    args.mark = mark;
    PR_HashTableEnumerateEntries(state->table, js_atom_marker, &args);
}

PR_STATIC_CALLBACK(intN)
js_atom_sweeper(PRHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;

    atom = (JSAtom *)he;
    if (atom->flags & ATOM_MARK) {
	atom->flags &= ~ATOM_MARK;
	return HT_ENUMERATE_NEXT;
    }
    PR_ASSERT((atom->flags & ATOM_PINNED) == 0);
    atom->entry.key = NULL;
    atom->flags = 0;
    return HT_ENUMERATE_REMOVE;
}

void
js_SweepAtomState(JSAtomState *state)
{
    PR_HashTableEnumerateEntries(state->table, js_atom_sweeper, NULL);
}

PR_STATIC_CALLBACK(intN)
js_atom_unpinner(PRHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;

    atom = (JSAtom *)he;
    atom->flags &= ~ATOM_PINNED;
    return HT_ENUMERATE_NEXT;
}

void
js_UnpinPinnedAtoms(JSAtomState *state)
{
    PR_HashTableEnumerateEntries(state->table, js_atom_unpinner, NULL);
}

static JSAtom *
js_AtomizeHashedKey(JSContext *cx, jsval key, prhashcode keyHash, uintN flags)
{
    JSAtomState *state;
    PRHashTable *table;
    PRHashEntry *he, **hep;
    JSAtom *atom;

    state = &cx->runtime->atomState;
    JS_LOCK(&state->lock,cx);
    table = state->table;
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    atom = NULL;
	    goto out;
	}
    }

    atom = (JSAtom *)he;
    atom->flags |= flags;
out:
    JS_UNLOCK(&state->lock,cx);
    return atom;
}

JSAtom *
js_AtomizeObject(JSContext *cx, JSObject *obj, uintN flags)
{
    jsval key;
    prhashcode keyHash;

    /* XXX must be set in the following order or MSVC1.52 will crash */
    keyHash = HASH_OBJECT(obj);
    key = OBJECT_TO_JSVAL(obj);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeBoolean(JSContext *cx, JSBool b, uintN flags)
{
    jsval key;
    prhashcode keyHash;

    key = BOOLEAN_TO_JSVAL(b);
    keyHash = HASH_BOOLEAN(b);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeInt(JSContext *cx, jsint i, uintN flags)
{
    jsval key;
    prhashcode keyHash;

    key = INT_TO_JSVAL(i);
    keyHash = HASH_INT(i);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeDouble(JSContext *cx, jsdouble d, uintN flags)
{
    jsdouble *dp;
    prhashcode keyHash;
    jsval key;
    JSAtomState *state;
    PRHashTable *table;
    PRHashEntry *he, **hep;
    JSAtom *atom;

#if PR_ALIGN_OF_DOUBLE == 8
    dp = &d;
#else
    char alignbuf[16];

    dp = (jsdouble *)&alignbuf[8 - ((pruword)&alignbuf & 7)];
    *dp = d;
#endif

    keyHash = HASH_DOUBLE(dp);
    key = DOUBLE_TO_JSVAL(dp);
    state = &cx->runtime->atomState;
    JS_LOCK(&state->lock,cx);
    table = state->table;
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
#ifdef JS_THREADSAFE
	uint32 gen = state->tablegen;
#endif
	JS_UNLOCK(&state->lock,cx);
	if (!js_NewDoubleValue(cx, d, &key))
	    return NULL;
	JS_LOCK(&state->lock,cx);
#ifdef JS_THREADSAFE
	if (state->tablegen != gen) {
	    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
	    if ((he = *hep) != NULL) {
	    	atom = (JSAtom *)he;
	    	goto out;
	    }
	}
#endif
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    atom = NULL;
	    goto out;
	}
    }

    atom = (JSAtom *)he;
    atom->flags |= flags;
out:
    JS_UNLOCK(&state->lock,cx);
    return atom;
}

JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, uintN flags)
{
    prhashcode keyHash;
    jsval key;
    JSAtomState *state;
    PRHashTable *table;
    PRHashEntry *he, **hep;
    JSAtom *atom;

    keyHash = js_HashString(str);
    key = STRING_TO_JSVAL(str);
    state = &cx->runtime->atomState;
    JS_LOCK(&state->lock,cx);
    table = state->table;
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
	if (flags & ATOM_TMPSTR) {
#ifdef JS_THREADSAFE
	    uint32 gen = state->tablegen;
#endif
	    JS_UNLOCK(&state->lock,cx);
	    flags &= ~ATOM_TMPSTR;
	    if (flags & ATOM_NOCOPY) {
		flags &= ~ATOM_NOCOPY;
		str = js_NewString(cx, str->chars, str->length, 0);
	    } else {
		str = js_NewStringCopyN(cx, str->chars, str->length, 0);
	    }
	    if (!str)
		return NULL;
	    key = STRING_TO_JSVAL(str);
	    JS_LOCK(&state->lock,cx);
#ifdef JS_THREADSAFE
	    if (state->tablegen != gen) {
		hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
		if ((he = *hep) != NULL) {
		    atom = (JSAtom *)he;
		    goto out;
		}
	    }
#endif
	}
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    atom = NULL;
	    goto out;
	}
    }

    atom = (JSAtom *)he;
    atom->flags |= flags;
out:
    JS_UNLOCK(&state->lock,cx);
    return atom;
}

JS_FRIEND_API(JSAtom *)
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags)
{
    jschar *chars;
    JSString *str;
    JSAtom *atom;
#if PR_ALIGN_OF_DOUBLE == 8
    union { jsdouble d; JSString s; } u;

    str = &u.s;
#else
    char alignbuf[16];

    str = (JSString *)&alignbuf[8 - ((pruword)&alignbuf & 7)];
#endif

    chars = js_InflateString(cx, bytes, length);
    if (!chars)
	return NULL;
    str->chars = chars;
    str->length = length;
    atom = js_AtomizeString(cx, str, ATOM_TMPSTR | ATOM_NOCOPY | flags);
    if (!atom || ATOM_TO_STRING(atom)->chars != chars)
	JS_free(cx, chars);
    return atom;
}

JS_FRIEND_API(JSAtom *)
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length, uintN flags)
{
    JSString *str;
#if PR_ALIGN_OF_DOUBLE == 8
    union { jsdouble d; JSString s; } u;

    str = &u.s;
#else
    char alignbuf[16];

    str = (JSString *)&alignbuf[8 - ((pruword)&alignbuf & 7)];
#endif

    str->chars = (jschar *)chars;
    str->length = length;
    return js_AtomizeString(cx, str, ATOM_TMPSTR | flags);
}

JSAtom *
js_AtomizeValue(JSContext *cx, jsval value, uintN flags)
{
    if (JSVAL_IS_STRING(value))
	return js_AtomizeString(cx, JSVAL_TO_STRING(value), flags);
    if (JSVAL_IS_INT(value))
	return js_AtomizeInt(cx, JSVAL_TO_INT(value), flags);
    if (JSVAL_IS_DOUBLE(value))
	return js_AtomizeDouble(cx, *JSVAL_TO_DOUBLE(value), flags);
    if (JSVAL_IS_OBJECT(value))
	return js_AtomizeObject(cx, JSVAL_TO_OBJECT(value), flags);
    if (JSVAL_IS_BOOLEAN(value))
	return js_AtomizeBoolean(cx, JSVAL_TO_BOOLEAN(value), flags);
    return js_AtomizeHashedKey(cx, value, (prhashcode)value, flags);
}

JSAtom *
js_ValueToStringAtom(JSContext *cx, jsval v)
{
    JSString *str;

    str = js_ValueToString(cx, v);
    if (!str)
	return NULL;
    return js_AtomizeString(cx, str, 0);
}

JSAtomListElement *
js_IndexAtom(JSContext *cx, JSAtom *atom, JSAtomList *al)
{
    JSAtomListElement *ale;

    ATOM_LIST_SEARCH(ale, al, atom);
    if (!ale) {
	PR_ARENA_ALLOCATE(ale, &cx->tempPool, sizeof(JSAtomListElement));
	if (!ale) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
	ale->atom = atom;
	ale->index = (jsatomid) al->count++;
	ale->next = al->list;
	al->list = ale;
    }
    return ale;
}

JS_FRIEND_API(JSAtom *)
js_GetAtom(JSContext *cx, JSAtomMap *map, jsatomid i)
{
    JSAtom *atom;

    PR_ASSERT(map->vector && i < map->length);
    if (!map->vector || i >= map->length) {
	JS_ReportError(cx, "internal error: no index for atom %ld", (long)i);
	return NULL;
    }
    atom = map->vector[i];
    PR_ASSERT(atom);
    return atom;
}

JS_FRIEND_API(JSBool)
js_InitAtomMap(JSContext *cx, JSAtomMap *map, JSAtomList *al)
{
    JSAtom **vector;
    JSAtomListElement *ale, *next;
    uint32 count;

    ale = al->list;
    if (!ale) {
	map->vector = NULL;
	map->length = 0;
	return JS_TRUE;
    }

    count = al->count;
    if (count >= ATOM_INDEX_LIMIT) {
	JS_ReportError(cx, "too many literals");
	return JS_FALSE;
    }
    vector = JS_malloc(cx, (size_t) count * sizeof *vector);
    if (!vector)
	return JS_FALSE;

    do {
        vector[ale->index] = ale->atom;
	next = ale->next;
	ale->next = NULL;
    } while ((ale = next) != NULL);
    al->list = NULL;
    al->count = 0;

    map->vector = vector;
    map->length = (jsatomid)count;
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_FreeAtomMap(JSContext *cx, JSAtomMap *map)
{
    if (map->vector) {
	free(map->vector);
	map->vector = NULL;
    }
    map->length = 0;
}
