/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbit.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsparse.h"
#include "jsscan.h"
#include "jsstr.h"
#include "jsversion.h"
#include "jsxml.h"

#include "jsstrinlines.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

/*
 * ATOM_HASH assumes that JSHashNumber is 32-bit even on 64-bit systems.
 */
JS_STATIC_ASSERT(sizeof(JSHashNumber) == 4);
JS_STATIC_ASSERT(sizeof(JSAtom *) == JS_BYTES_PER_WORD);

/*
 * Start and limit offsets for atom pointers in JSAtomState must be aligned
 * on the word boundary.
 */
JS_STATIC_ASSERT(ATOM_OFFSET_START % sizeof(JSAtom *) == 0);
JS_STATIC_ASSERT(ATOM_OFFSET_LIMIT % sizeof(JSAtom *) == 0);

/*
 * JS_BOOLEAN_STR and JS_TYPE_STR assume that boolean names starts from the
 * index 1 and type name starts from the index 1+2 atoms in JSAtomState.
 */
JS_STATIC_ASSERT(1 * sizeof(JSAtom *) ==
                 offsetof(JSAtomState, booleanAtoms) - ATOM_OFFSET_START);
JS_STATIC_ASSERT((1 + 2) * sizeof(JSAtom *) ==
                 offsetof(JSAtomState, typeAtoms) - ATOM_OFFSET_START);

const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom)
{
    return js_ValueToPrintableString(cx, StringValue(ATOM_TO_STRING(atom)));
}

#define JS_PROTO(name,code,init) const char js_##name##_str[] = #name;
#include "jsproto.tbl"
#undef JS_PROTO

/*
 * String constants for common atoms defined in JSAtomState starting from
 * JSAtomState.emptyAtom until JSAtomState.lazy.
 *
 * The elements of the array after the first empty string define strings
 * corresponding to the two boolean literals, false and true, followed by the
 * JSType enumerators from jspubtd.h starting with "undefined" for JSTYPE_VOID
 * (which is special-value 2) and continuing as initialized below. The static
 * asserts check these relations.
 */
JS_STATIC_ASSERT(JSTYPE_LIMIT == 8);
JS_STATIC_ASSERT(JSTYPE_VOID == 0);

