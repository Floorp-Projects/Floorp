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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#ifndef NSPR20
#include "prhash.h"
#else
#include "plhash.h"
#endif
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

char   js_Array_str[]             = "Array";
char   js_Object_str[]            = "Object";
char   js_anonymous_str[]         = "anonymous";
char   js_assign_str[]            = "assign";
char   js_class_prototype_str[]   = "prototype";
char   js_constructor_str[]       = "constructor";
char   js_count_str[]             = "__count__";
char   js_eval_str[]              = "eval";
char   js_index_str[]             = "index";
char   js_input_str[]             = "input";
char   js_length_str[]            = "length";
char   js_parent_str[]            = "__parent__";
char   js_proto_str[]             = "__proto__";
char   js_toString_str[]          = "toString";
char   js_valueOf_str[]           = "valueOf";

#define HASH_OBJECT(o)  ((PRHashNumber)(o) >> JSVAL_TAGBITS)
#define HASH_INT(i)     ((PRHashNumber)(i))
#define HASH_DOUBLE(dp) ((PRHashNumber)(((uint32*)(dp))[0] ^ ((uint32*)(dp))[1]))
#define HASH_BOOLEAN(b) ((PRHashNumber)(b))

PR_STATIC_CALLBACK(PRHashNumber)
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
    return (PRHashNumber)v;
}

PR_STATIC_CALLBACK(intN)
js_compare_atom_keys(const void *k1, const void *k2)
{
    jsval v1, v2;

    v1 = (jsval)k1, v2 = (jsval)k2;
    if (JSVAL_IS_STRING(v1) && JSVAL_IS_STRING(v2))
	return !js_CompareStrings(JSVAL_TO_STRING(v1), JSVAL_TO_STRING(v2));
    if (JSVAL_IS_DOUBLE(v1) && JSVAL_IS_DOUBLE(v2))
	return *JSVAL_TO_DOUBLE(v1) == *JSVAL_TO_DOUBLE(v2);
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
    atom->entry.key = key;
    atom->entry.value = NULL;
    atom->nrefs = 0;
    atom->flags = 0;
    atom->kwindex = -1;
    atom->index = 0;
    atom->number = state->number++;
    return &atom->entry;
}

PR_STATIC_CALLBACK(void)
js_free_atom(void *priv, PRHashEntry *he, uintN flag)
{
    if (flag == HT_FREE_ENTRY)
	free(he);
}

static PRHashAllocOps atomAllocOps = {
    js_alloc_atom_space,    js_free_atom_space,
    js_alloc_atom,          js_free_atom
};

#define JS_ATOM_HASH_SIZE   1024

