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
 * JS atom table.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsscan.h"
#include "jsstr.h"

JS_FRIEND_API(const char *)
js_AtomToPrintableString(JSContext *cx, JSAtom *atom)
{
    return js_ValueToPrintableString(cx, ATOM_KEY(atom));
}

#define JS_PROTO(name,code,init) const char js_##name##_str[] = #name;
#include "jsproto.tbl"
#undef JS_PROTO

/*
 * Names for common atoms defined in JSAtomState starting from
 * JSAtomState.emptyAtom until JSAtomState.lazy.
 *
 * The elements of the array after the first empty string define strings
 * corresponding to JSType enumerators from jspubtd.h and to two boolean
 * literals, false and true. The following assert insists that JSType defines
 * exactly 8 types.
 */
JS_STATIC_ASSERT(JSTYPE_LIMIT == 8);
const char *const js_common_atom_names[] = {
    "",                         /* emptyAtom                    */
    js_undefined_str,           /* typeAtoms[JSTYPE_VOID]       */
    js_object_str,              /* typeAtoms[JSTYPE_OBJECT]     */
    js_function_str,            /* typeAtoms[JSTYPE_FUNCTION]   */
    "string",                   /* typeAtoms[JSTYPE_STRING]     */
    "number",                   /* typeAtoms[JSTYPE_NUMBER]     */
    "boolean",                  /* typeAtoms[JSTYPE_BOOLEAN]    */
    js_null_str,                /* typeAtoms[JSTYPE_NULL]       */
    "xml",                      /* typeAtoms[JSTYPE_XML]        */
    js_false_str,               /* booleanAtoms[0]              */
    js_true_str,                /* booleanAtoms[1]              */
    js_null_str,                /* nullAtom                     */

#define JS_PROTO(name,code,init) js_##name##_str,
#include "jsproto.tbl"
#undef JS_PROTO

    js_anonymous_str,           /* anonymousAtom                */
    js_arguments_str,           /* argumentsAtom                */
    js_arity_str,               /* arityAtom                    */
    js_callee_str,              /* calleeAtom                   */
    js_caller_str,              /* callerAtom                   */
    js_class_prototype_str,     /* classPrototypeAtom           */
    js_constructor_str,         /* constructorAtom              */
    js_count_str,               /* countAtom                    */
    js_each_str,                /* eachAtom                     */
    js_eval_str,                /* evalAtom                     */
    js_fileName_str,            /* fileNameAtom                 */
    js_get_str,                 /* getAtom                      */
    js_getter_str,              /* getterAtom                   */
    js_index_str,               /* indexAtom                    */
    js_input_str,               /* inputAtom                    */
    js_iterator_str,            /* iteratorAtom                 */
    js_length_str,              /* lengthAtom                   */
    js_lineNumber_str,          /* lineNumberAtom               */
    js_message_str,             /* messageAtom                  */
    js_name_str,                /* nameAtom                     */
    js_next_str,                /* nextAtom                     */
    js_noSuchMethod_str,        /* noSuchMethodAtom             */
    js_parent_str,              /* parentAtom                   */
    js_proto_str,               /* protoAtom                    */
    js_set_str,                 /* setAtom                      */
    js_setter_str,              /* setterAtom                   */
    js_stack_str,               /* stackAtom                    */
    js_toLocaleString_str,      /* toLocaleStringAtom           */
    js_toSource_str,            /* toSourceAtom                 */
    js_toString_str,            /* toStringAtom                 */
    js_valueOf_str,             /* valueOfAtom                  */
    "(void 0)",                 /* void0Atom                    */

#if JS_HAS_XML_SUPPORT
    js_etago_str,               /* etagoAtom                    */
    js_namespace_str,           /* namespaceAtom                */
    js_ptagc_str,               /* ptagcAtom                    */
    js_qualifier_str,           /* qualifierAtom                */
    js_space_str,               /* spaceAtom                    */
    js_stago_str,               /* stagoAtom                    */
    js_star_str,                /* starAtom                     */
    js_starQualifier_str,       /* starQualifierAtom            */
    js_tagc_str,                /* tagcAtom                     */
    js_xml_str,                 /* xmlAtom                      */
#endif

#ifdef NARCISSUS
    js_call_str,                /* callAtom                     */
    js_construct_str,           /* constructAtom                */
    js_hasInstance_str,         /* hasInstanceAtom              */
    js_ExecutionContext_str,    /* ExecutionContextAtom         */
    js_current_str,             /* currentAtom                  */
#endif
};
JS_STATIC_ASSERT(JS_ARRAY_LENGTH(js_common_atom_names) * sizeof(JSAtom *) ==
                 LAZY_ATOM_OFFSET_START - ATOM_OFFSET_START);