const char *const js_common_atom_names[] = {
    "",                         /* emptyAtom                    */
    js_false_str,               /* booleanAtoms[0]              */
    js_true_str,                /* booleanAtoms[1]              */
    js_undefined_str,           /* typeAtoms[JSTYPE_VOID]       */
    js_object_str,              /* typeAtoms[JSTYPE_OBJECT]     */
    js_function_str,            /* typeAtoms[JSTYPE_FUNCTION]   */
    "string",                   /* typeAtoms[JSTYPE_STRING]     */
    "number",                   /* typeAtoms[JSTYPE_NUMBER]     */
    "boolean",                  /* typeAtoms[JSTYPE_BOOLEAN]    */
    js_null_str,                /* typeAtoms[JSTYPE_NULL]       */
    "xml",                      /* typeAtoms[JSTYPE_XML]        */
    js_null_str,                /* nullAtom                     */

#define JS_PROTO(name,code,init) js_##name##_str,
#include "jsproto.tbl"
#undef JS_PROTO

    js_anonymous_str,           /* anonymousAtom                */
    js_apply_str,               /* applyAtom                    */
    js_arguments_str,           /* argumentsAtom                */
    js_arity_str,               /* arityAtom                    */
    js_call_str,                /* callAtom                     */
    js_callee_str,              /* calleeAtom                   */
    js_caller_str,              /* callerAtom                   */
    js_class_prototype_str,     /* classPrototypeAtom           */
    js_constructor_str,         /* constructorAtom              */
    js_each_str,                /* eachAtom                     */
    js_eval_str,                /* evalAtom                     */
    js_fileName_str,            /* fileNameAtom                 */
    js_get_str,                 /* getAtom                      */
    js_global_str,              /* globalAtom                   */
    js_ignoreCase_str,          /* ignoreCaseAtom               */
    js_index_str,               /* indexAtom                    */
    js_input_str,               /* inputAtom                    */
    "toISOString",              /* toISOStringAtom              */
    js_iterator_str,            /* iteratorAtom                 */
    js_join_str,                /* joinAtom                     */
    js_lastIndex_str,           /* lastIndexAtom                */
    js_length_str,              /* lengthAtom                   */
    js_lineNumber_str,          /* lineNumberAtom               */
    js_message_str,             /* messageAtom                  */
    js_multiline_str,           /* multilineAtom                */
    js_name_str,                /* nameAtom                     */
    js_next_str,                /* nextAtom                     */
    js_noSuchMethod_str,        /* noSuchMethodAtom             */
    js_proto_str,               /* protoAtom                    */
    js_set_str,                 /* setAtom                      */
    js_source_str,              /* sourceAtom                   */
    js_stack_str,               /* stackAtom                    */
    js_sticky_str,              /* stickyAtom                   */
    js_toGMTString_str,         /* toGMTStringAtom              */
    js_toLocaleString_str,      /* toLocaleStringAtom           */
    js_toSource_str,            /* toSourceAtom                 */
    js_toString_str,            /* toStringAtom                 */
    js_toUTCString_str,         /* toUTCStringAtom              */
    js_valueOf_str,             /* valueOfAtom                  */
    js_toJSON_str,              /* toJSONAtom                   */
    "(void 0)",                 /* void0Atom                    */
    js_enumerable_str,          /* enumerableAtom               */
    js_configurable_str,        /* configurableAtom             */
    js_writable_str,            /* writableAtom                 */
    js_value_str,               /* valueAtom                    */
    js_test_str,                /* testAtom                     */
    "use strict",               /* useStrictAtom                */

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
    "@mozilla.org/js/function", /* functionNamespaceURIAtom     */
#endif

    "Proxy",                    /* ProxyAtom                    */

    "getOwnPropertyDescriptor", /* getOwnPropertyDescriptorAtom */
    "getPropertyDescriptor",    /* getPropertyDescriptorAtom    */
    "defineProperty",           /* definePropertyAtom           */
    "delete",                   /* deleteAtom                   */
    "getOwnPropertyNames",      /* getOwnPropertyNames          */
    "enumerate",                /* enumerateAtom                */
    "fix",                      /* fixAtom                      */

    "has",                      /* hasAtom                      */
    "hasOwn",                   /* hasOwnAtom                   */
    "enumerateOwn",             /* enumerateOwnAtom             */
    "iterate"                   /* iterateAtom                  */
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(js_common_atom_names) * sizeof(JSAtom *) ==
                 LAZY_ATOM_OFFSET_START - ATOM_OFFSET_START);

/*
 * Interpreter macros called by the trace recorder assume common atom indexes
 * fit in one byte of immediate operand.
 */
JS_STATIC_ASSERT(JS_ARRAY_LENGTH(js_common_atom_names) < 256);

const size_t js_common_atom_count = JS_ARRAY_LENGTH(js_common_atom_names);

const char js_anonymous_str[]       = "anonymous";
const char js_apply_str[]           = "apply";
const char js_arguments_str[]       = "arguments";
const char js_arity_str[]           = "arity";
const char js_call_str[]            = "call";
const char js_callee_str[]          = "callee";
const char js_caller_str[]          = "caller";
const char js_class_prototype_str[] = "prototype";
const char js_constructor_str[]     = "constructor";
const char js_each_str[]            = "each";
const char js_eval_str[]            = "eval";
const char js_fileName_str[]        = "fileName";
const char js_get_str[]             = "get";
const char js_getter_str[]          = "getter";
const char js_global_str[]          = "global";
const char js_ignoreCase_str[]      = "ignoreCase";
const char js_index_str[]           = "index";
const char js_input_str[]           = "input";
const char js_iterator_str[]        = "__iterator__";
const char js_join_str[]            = "join";
const char js_lastIndex_str[]       = "lastIndex";
const char js_length_str[]          = "length";
const char js_lineNumber_str[]      = "lineNumber";
const char js_message_str[]         = "message";
const char js_multiline_str[]       = "multiline";
const char js_name_str[]            = "name";
const char js_next_str[]            = "next";
const char js_noSuchMethod_str[]    = "__noSuchMethod__";
const char js_object_str[]          = "object";
const char js_proto_str[]           = "__proto__";
const char js_setter_str[]          = "setter";
const char js_set_str[]             = "set";
const char js_source_str[]          = "source";
const char js_stack_str[]           = "stack";
const char js_sticky_str[]          = "sticky";
const char js_toGMTString_str[]     = "toGMTString";
const char js_toLocaleString_str[]  = "toLocaleString";
const char js_toSource_str[]        = "toSource";
const char js_toString_str[]        = "toString";
const char js_toUTCString_str[]     = "toUTCString";
const char js_undefined_str[]       = "undefined";
const char js_valueOf_str[]         = "valueOf";
const char js_toJSON_str[]          = "toJSON";
const char js_enumerable_str[]      = "enumerable";
const char js_configurable_str[]    = "configurable";
const char js_writable_str[]        = "writable";
const char js_value_str[]           = "value";
const char js_test_str[]            = "test";

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