JSBool
js_InitAtomState(JSContext *cx, JSAtomState *state)
{
    uintN i;

    state->number = 0;
    state->table = PR_NewHashTable(JS_ATOM_HASH_SIZE, js_hash_atom_key,
				   js_compare_atom_keys, js_compare_stub,
				   &atomAllocOps, state);
    if (!state->table) {
	JS_ReportOutOfMemory(cx);
	return JS_FALSE;
    }

#define FROB(lval,str) {                                                      \
    if (!(state->lval = js_Atomize(cx, str, strlen(str), 0))) {               \
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
    FROB(ObjectAtom,              js_Object_str);
    FROB(anonymousAtom,           js_anonymous_str);
    FROB(assignAtom,              js_assign_str);
    FROB(classPrototypeAtom,      js_class_prototype_str);
    FROB(constructorAtom,         js_constructor_str);
    FROB(countAtom,               js_count_str);
    FROB(indexAtom,               js_index_str);
    FROB(inputAtom,               js_input_str);
    FROB(lengthAtom,              js_length_str);
    FROB(parentAtom,              js_parent_str);
    FROB(protoAtom,               js_proto_str);
    FROB(toStringAtom,            js_toString_str);
    FROB(valueOfAtom,             js_valueOf_str);

#undef FROB

    return JS_TRUE;
}

PR_STATIC_CALLBACK(intN)
js_atom_key_zapper(PRHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;

    atom = (JSAtom *)he;
    if (atom->nrefs == 0)
	return HT_ENUMERATE_REMOVE;
    he->key = NULL;
    return HT_ENUMERATE_NEXT;
}

void
js_FreeAtomState(JSContext *cx, JSAtomState *state)
{
    PRHashTable *table;

    js_ForceGC(cx);
    table = state->table;
    PR_HashTableEnumerateEntries(table, js_atom_key_zapper, NULL);
    js_ForceGC(cx);
    PR_HashTableDestroy(table);
    state->table = NULL;
    state->number = 0;
}

typedef struct MarkAtomArgs {
    JSRuntime       *runtime;
    JSAtomMarker    mark;
} MarkAtomArgs;

PR_STATIC_CALLBACK(intN)
js_atom_key_marker(PRHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;
    jsval key;
    MarkAtomArgs *args;

    atom = (JSAtom *)he;
    if (atom->nrefs == 0) {
	/*
	 * Unreferenced atom, probably from the scanner atomizing a name,
	 * number, or string that the parser did not save in an atom map
	 * (because it was a syntax error, e.g.).
	 */
	return HT_ENUMERATE_REMOVE;
    }
    key = ATOM_KEY(atom);
    if (JSVAL_IS_GCTHING(key)) {
	args = arg;
	args->mark(args->runtime, JSVAL_TO_GCTHING(key));
    }
    return HT_ENUMERATE_NEXT;
}

void
js_MarkAtomState(JSRuntime *rt, JSAtomMarker mark)
{
    PRHashTable *table;
    MarkAtomArgs args;

    table = rt->atomState.table;
    if (!table)
	return;
    args.runtime = rt;
    args.mark = mark;
    PR_HashTableEnumerateEntries(table, js_atom_key_marker, &args);
}

static JSAtom *
js_AtomizeHashedKey(JSContext *cx, jsval key, PRHashNumber keyHash, uintN flags)
{
    PRHashTable *table;
    PRHashEntry *he, **hep;
    JSAtom *atom;

    PR_ASSERT(JS_IS_LOCKED(cx));
    table = cx->runtime->atomState.table;
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
    }

    atom = (JSAtom *)he;
#ifdef DEBUG_DUPLICATE_ATOMS
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    he = *hep;
    PR_ASSERT(atom == (JSAtom *)he);
#endif

    if (flags & ATOM_NOHOLD)
	return atom;
    return js_HoldAtom(cx, atom);
}

JSAtom *
js_AtomizeObject(JSContext *cx, JSObject *obj, uintN flags)
{
    jsval key;
    PRHashNumber keyHash;

    /* XXX must be set in the following order or MSVC1.52 will crash */
    keyHash = HASH_OBJECT(obj);
    key = OBJECT_TO_JSVAL(obj);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeBoolean(JSContext *cx, JSBool b, uintN flags)
{
    jsval key;
    PRHashNumber keyHash;

    key = BOOLEAN_TO_JSVAL(b);
    keyHash = HASH_BOOLEAN(b);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeInt(JSContext *cx, jsint i, uintN flags)
{
    jsval key;
    PRHashNumber keyHash;

    key = INT_TO_JSVAL(i);
    keyHash = HASH_INT(i);
    return js_AtomizeHashedKey(cx, key, keyHash, flags);
}

JSAtom *
js_AtomizeDouble(JSContext *cx, jsdouble d, uintN flags)
{
    jsdouble *dp;
    PRHashTable *table;
    PRHashNumber keyHash;
    jsval key;
    PRHashEntry *he, **hep;
    JSAtom *atom;

#if PR_ALIGN_OF_DOUBLE == 8
    dp = &d;
#else
    char alignbuf[16];

    dp = (jsdouble *)&alignbuf[8 - ((pruword)&alignbuf & 7)];
    *dp = d;
#endif

    PR_ASSERT(JS_IS_LOCKED(cx));
    table = cx->runtime->atomState.table;
    keyHash = HASH_DOUBLE(dp);
    key = DOUBLE_TO_JSVAL(dp);
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
	if (!js_NewDoubleValue(cx, d, &key))
	    return NULL;
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
    }

    atom = (JSAtom *)he;
#ifdef DEBUG_DUPLICATE_ATOMS
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    he = *hep;
    PR_ASSERT(atom == (JSAtom *)he);
#endif

    if (flags & ATOM_NOHOLD)
	return atom;
    return js_HoldAtom(cx, atom);
}

JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, uintN flags)
{
    PRHashTable *table;
    PRHashNumber keyHash;
    jsval key;
    PRHashEntry *he, **hep;
    JSAtom *atom;

    PR_ASSERT(JS_IS_LOCKED(cx));
    table = cx->runtime->atomState.table;
    keyHash = js_HashString(str);
    key = STRING_TO_JSVAL(str);
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
	if (flags & ATOM_TMPSTR) {
	    if (flags & ATOM_NOCOPY)
		str = js_NewString(cx, str->chars, str->length, 0);
	    else
		str = js_NewStringCopyN(cx, str->chars, str->length, 0);
	    if (!str)
		return NULL;
	    key = STRING_TO_JSVAL(str);
	}
	he = PR_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
	if (!he) {
	    JS_ReportOutOfMemory(cx);
	    return NULL;
	}
    }

    atom = (JSAtom *)he;