const char js_anonymous_str[]       = "anonymous";
const char js_arguments_str[]       = "arguments";
const char js_arity_str[]           = "arity";
const char js_callee_str[]          = "callee";
const char js_caller_str[]          = "caller";
const char js_class_prototype_str[] = "prototype";
const char js_constructor_str[]     = "constructor";
const char js_count_str[]           = "__count__";
const char js_each_str[]            = "each";
const char js_eval_str[]            = "eval";
const char js_fileName_str[]        = "fileName";
const char js_get_str[]             = "get";
const char js_getter_str[]          = "getter";
const char js_index_str[]           = "index";
const char js_input_str[]           = "input";
const char js_iterator_str[]        = "__iterator__";
const char js_length_str[]          = "length";
const char js_lineNumber_str[]      = "lineNumber";
const char js_message_str[]         = "message";
const char js_name_str[]            = "name";
const char js_next_str[]            = "next";
const char js_noSuchMethod_str[]    = "__noSuchMethod__";
const char js_object_str[]          = "object";
const char js_parent_str[]          = "__parent__";
const char js_proto_str[]           = "__proto__";
const char js_setter_str[]          = "setter";
const char js_set_str[]             = "set";
const char js_stack_str[]           = "stack";
const char js_toSource_str[]        = "toSource";
const char js_toString_str[]        = "toString";
const char js_toLocaleString_str[]  = "toLocaleString";
const char js_undefined_str[]       = "undefined";
const char js_valueOf_str[]         = "valueOf";

#if JS_HAS_XML_SUPPORT
const char js_etago_str[]           = "</";
const char js_namespace_str[]       = "namespace";
const char js_ptagc_str[]           = "/>";
const char js_qualifier_str[]       = "::";
const char js_space_str[]           = " ";
const char js_stago_str[]           = "<";
const char js_star_str[]            = "*";
const char js_starQualifier_str[]   = "*::";
const char js_tagc_str[]            = ">";
const char js_xml_str[]             = "xml";
#endif

#if JS_HAS_GENERATORS
const char js_close_str[]           = "close";
const char js_send_str[]            = "send";
#endif

#ifdef NARCISSUS
const char js_call_str[]             = "__call__";
const char js_construct_str[]        = "__construct__";
const char js_hasInstance_str[]      = "__hasInstance__";
const char js_ExecutionContext_str[] = "ExecutionContext";
const char js_current_str[]          = "current";
#endif

#define HASH_DOUBLE(dp) ((JSDOUBLE_HI32(*dp) ^ JSDOUBLE_LO32(*dp)))

JS_STATIC_DLL_CALLBACK(JSHashNumber)
js_hash_atom_key(const void *key)
{
    jsval v;
    jsdouble *dp;

    v = (jsval)key;
    if (JSVAL_IS_STRING(v))
        return js_HashString(JSVAL_TO_STRING(v));
    if (JSVAL_IS_DOUBLE(v)) {
        dp = JSVAL_TO_DOUBLE(v);
        return HASH_DOUBLE(dp);
    }
    JS_ASSERT(JSVAL_IS_INT(v) || v == JSVAL_TRUE || v == JSVAL_FALSE ||
              v == JSVAL_NULL || v == JSVAL_VOID);
    return (JSHashNumber)v;
}

JS_STATIC_DLL_CALLBACK(intN)
js_compare_atom_keys(const void *k1, const void *k2)
{
    jsval v1, v2;

    v1 = (jsval)k1, v2 = (jsval)k2;
    if (JSVAL_IS_STRING(v1) && JSVAL_IS_STRING(v2))
        return js_EqualStrings(JSVAL_TO_STRING(v1), JSVAL_TO_STRING(v2));
    if (JSVAL_IS_DOUBLE(v1) && JSVAL_IS_DOUBLE(v2)) {
        double d1 = *JSVAL_TO_DOUBLE(v1);
        double d2 = *JSVAL_TO_DOUBLE(v2);
        if (JSDOUBLE_IS_NaN(d1))
            return JSDOUBLE_IS_NaN(d2);
#if defined(XP_WIN)
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(d2))
            return JS_FALSE;
#endif
        return d1 == d2;
    }
    return v1 == v2;
}