/*
 * Helper macros to access and modify JSAtomHashEntry.
 */

inline AtomEntryType
StringToInitialAtomEntry(JSString *str)
{
    return (AtomEntryType) str;
}

inline uintN
AtomEntryFlags(AtomEntryType entry)
{
    return (uintN) (entry & ATOM_ENTRY_FLAG_MASK);
}

/*
 * Conceptually, we have compressed a HashMap<JSAtom *, uint> into a
 * HashMap<size_t>. Here, we promise that we are only changing the "value" of
 * the HashMap entry, so the const_cast is safe.
 */

inline void
AddAtomEntryFlags(const AtomEntryType &entry, uintN flags)
{
    const_cast<AtomEntryType &>(entry) |= AtomEntryType(flags);
}

inline void
ClearAtomEntryFlags(const AtomEntryType &entry, uintN flags)
{
    const_cast<AtomEntryType &>(entry) &= ~AtomEntryType(flags);
}

/*
 * For a browser build from 2007-08-09 after the browser starts up there are
 * just 55 double atoms, but over 15000 string atoms. Not to penalize more
 * economical embeddings allocating too much memory initially we initialize
 * atomized strings with just 1K entries.
 */
#define JS_STRING_HASH_COUNT   1024

JSBool
js_InitAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

    JS_ASSERT(!state->atoms.initialized());
    if (!state->atoms.init(JS_STRING_HASH_COUNT))
        return false;

#ifdef JS_THREADSAFE
    js_InitLock(&state->lock);
#endif
    JS_ASSERT(state->atoms.initialized());
    return JS_TRUE;
}

void
js_FinishAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

    if (!state->atoms.initialized()) {
        /*
         * We are called with uninitialized state when JS_NewRuntime fails and
         * calls JS_DestroyRuntime on a partially initialized runtime.
         */
        return;
    }

    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
        JSString *str = AtomEntryToKey(r.front());
        js_FinalizeStringRT(rt, str);
    }

#ifdef JS_THREADSAFE
    js_FinishLock(&state->lock);
#endif
}

JSBool
js_InitCommonAtoms(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;
    uintN i;
    JSAtom **atoms;

    atoms = COMMON_ATOMS_START(state);
    for (i = 0; i < JS_ARRAY_LENGTH(js_common_atom_names); i++, atoms++) {
        *atoms = js_Atomize(cx, js_common_atom_names[i],
                            strlen(js_common_atom_names[i]), ATOM_PINNED);
        if (!*atoms)
            return JS_FALSE;
    }
    JS_ASSERT((uint8 *)atoms - (uint8 *)state == LAZY_ATOM_OFFSET_START);
    memset(atoms, 0, ATOM_OFFSET_LIMIT - LAZY_ATOM_OFFSET_START);

    cx->runtime->emptyString = ATOM_TO_STRING(state->emptyAtom);
    return JS_TRUE;
}

void
js_FinishCommonAtoms(JSContext *cx)
{
    cx->runtime->emptyString = NULL;
    JSAtomState *state = &cx->runtime->atomState;

    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront())
        ClearAtomEntryFlags(r.front(), ATOM_PINNED);

#ifdef DEBUG
    memset(COMMON_ATOMS_START(state), JS_FREE_PATTERN,
           ATOM_OFFSET_LIMIT - ATOM_OFFSET_START);
#endif
}

void
js_TraceAtomState(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;
    JSAtomState *state = &rt->atomState;

#ifdef DEBUG
    size_t number = 0;
#endif

    if (rt->gcKeepAtoms) {
        for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
            JS_SET_TRACING_INDEX(trc, "locked_atom", number++);
            MarkString(trc, AtomEntryToKey(r.front()));
        }
    } else {
        for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
            AtomEntryType entry = r.front();
            uintN flags = AtomEntryFlags(entry);
            if (flags & (ATOM_PINNED | ATOM_INTERNED)) {
                JS_SET_TRACING_INDEX(trc,
                                     flags & ATOM_PINNED
                                     ? "pinned_atom"
                                     : "interned_atom",
                                     number++);
                MarkString(trc, AtomEntryToKey(entry));
            }
        }
    }
}