#ifdef DEBUG_DUPLICATE_ATOMS
    hep = PR_HashTableRawLookup(table, keyHash, (void *)key);
    he = *hep;
    PR_ASSERT(atom == (JSAtom *)he);
#endif

    if (flags & ATOM_NOHOLD)
	return atom;
    return js_HoldAtom(cx, atom);
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
    return js_AtomizeHashedKey(cx, value, (PRHashNumber)value, flags);
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

jsatomid
js_IndexAtom(JSContext *cx, JSAtom *atom, JSAtomList *al)
{
    if (!(atom->flags & ATOM_INDEXED)) {
	atom->flags |= ATOM_INDEXED;
	atom->index = (jsatomid) al->count++;
	ATOM_SET_NEXT(atom, al->list);
	al->list = js_HoldAtom(cx, atom);
    }
    return atom->index;
}

JSAtom *
js_HoldAtom(JSContext *cx, JSAtom *atom)
{
#ifdef DEBUG_DUPLICATE_ATOMS
    jsval key;
    PRHashNumber keyHash;
    PRHashEntry *he, **hep;

    key = ATOM_KEY(atom);
    keyHash = js_hash_atom_key((void *)key);
    hep = PR_HashTableRawLookup(cx->runtime->atomState.table, keyHash,
				(void*)key);
    he = *hep;
    PR_ASSERT(atom == (JSAtom *)he);
#endif
    PR_ASSERT(JS_IS_LOCKED(cx));
    atom->nrefs++;
    PR_ASSERT(atom->nrefs > 0);
    return atom;
}

JS_FRIEND_API(JSAtom *)
js_DropAtom(JSContext *cx, JSAtom *atom)
{
#ifdef DEBUG_DUPLICATE_ATOMS
    jsval key;
    PRHashNumber keyHash;
    PRHashEntry *he, **hep;

    key = ATOM_KEY(atom);
    keyHash = js_hash_atom_key((void *)key);
    hep = PR_HashTableRawLookup(cx->runtime->atomState.table, keyHash,
				(void*)key);
    he = *hep;
    PR_ASSERT(atom == (JSAtom *)he);
#endif
    PR_ASSERT(JS_IS_LOCKED(cx));
    PR_ASSERT(atom->nrefs > 0);
    if (atom->nrefs <= 0)
	return NULL;
    if (--atom->nrefs == 0) {
	PR_HashTableRemove(cx->runtime->atomState.table, atom->entry.key);
#ifdef DEBUG_DUPLICATE_ATOMS
	hep = PR_HashTableRawLookup(cx->runtime->atomState.table, keyHash,
				    (void *)key);
	he = *hep;
	PR_ASSERT(he == NULL);
#endif
	atom = NULL;
    }
    return atom;
}

JSAtom *
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
    JSAtom *atom, *next, **vector;
    uint32 count;

    atom = al->list;
    if (!atom) {
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
        vector[atom->index] = atom;
	atom->flags &= ~ATOM_INDEXED;
	next = ATOM_NEXT(atom);
	ATOM_SET_NEXT(atom, NULL);
    } while ((atom = next) != NULL);
    al->list = NULL;
    al->count = 0;

    map->vector = vector;
    map->length = (jsatomid)count;
    return JS_TRUE;
}

JS_FRIEND_API(void)
js_FreeAtomMap(JSContext *cx, JSAtomMap *map)
{
    uintN i;

    if (map->vector) {
        for (i = 0; i < map->length; i++)
	    js_DropAtom(cx, map->vector[i]);
	free(map->vector);
	map->vector = NULL;
    }
    map->length = 0;
}

void
js_DropUnmappedAtoms(JSContext *cx, JSAtomList *al)
{
    JSAtom *atom, *next;

    for (atom = al->list; atom; atom = next) {
	atom->flags &= ~ATOM_INDEXED;
	next = ATOM_NEXT(atom);
	ATOM_SET_NEXT(atom, NULL);
	js_DropAtom(cx, atom);
    }
    al->list = NULL;
    al->count = 0;
}