JS_STATIC_DLL_CALLBACK(int)
js_compare_stub(const void *v1, const void *v2)
{
    return 1;
}

/* These next two are exported to jsscript.c and used similarly there. */
void * JS_DLL_CALLBACK
js_alloc_table_space(void *priv, size_t size)
{
    return malloc(size);
}

void JS_DLL_CALLBACK
js_free_table_space(void *priv, void *item)
{
    free(item);
}

JS_STATIC_DLL_CALLBACK(JSHashEntry *)
js_alloc_atom(void *priv, const void *key)
{
    JSAtom *atom;

    atom = (JSAtom *) malloc(sizeof(JSAtom));
    if (!atom)
        return NULL;
    ((JSAtomState *)priv)->tablegen++;
    atom->entry.key = key;
    atom->entry.value = NULL;
    atom->flags = 0;
    return &atom->entry;
}

JS_STATIC_DLL_CALLBACK(void)
js_free_atom(void *priv, JSHashEntry *he, uintN flag)
{
    if (flag != HT_FREE_ENTRY)
        return;
    ((JSAtomState *)priv)->tablegen++;
    free(he);
}

static JSHashAllocOps atom_alloc_ops = {
    js_alloc_table_space,   js_free_table_space,
    js_alloc_atom,          js_free_atom
};

#define JS_ATOM_HASH_SIZE   1024

JSBool
js_InitAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

   /*
    * The caller must zero the state before calling this function.
    */
    JS_ASSERT(!state->table);
    JS_ASSERT(state->tablegen == 0);

    state->table = JS_NewHashTable(JS_ATOM_HASH_SIZE, js_hash_atom_key,
                                   js_compare_atom_keys, js_compare_stub,
                                   &atom_alloc_ops, state);
    if (!state->table)
        return JS_FALSE;

#ifdef JS_THREADSAFE
    js_InitLock(&state->lock);
#endif
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(intN)
js_atom_uninterner(JSHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;
    JSRuntime *rt;

    atom = (JSAtom *)he;
    rt = (JSRuntime *)arg;
    if (ATOM_IS_STRING(atom))
        js_FinalizeStringRT(rt, ATOM_TO_STRING(atom));
    return HT_ENUMERATE_NEXT;
}

void
js_FinishAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

    if (!state->table) {
        /*
         * state->table is null when JS_NewRuntime fails and calls
         * JS_DestroyRuntime on a partially initialized runtime.
         */
        return;
    }

    JS_HashTableEnumerateEntries(state->table, js_atom_uninterner, rt);
    JS_HashTableDestroy(state->table);
#ifdef JS_THREADSAFE
    js_FinishLock(&state->lock);
#endif
#ifdef DEBUG
    memset(state, JS_FREE_PATTERN, sizeof *state);
#endif
}