void
js_SweepAtomState(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;

    for (AtomSet::Enum e(state->atoms); !e.empty(); e.popFront()) {
        AtomEntryType entry = e.front();
        if (AtomEntryFlags(entry) & (ATOM_PINNED | ATOM_INTERNED)) {
            /* Pinned or interned key cannot be finalized. */
            JS_ASSERT(!IsAboutToBeFinalized(AtomEntryToKey(entry)));
        } else if (IsAboutToBeFinalized(AtomEntryToKey(entry))) {
            e.removeFront();
        }
    }
}

JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, uintN flags)
{
    JS_ASSERT(!(flags & ~(ATOM_PINNED|ATOM_INTERNED|ATOM_TMPSTR|ATOM_NOCOPY)));
    JS_ASSERT_IF(flags & ATOM_NOCOPY, flags & ATOM_TMPSTR);

    if (str->isAtomized())
        return STRING_TO_ATOM(str);

    size_t length = str->length();
    if (length == 1) {
        jschar c = str->chars()[0];
        if (c < UNIT_STRING_LIMIT)
            return STRING_TO_ATOM(JSString::unitString(c));
    }

    if (length == 2) {
        jschar *chars = str->chars();
        if (JSString::fitsInSmallChar(chars[0]) &&
            JSString::fitsInSmallChar(chars[1])) {
            return STRING_TO_ATOM(JSString::length2String(chars[0], chars[1]));
        }
    }

    /*
     * Here we know that JSString::intStringTable covers only 256 (or at least
     * not 1000 or more) chars. We rely on order here to resolve the unit vs.
     * int string/length-2 string atom identity issue by giving priority to unit
     * strings for "0" through "9" and length-2 strings for "10" through "99".
     */
    JS_STATIC_ASSERT(INT_STRING_LIMIT <= 999);
    if (length == 3) {
        const jschar *chars = str->chars();

        if ('1' <= chars[0] && chars[0] <= '9' &&
            '0' <= chars[1] && chars[1] <= '9' &&
            '0' <= chars[2] && chars[2] <= '9') {
            jsint i = (chars[0] - '0') * 100 +
                      (chars[1] - '0') * 10 +
                      (chars[2] - '0');

            if (jsuint(i) < INT_STRING_LIMIT)
                return STRING_TO_ATOM(JSString::intString(i));
        }
    }

    JSAtomState *state = &cx->runtime->atomState;
    AtomSet &atoms = state->atoms;

    AutoLockDefaultCompartment lock(cx);
    AtomSet::AddPtr p = atoms.lookupForAdd(str);

    /* Hashing the string should have flattened it if it was a rope. */
    JS_ASSERT(str->isFlat() || str->isDependent());

    JSString *key;
    if (p) {
        key = AtomEntryToKey(*p);
    } else {
        /*
         * Ensure that any atomized string lives only in the default
         * compartment.
         */
        bool needNewString = !!(flags & ATOM_TMPSTR) ||
                             str->asCell()->compartment() != cx->runtime->defaultCompartment;

        /*
         * Unless str is already comes from the default compartment and flat,
         * we have to relookup the key as the last ditch GC invoked from the
         * string allocation or OOM handling may unlock the default
         * compartment lock.
         */
        if (!needNewString && str->isFlat()) {
            str->flatClearMutable();
            key = str;
            atoms.add(p, StringToInitialAtomEntry(key));
        } else {
            if (needNewString) {
                SwitchToCompartment sc(cx, cx->runtime->defaultCompartment);
                jschar *chars = str->chars();
                if (flags & ATOM_NOCOPY) {
                    key = js_NewString(cx, chars, length);
                    if (!key)
                        return NULL;

                    /* Finish handing off chars to the GC'ed key string. */
                    JS_ASSERT(flags & ATOM_TMPSTR);
                    str->mChars = NULL;
                } else {
                    key = js_NewStringCopyN(cx, chars, length);
                    if (!key)
                        return NULL;
                }
            } else {
                JS_ASSERT(str->isDependent());
                if (!str->undepend(cx))
                    return NULL;
                key = str;
            }

            if (!atoms.relookupOrAdd(p, key, StringToInitialAtomEntry(key))) {
                JS_ReportOutOfMemory(cx); /* SystemAllocPolicy does not report */
                return NULL;
            }
        }
        key->flatSetAtomized();
    }

    AddAtomEntryFlags(*p, flags & (ATOM_PINNED | ATOM_INTERNED));

    JS_ASSERT(key->isAtomized());
    JSAtom *atom = STRING_TO_ATOM(key);
    return atom;
}

JSAtom *
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags)
{
    jschar *chars;
    JSString str;
    JSAtom *atom;

    CHECK_REQUEST(cx);

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

    str.initFlat(chars, inflatedLength);
    atom = js_AtomizeString(cx, &str, ATOM_TMPSTR | flags);
    if (chars != inflated && str.flatChars())
        cx->free(chars);
    return atom;
}

JSAtom *
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length, uintN flags)
{
    JSString str;

    CHECK_REQUEST(cx);
    str.initFlat((jschar *)chars, length);
    return js_AtomizeString(cx, &str, ATOM_TMPSTR | flags);
}

JSAtom *
js_GetExistingStringAtom(JSContext *cx, const jschar *chars, size_t length)
{
    JSString str, *str2;
    JSAtomState *state;

    if (length == 1) {
        jschar c = *chars;
        if (c < UNIT_STRING_LIMIT)
            return STRING_TO_ATOM(JSString::unitString(c));
    }

    str.initFlat((jschar *)chars, length);
    state = &cx->runtime->atomState;

    JS_LOCK(cx, &state->lock);
    AtomSet::Ptr p = state->atoms.lookup(&str);
    str2 = p ? AtomEntryToKey(*p) : NULL;
    JS_UNLOCK(cx, &state->lock);

    return str2 ? STRING_TO_ATOM(str2) : NULL;
}

#ifdef DEBUG
JS_FRIEND_API(void)
js_DumpAtoms(JSContext *cx, FILE *fp)
{
    JSAtomState *state = &cx->runtime->atomState;

    fprintf(fp, "atoms table contents:\n");
    unsigned number = 0;
    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
        AtomEntryType entry = r.front();
        fprintf(fp, "%3u ", number++);
        if (entry == 0) {
            fputs("<uninitialized>", fp);
        } else {
            JSString *key = AtomEntryToKey(entry);
            js_FileEscapedString(fp, key, '"');
            uintN flags = AtomEntryFlags(entry);
            if (flags != 0) {
                fputs((flags & (ATOM_PINNED | ATOM_INTERNED))
                      ? " pinned | interned"
                      : (flags & ATOM_PINNED) ? " pinned" : " interned",
                      fp);
            }
        }
        putc('\n', fp);
    }
    putc('\n', fp);
}
#endif

static JSHashNumber
js_hash_atom_ptr(const void *key)
{
    const JSAtom *atom = (const JSAtom *) key;
    return ATOM_HASH(atom);
}

#if JS_BITS_PER_WORD == 32
# define TEMP_SIZE_START_LOG2   5
#else
# define TEMP_SIZE_START_LOG2   6
#endif
#define TEMP_SIZE_LIMIT_LOG2    (TEMP_SIZE_START_LOG2 + NUM_TEMP_FREELISTS)

#define TEMP_SIZE_START         JS_BIT(TEMP_SIZE_START_LOG2)
#define TEMP_SIZE_LIMIT         JS_BIT(TEMP_SIZE_LIMIT_LOG2)

JS_STATIC_ASSERT(TEMP_SIZE_START >= sizeof(JSHashTable));

static void *
js_alloc_temp_space(void *priv, size_t size)
{
    Parser *parser = (Parser *) priv;

    void *space;
    if (size < TEMP_SIZE_LIMIT) {
        int bin = JS_CeilingLog2(size) - TEMP_SIZE_START_LOG2;
        JS_ASSERT(unsigned(bin) < NUM_TEMP_FREELISTS);

        space = parser->tempFreeList[bin];
        if (space) {
            parser->tempFreeList[bin] = *(void **)space;
            return space;
        }
    }

    JS_ARENA_ALLOCATE(space, &parser->context->tempPool, size);
    if (!space)
        js_ReportOutOfScriptQuota(parser->context);
    return space;
}