JSBool
js_InitCommonAtoms(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;
    uintN i;
    JSAtom **atoms;

    atoms = (JSAtom **)((uint8 *)state + ATOM_OFFSET_START);
    for (i = 0; i < JS_ARRAY_LENGTH(js_common_atom_names); i++, atoms++) {
        *atoms = js_Atomize(cx, js_common_atom_names[i],
                            strlen(js_common_atom_names[i]), ATOM_PINNED);
        if (!*atoms)
            return JS_FALSE;
    }
    JS_ASSERT((uint8 *)atoms - (uint8 *)state == LAZY_ATOM_OFFSET_START);
    memset(atoms, 0, ATOM_OFFSET_LIMIT - LAZY_ATOM_OFFSET_START);

    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(intN)
js_atom_unpinner(JSHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;

    atom = (JSAtom *)he;
    atom->flags &= ~ATOM_PINNED;
    return HT_ENUMERATE_NEXT;
}

void
js_FinishCommonAtoms(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;

    JS_HashTableEnumerateEntries(state->table, js_atom_unpinner, NULL);
#ifdef DEBUG
    memset((uint8 *)state + ATOM_OFFSET_START, JS_FREE_PATTERN,
           ATOM_OFFSET_LIMIT - ATOM_OFFSET_START);
#endif
}

void
js_TraceAtom(JSTracer *trc, JSAtom *atom)
{
    jsval key;

    key = ATOM_KEY(atom);
    JS_CALL_VALUE_TRACER(trc, key, "key");
    if (atom->flags & ATOM_HIDDEN)
        JS_CALL_TRACER(trc, atom->entry.value, JSTRACE_ATOM, "hidden");
}

typedef struct TraceArgs {
    JSBool      allAtoms;
    JSTracer    *trc;
} TraceArgs;

JS_STATIC_DLL_CALLBACK(intN)
js_locked_atom_tracer(JSHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;
    TraceArgs *args;

    atom = (JSAtom *)he;
    args = (TraceArgs *)arg;
    if ((atom->flags & (ATOM_PINNED | ATOM_INTERNED)) || args->allAtoms) {
        JS_SET_TRACING_INDEX(args->trc,
                             (atom->flags & ATOM_PINNED)
                             ? "pinned_atom"
                             : (atom->flags & ATOM_INTERNED)
                             ? "interned_atom"
                             : "locked_atom",
                             (size_t)i);
        JS_CallTracer(args->trc, atom, JSTRACE_ATOM);
    }
    return HT_ENUMERATE_NEXT;
}

void
js_TraceLockedAtoms(JSTracer *trc, JSBool allAtoms)
{
    JSAtomState *state;
    TraceArgs args;

    state = &trc->context->runtime->atomState;
    if (!state->table)
        return;
    args.allAtoms = allAtoms;
    args.trc = trc;
    JS_HashTableEnumerateEntries(state->table, js_locked_atom_tracer, &args);
}

JS_STATIC_DLL_CALLBACK(intN)
js_atom_sweeper(JSHashEntry *he, intN i, void *arg)
{
    JSAtom *atom;

    atom = (JSAtom *)he;
    if (atom->flags & ATOM_MARK) {
        atom->flags &= ~ATOM_MARK;
        return HT_ENUMERATE_NEXT;
    }
    JS_ASSERT((atom->flags & (ATOM_PINNED | ATOM_INTERNED)) == 0);
    atom->entry.key = atom->entry.value = NULL;
    atom->flags = 0;
    return HT_ENUMERATE_REMOVE;
}

void
js_SweepAtomState(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;

    JS_HashTableEnumerateEntries(state->table, js_atom_sweeper, NULL);
}

static JSAtom *
AtomizeHashedKey(JSContext *cx, jsval key, JSHashNumber keyHash)
{
    JSAtomState *state;
    JSHashTable *table;
    JSHashEntry *he, **hep;
    JSAtom *atom;

    state = &cx->runtime->atomState;
    JS_LOCK(&state->lock, cx);
    table = state->table;
    hep = JS_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
        he = JS_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
        if (!he) {
            JS_UNLOCK(&state->lock,cx);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    atom = (JSAtom *)he;
    cx->weakRoots.lastAtom = atom;
    JS_UNLOCK(&state->lock,cx);
    return atom;
}

/* Worst-case alignment grain and aligning macro for 2x-sized buffer. */
#define ALIGNMENT(t)    JS_MAX(JSVAL_ALIGN, sizeof(t))
#define ALIGN(b,t)      ((t*) &(b)[ALIGNMENT(t) - (jsuword)(b) % ALIGNMENT(t)])

JSAtom *
js_AtomizeDouble(JSContext *cx, jsdouble d)
{
    char buf[2 * ALIGNMENT(double)];
    jsdouble *dp;
    JSHashNumber keyHash;
    jsval key;
    JSAtomState *state;
    JSHashTable *table;
    JSHashEntry *he, **hep;
    uint32 gen;
    JSAtom *atom;

    dp = ALIGN(buf, double);
    *dp = d;
    keyHash = HASH_DOUBLE(dp);
    key = DOUBLE_TO_JSVAL(dp);
    state = &cx->runtime->atomState;
    table = state->table;

    JS_LOCK(&state->lock, cx);
    hep = JS_HashTableRawLookup(table, keyHash, (void *)key);
    if ((he = *hep) == NULL) {
        gen = state->tablegen;
        JS_UNLOCK(&state->lock, cx);

        if (!js_NewDoubleValue(cx, d, &key))
            return NULL;

        JS_LOCK(&state->lock, cx);
        if (state->tablegen != gen) {
            hep = JS_HashTableRawLookup(table, keyHash, (void *)key);
            if ((he = *hep) != NULL)
                goto finish;
        }
        he = JS_HashTableRawAdd(table, hep, keyHash, (void *)key, NULL);
        if (!he) {
            JS_UNLOCK(&state->lock, cx);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

  finish:
    atom = (JSAtom *)he;
    cx->weakRoots.lastAtom = atom;
    JS_UNLOCK(&state->lock,cx);
    return atom;
}

/*
 * To put an atom into the hidden subspace. XOR its keyHash with this value,
 * which is (sqrt(2)-1) in 32-bit fixed point.
 */
#define HIDDEN_ATOM_SUBSPACE_KEYHASH    0x6A09E667

JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, uintN flags)
{
    JSHashNumber keyHash;
    void *key;
    JSAtomState *state;
    JSHashTable *table;
    JSHashEntry *he, **hep;
    uint32 gen;
    JSString *hashed;
    JSAtom *atom;

    keyHash = js_HashString(str);
    if (flags & ATOM_HIDDEN)
        keyHash ^= HIDDEN_ATOM_SUBSPACE_KEYHASH;
    key = (void *)STRING_TO_JSVAL(str);
    state = &cx->runtime->atomState;
    table = state->table;

    JS_LOCK(&state->lock, cx);
    hep = JS_HashTableRawLookup(table, keyHash, key);
    if ((he = *hep) == NULL) {
        gen = state->tablegen;
        JS_UNLOCK(&state->lock, cx);

        if (flags & ATOM_TMPSTR) {
            if (flags & ATOM_NOCOPY) {
                hashed = js_NewString(cx, str->chars, str->length, 0);
                if (!hashed)
                    return NULL;

                /* Transfer ownership of str->chars to GC-controlled string. */
                str->chars = NULL;
            } else {
                hashed = js_NewStringCopyN(cx, str->chars, str->length);
                if (!hashed)
                    return NULL;
            }
            key = (void *)STRING_TO_JSVAL(hashed);
        } else {
            JS_ASSERT((flags & ATOM_NOCOPY) == 0);
            if (!JS_MakeStringImmutable(cx, str))
                return NULL;
        }

        JS_LOCK(&state->lock, cx);
        if (state->tablegen != gen) {
            hep = JS_HashTableRawLookup(table, keyHash, key);
            if ((he = *hep) != NULL)
                goto finish;
        }
        he = JS_HashTableRawAdd(table, hep, keyHash, key, NULL);
        if (!he) {
            JS_UNLOCK(&state->lock, cx);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

  finish:
    atom = (JSAtom *)he;
    atom->flags |= flags & (ATOM_PINNED | ATOM_INTERNED | ATOM_HIDDEN);
    cx->weakRoots.lastAtom = atom;
    JS_UNLOCK(&state->lock, cx);
    return atom;
}

JS_FRIEND_API(JSAtom *)
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags)
{
    jschar *chars;
    JSString *str;
    JSAtom *atom;
    char buf[2 * ALIGNMENT(JSString)];

    /*
     * Avoiding the malloc in js_InflateString on shorter strings saves us
     * over 20,000 malloc calls on mozilla browser startup. This compares to
     * only 131 calls where the string is longer than a 31 char (net) buffer.
     * The vast majority of atomized strings are already in the hashtable. So
     * js_AtomizeString rarely has to copy the temp string we make.
     */
#define ATOMIZE_BUF_MAX 32
    jschar inflated[ATOMIZE_BUF_MAX];
    size_t inflatedLength = ATOMIZE_BUF_MAX - 1;

    if (length < ATOMIZE_BUF_MAX) {
        js_InflateStringToBuffer(cx, bytes, length, inflated, &inflatedLength);
        inflated[inflatedLength] = 0;
        chars = inflated;
    } else {
        inflatedLength = length;
        chars = js_InflateString(cx, bytes, &inflatedLength);
        if (!chars)
            return NULL;
        flags |= ATOM_NOCOPY;
    }

    str = ALIGN(buf, JSString);

    str->chars = chars;
    str->length = inflatedLength;
    atom = js_AtomizeString(cx, str, ATOM_TMPSTR | flags);
    if (chars != inflated && str->chars)
        JS_free(cx, chars);
    return atom;
}

JS_FRIEND_API(JSAtom *)
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length, uintN flags)
{
    JSString *str;
    char buf[2 * ALIGNMENT(JSString)];

    str = ALIGN(buf, JSString);
    str->chars = (jschar *)chars;
    str->length = length;
    return js_AtomizeString(cx, str, ATOM_TMPSTR | flags);
}

JSAtom *
js_GetExistingStringAtom(JSContext *cx, const jschar *chars, size_t length)
{
    JSString *str;
    char buf[2 * ALIGNMENT(JSString)];
    JSHashNumber keyHash;
    jsval key;
    JSAtomState *state;
    JSHashTable *table;
    JSHashEntry **hep;

    str = ALIGN(buf, JSString);
    str->chars = (jschar *)chars;
    str->length = length;
    keyHash = js_HashString(str);
    key = STRING_TO_JSVAL(str);
    state = &cx->runtime->atomState;
    JS_LOCK(&state->lock, cx);
    table = state->table;
    hep = JS_HashTableRawLookup(table, keyHash, (void *)key);
    JS_UNLOCK(&state->lock, cx);
    return (hep) ? (JSAtom *)*hep : NULL;
}

JSAtom *
js_AtomizePrimitiveValue(JSContext *cx, jsval v)
{
    if (JSVAL_IS_STRING(v))
        return js_AtomizeString(cx, JSVAL_TO_STRING(v), 0);
    if (JSVAL_IS_DOUBLE(v))
        return js_AtomizeDouble(cx, *JSVAL_TO_DOUBLE(v));
    JS_ASSERT(JSVAL_IS_INT(v) || v == JSVAL_TRUE || v == JSVAL_FALSE ||
              v == JSVAL_NULL || v == JSVAL_VOID);
    return AtomizeHashedKey(cx, v, (JSHashNumber)v);
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

#ifdef DEBUG

JS_STATIC_DLL_CALLBACK(int)
atom_dumper(JSHashEntry *he, int i, void *arg)
{
    FILE *fp = (FILE *)arg;
    JSAtom *atom = (JSAtom *)he;

    fprintf(fp, "%3u %08x ", (uintN)i, (uintN)he->keyHash);
    if (ATOM_IS_STRING(atom))
        js_FileEscapedString(fp, ATOM_TO_STRING(atom), '"');
    else if (ATOM_IS_INT(atom))
        fprintf(fp, "%ld", (long)ATOM_TO_INT(atom));
    else
        fprintf(fp, "%.16g", *ATOM_TO_DOUBLE(atom));
    putc('\n', fp);
    return HT_ENUMERATE_NEXT;
}

JS_FRIEND_API(void)
js_DumpAtoms(JSContext *cx, FILE *fp)
{
    JSAtomState *state = &cx->runtime->atomState;

    fprintf(fp, "\natom table contents:\n");
    JS_HashTableEnumerateEntries(state->table, atom_dumper, fp);
#ifdef HASHMETER
    JS_HashTableDumpMeter(state->table, atom_dumper, fp);
#endif
}

#endif

JS_STATIC_DLL_CALLBACK(JSHashNumber)
js_hash_atom_ptr(const void *key)
{
    const JSAtom *atom = (const JSAtom *) key;
    return ATOM_HASH(atom);
}

JS_STATIC_DLL_CALLBACK(void *)
js_alloc_temp_space(void *priv, size_t size)
{
    JSContext *cx = (JSContext *) priv;
    void *space;

    JS_ARENA_ALLOCATE(space, &cx->tempPool, size);
    if (!space)
        JS_ReportOutOfMemory(cx);
    return space;
}

JS_STATIC_DLL_CALLBACK(void)
js_free_temp_space(void *priv, void *item)
{
}

JS_STATIC_DLL_CALLBACK(JSHashEntry *)
js_alloc_temp_entry(void *priv, const void *key)
{
    JSContext *cx = (JSContext *) priv;
    JSAtomListElement *ale;

    JS_ARENA_ALLOCATE_TYPE(ale, JSAtomListElement, &cx->tempPool);
    if (!ale) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    return &ale->entry;
}

JS_STATIC_DLL_CALLBACK(void)
js_free_temp_entry(void *priv, JSHashEntry *he, uintN flag)
{
}

static JSHashAllocOps temp_alloc_ops = {
    js_alloc_temp_space,    js_free_temp_space,
    js_alloc_temp_entry,    js_free_temp_entry
};

JSAtomListElement *
js_IndexAtom(JSContext *cx, JSAtom *atom, JSAtomList *al)
{
    JSAtomListElement *ale, *ale2, *next;
    JSHashEntry **hep;

    ATOM_LIST_LOOKUP(ale, hep, al, atom);
    if (!ale) {
        if (al->count < 10) {
            /* Few enough for linear search, no hash table needed. */
            JS_ASSERT(!al->table);
            ale = (JSAtomListElement *)js_alloc_temp_entry(cx, atom);
            if (!ale)
                return NULL;
            ALE_SET_ATOM(ale, atom);
            ale->entry.next = al->list;
            al->list = &ale->entry;
        } else {
            /* We want to hash.  Have we already made a hash table? */
            if (!al->table) {
                /* No hash table yet, so hep had better be null! */
                JS_ASSERT(!hep);
                al->table = JS_NewHashTable(al->count + 1, js_hash_atom_ptr,
                                            JS_CompareValues, JS_CompareValues,
                                            &temp_alloc_ops, cx);
                if (!al->table)
                    return NULL;

                /*
                 * Set ht->nentries explicitly, because we are moving entries
                 * from al to ht, not calling JS_HashTable(Raw|)Add.
                 */
                al->table->nentries = al->count;

                /* Insert each ale on al->list into the new hash table. */
                for (ale2 = (JSAtomListElement *)al->list; ale2; ale2 = next) {
                    next = ALE_NEXT(ale2);
                    ale2->entry.keyHash = ATOM_HASH(ALE_ATOM(ale2));
                    hep = JS_HashTableRawLookup(al->table, ale2->entry.keyHash,
                                                ale2->entry.key);
                    ale2->entry.next = *hep;
                    *hep = &ale2->entry;
                }
                al->list = NULL;

                /* Set hep for insertion of atom's ale, immediately below. */
                hep = JS_HashTableRawLookup(al->table, ATOM_HASH(atom), atom);
            }

            /* Finally, add an entry for atom into the hash bucket at hep. */
            ale = (JSAtomListElement *)
                  JS_HashTableRawAdd(al->table, hep, ATOM_HASH(atom), atom,
                                     NULL);
            if (!ale)
                return NULL;
        }

        ALE_SET_INDEX(ale, al->count++);
    }
    return ale;
}

JS_STATIC_DLL_CALLBACK(intN)
js_map_atom(JSHashEntry *he, intN i, void *arg)
{
    JSAtomListElement *ale = (JSAtomListElement *)he;
    JSAtom **vector = (JSAtom **) arg;

    vector[ALE_INDEX(ale)] = ALE_ATOM(ale);
    return HT_ENUMERATE_NEXT;
}

#ifdef DEBUG
static jsrefcount js_atom_map_count;
static jsrefcount js_atom_map_hash_table_count;
#endif

JS_FRIEND_API(void)
js_InitAtomMap(JSContext *cx, JSAtomMap *map, JSAtomList *al)
{
    JSAtom **vector;
    JSAtomListElement *ale;
    uint32 count;

    /* Map length must already be initialized. */
    JS_ASSERT(al->count == map->length);
#ifdef DEBUG
    JS_ATOMIC_INCREMENT(&js_atom_map_count);
#endif
    ale = (JSAtomListElement *)al->list;
    if (!ale && !al->table) {
        JS_ASSERT(!map->vector);
        return;
    }

    count = al->count;
    vector = map->vector;
    if (al->table) {
#ifdef DEBUG
        JS_ATOMIC_INCREMENT(&js_atom_map_hash_table_count);
#endif
        JS_HashTableEnumerateEntries(al->table, js_map_atom, vector);
    } else {
        do {
            vector[ALE_INDEX(ale)] = ALE_ATOM(ale);
        } while ((ale = ALE_NEXT(ale)) != NULL);
    }
    ATOM_LIST_INIT(al);
}