static void
js_free_temp_space(void *priv, void *item, size_t size)
{
    if (size >= TEMP_SIZE_LIMIT)
        return;

    Parser *parser = (Parser *) priv;
    int bin = JS_CeilingLog2(size) - TEMP_SIZE_START_LOG2;
    JS_ASSERT(unsigned(bin) < NUM_TEMP_FREELISTS);

    *(void **)item = parser->tempFreeList[bin];
    parser->tempFreeList[bin] = item;
}

static JSHashEntry *
js_alloc_temp_entry(void *priv, const void *key)
{
    Parser *parser = (Parser *) priv;
    JSAtomListElement *ale;

    ale = parser->aleFreeList;
    if (ale) {
        parser->aleFreeList = ALE_NEXT(ale);
        return &ale->entry;
    }

    JS_ARENA_ALLOCATE_TYPE(ale, JSAtomListElement, &parser->context->tempPool);
    if (!ale) {
        js_ReportOutOfScriptQuota(parser->context);
        return NULL;
    }
    return &ale->entry;
}

static void
js_free_temp_entry(void *priv, JSHashEntry *he, uintN flag)
{
    Parser *parser = (Parser *) priv;
    JSAtomListElement *ale = (JSAtomListElement *) he;

    ALE_SET_NEXT(ale, parser->aleFreeList);
    parser->aleFreeList = ale;
}

static JSHashAllocOps temp_alloc_ops = {
    js_alloc_temp_space,    js_free_temp_space,
    js_alloc_temp_entry,    js_free_temp_entry
};

JSAtomListElement *
JSAtomList::rawLookup(JSAtom *atom, JSHashEntry **&hep)
{
    if (table) {
        hep = JS_HashTableRawLookup(table, ATOM_HASH(atom), atom);
        return (JSAtomListElement *) *hep;
    }

    JSHashEntry **alep = &list;
    hep = NULL;
    JSAtomListElement *ale;
    while ((ale = (JSAtomListElement *)*alep) != NULL) {
        if (ALE_ATOM(ale) == atom) {
            /* Hit, move atom's element to the front of the list. */
            *alep = ale->entry.next;
            ale->entry.next = list;
            list = &ale->entry;
            break;
        }
        alep = &ale->entry.next;
    }
    return ale;
}

#define ATOM_LIST_HASH_THRESHOLD        12

JSAtomListElement *
JSAtomList::add(Parser *parser, JSAtom *atom, AddHow how)
{
    JS_ASSERT(!set);

    JSAtomListElement *ale, *ale2, *next;
    JSHashEntry **hep;

    ale = rawLookup(atom, hep);
    if (!ale || how != UNIQUE) {
        if (count < ATOM_LIST_HASH_THRESHOLD && !table) {
            /* Few enough for linear search and no hash table yet needed. */
            ale = (JSAtomListElement *)js_alloc_temp_entry(parser, atom);
            if (!ale)
                return NULL;
            ALE_SET_ATOM(ale, atom);

            if (how == HOIST) {
                ale->entry.next = NULL;
                hep = (JSHashEntry **) &list;
                while (*hep)
                    hep = &(*hep)->next;
                *hep = &ale->entry;
            } else {
                ale->entry.next = list;
                list = &ale->entry;
            }
        } else {
            /*
             * We should hash, or else we already are hashing, but count was
             * reduced by JSAtomList::rawRemove below ATOM_LIST_HASH_THRESHOLD.
             * Check whether we should create the table.
             */
            if (!table) {
                /* No hash table yet, so hep had better be null! */
                JS_ASSERT(!hep);
                table = JS_NewHashTable(count + 1, js_hash_atom_ptr,
                                        JS_CompareValues, JS_CompareValues,
                                        &temp_alloc_ops, parser);
                if (!table)
                    return NULL;

                /*
                 * Set ht->nentries explicitly, because we are moving entries
                 * from list to ht, not calling JS_HashTable(Raw|)Add.
                 */
                table->nentries = count;

                /*
                 * Insert each ale on list into the new hash table. Append to
                 * the hash chain rather than inserting at the bucket head, to
                 * preserve order among entries with the same key.
                 */
                for (ale2 = (JSAtomListElement *)list; ale2; ale2 = next) {
                    next = ALE_NEXT(ale2);
                    ale2->entry.keyHash = ATOM_HASH(ALE_ATOM(ale2));
                    hep = JS_HashTableRawLookup(table, ale2->entry.keyHash,
                                                ale2->entry.key);
                    while (*hep)
                        hep = &(*hep)->next;
                    *hep = &ale2->entry;
                    ale2->entry.next = NULL;
                }
                list = NULL;

                /* Set hep for insertion of atom's ale, immediately below. */
                hep = JS_HashTableRawLookup(table, ATOM_HASH(atom), atom);
            }

            /* Finally, add an entry for atom into the hash bucket at hep. */
            ale = (JSAtomListElement *)
                  JS_HashTableRawAdd(table, hep, ATOM_HASH(atom), atom, NULL);
            if (!ale)
                return NULL;

            /*
             * If hoisting, move ale to the end of its chain after we called
             * JS_HashTableRawAdd, since RawAdd may have grown the table and
             * then recomputed hep to refer to the pointer to the first entry
             * with the given key.
             */
            if (how == HOIST && ale->entry.next) {
                JS_ASSERT(*hep == &ale->entry);
                *hep = ale->entry.next;
                ale->entry.next = NULL;
                do {
                    hep = &(*hep)->next;
                } while (*hep);
                *hep = &ale->entry;
            }
        }

        ALE_SET_INDEX(ale, count++);
    }
    return ale;
}

void
JSAtomList::rawRemove(Parser *parser, JSAtomListElement *ale, JSHashEntry **hep)
{
    JS_ASSERT(!set);
    JS_ASSERT(count != 0);

    if (table) {
        JS_ASSERT(hep);
        JS_HashTableRawRemove(table, hep, &ale->entry);
    } else {
        JS_ASSERT(!hep);
        hep = &list;
        while (*hep != &ale->entry) {
            JS_ASSERT(*hep);
            hep = &(*hep)->next;
        }
        *hep = ale->entry.next;
        js_free_temp_entry(parser, &ale->entry, HT_FREE_ENTRY);
    }

    --count;
}

JSAutoAtomList::~JSAutoAtomList()
{
    if (table) {
        JS_HashTableDestroy(table);
    } else {
        JSHashEntry *hep = list;
        while (hep) {
            JSHashEntry *next = hep->next;
            js_free_temp_entry(parser, hep, HT_FREE_ENTRY);
            hep = next;
        }
    }
}

JSAtomListElement *
JSAtomListIterator::operator ()()
{
    JSAtomListElement *ale;
    JSHashTable *ht;

    if (index == uint32(-1))
        return NULL;

    ale = next;
    if (!ale) {
        ht = list->table;
        if (!ht)
            goto done;
        do {
            if (index == JS_BIT(JS_HASH_BITS - ht->shift))
                goto done;
            next = (JSAtomListElement *) ht->buckets[index++];
        } while (!next);
        ale = next;
    }

    next = ALE_NEXT(ale);
    return ale;

  done:
    index = uint32(-1);
    return NULL;
}

static intN
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

void
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
    al->clear();
}

#if JS_HAS_XML_SUPPORT
bool
js_InternNonIntElementIdSlow(JSContext *cx, JSObject *obj, const Value &idval,
                             jsid *idp)
{
    JS_ASSERT(idval.isObject());
    if (obj->isXML()) {
        *idp = OBJECT_TO_JSID(&idval.toObject());
        return true;
    }

    if (!js_IsFunctionQName(cx, &idval.toObject(), idp))
        return JS_FALSE;
    if (!JSID_IS_VOID(*idp))
        return true;

    return js_ValueToStringId(cx, idval, idp);
}

bool
js_InternNonIntElementIdSlow(JSContext *cx, JSObject *obj, const Value &idval,
                             jsid *idp, Value *vp)
{
    JS_ASSERT(idval.isObject());
    if (obj->isXML()) {
        JSObject &idobj = idval.toObject();
        *idp = OBJECT_TO_JSID(&idobj);
        vp->setObject(idobj);
        return true;
    }

    if (!js_IsFunctionQName(cx, &idval.toObject(), idp))
        return JS_FALSE;
    if (!JSID_IS_VOID(*idp)) {
        *vp = IdToValue(*idp);
        return true;
    }

    if (js_ValueToStringId(cx, idval, idp)) {
        vp->setString(JSID_TO_STRING(*idp));
        return true;
    }
    return false;
}
#endif
