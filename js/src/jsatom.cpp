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

#include "mozilla/RangedPtr.h"
#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsstr.h"
#include "jsversion.h"
#include "jsxml.h"

#include "frontend/Parser.h"

#include "jsstrinlines.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/String-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;

const size_t JSAtomState::commonAtomsOffset = offsetof(JSAtomState, emptyAtom);
const size_t JSAtomState::lazyAtomsOffset = offsetof(JSAtomState, lazy);

/*
 * ATOM_HASH assumes that JSHashNumber is 32-bit even on 64-bit systems.
 */
JS_STATIC_ASSERT(sizeof(JSHashNumber) == 4);
JS_STATIC_ASSERT(sizeof(JSAtom *) == JS_BYTES_PER_WORD);

const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom, JSAutoByteString *bytes)
{
    return js_ValueToPrintable(cx, StringValue(atom), bytes);
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
    js_BYTES_PER_ELEMENT_str,   /* BYTES_PER_ELEMENTAtom        */
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
    "[object Null]",            /* objectNullAtom               */
    "[object Undefined]",       /* objectUndefinedAtom          */
    "of",                       /* ofAtom                       */
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
    "loc",                      /* locAtom                      */
    "line",                     /* lineAtom                     */
    "Infinity",                 /* InfinityAtom                 */
    "NaN",                      /* NaNAtom                      */
    "builder",                  /* builderAtom                  */

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
    "keys",                     /* keysAtom                     */
    "iterate",                  /* iterateAtom                  */

    "WeakMap",                  /* WeakMapAtom                  */

    "byteLength",               /* byteLengthAtom               */

    "return",                   /* returnAtom                   */
    "throw"                     /* throwAtom                    */
};

void
JSAtomState::checkStaticInvariants()
{
    /*
     * Start and limit offsets for atom pointers in JSAtomState must be aligned
     * on the word boundary.
     */
    JS_STATIC_ASSERT(commonAtomsOffset % sizeof(JSAtom *) == 0);
    JS_STATIC_ASSERT(sizeof(*this) % sizeof(JSAtom *) == 0);

    /*
     * JS_BOOLEAN_STR and JS_TYPE_STR assume that boolean names starts from the
     * index 1 and type name starts from the index 1+2 atoms in JSAtomState.
     */
    JS_STATIC_ASSERT(1 * sizeof(JSAtom *) ==
                     offsetof(JSAtomState, booleanAtoms) - commonAtomsOffset);
    JS_STATIC_ASSERT((1 + 2) * sizeof(JSAtom *) ==
                     offsetof(JSAtomState, typeAtoms) - commonAtomsOffset);

    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(js_common_atom_names) * sizeof(JSAtom *) ==
                     lazyAtomsOffset - commonAtomsOffset);
}

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
const char js_BYTES_PER_ELEMENT_str[] = "BYTES_PER_ELEMENT";
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

    JS_ASSERT(state->atoms.initialized());
    return true;
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

    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront())
        r.front().asPtr()->finalize(rt);
}

bool
js_InitCommonAtoms(JSContext *cx)
{
    JSAtomState *state = &cx->runtime->atomState;
    JSAtom **atoms = state->commonAtomsStart();
    for (size_t i = 0; i < ArrayLength(js_common_atom_names); i++, atoms++) {
        JSAtom *atom = js_Atomize(cx, js_common_atom_names[i], strlen(js_common_atom_names[i]),
                                  InternAtom);
        if (!atom)
            return false;
        *atoms = atom->asPropertyName();
    }

    state->clearLazyAtoms();
    cx->runtime->emptyString = state->emptyAtom;
    return true;
}

void
js_FinishCommonAtoms(JSContext *cx)
{
    cx->runtime->emptyString = NULL;
    cx->runtime->atomState.junkAtoms();
}

void
js_TraceAtomState(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    JSAtomState *state = &rt->atomState;

    if (rt->gcKeepAtoms) {
        for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
            JSAtom *tmp = r.front().asPtr();
            MarkStringRoot(trc, &tmp, "locked_atom");
            JS_ASSERT(tmp == r.front().asPtr());
        }
    } else {
        for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
            AtomStateEntry entry = r.front();
            if (!entry.isTagged())
                continue;

            JSAtom *tmp = entry.asPtr();
            MarkStringRoot(trc, &tmp, "interned_atom");
            JS_ASSERT(tmp == entry.asPtr());
        }
    }
}

void
js_SweepAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

    for (AtomSet::Enum e(state->atoms); !e.empty(); e.popFront()) {
        AtomStateEntry entry = e.front();

        if (entry.isTagged()) {
            /* Pinned or interned key cannot be finalized. */
            JS_ASSERT(!IsAboutToBeFinalized(entry.asPtr()));
            continue;
        }

        if (IsAboutToBeFinalized(entry.asPtr()))
            e.removeFront();
    }
}

bool
AtomIsInterned(JSContext *cx, JSAtom *atom)
{
    /* We treat static strings as interned because they're never collected. */
    if (StaticStrings::isStatic(atom))
        return true;

    AtomSet::Ptr p = cx->runtime->atomState.atoms.lookup(atom);
    if (!p)
        return false;

    return p->isTagged();
}

enum OwnCharsBehavior
{
    CopyChars, /* in other words, do not take ownership */
    TakeCharOwnership
};

/*
 * Callers passing OwnChars have freshly allocated *pchars and thus this
 * memory can be used as a new JSAtom's buffer without copying. When this flag
 * is set, the contract is that callers will free *pchars iff *pchars == NULL.
 */
JS_ALWAYS_INLINE
static JSAtom *
AtomizeInline(JSContext *cx, const jschar **pchars, size_t length,
              InternBehavior ib, OwnCharsBehavior ocb = CopyChars)
{
    const jschar *chars = *pchars;

    if (JSAtom *s = cx->runtime->staticStrings.lookup(chars, length))
        return s;

    AtomSet &atoms = cx->runtime->atomState.atoms;
    AtomSet::AddPtr p = atoms.lookupForAdd(AtomHasher::Lookup(chars, length));

    if (p) {
        JSAtom *atom = p->asPtr();
        p->setTagged(bool(ib));
        return atom;
    }

    SwitchToCompartment sc(cx, cx->runtime->atomsCompartment);

    JSFixedString *key;

    if (ocb == TakeCharOwnership) {
        key = js_NewString(cx, const_cast<jschar *>(chars), length);
        if (!key)
            return NULL;
        *pchars = NULL; /* Called should not free *pchars. */
    } else {
        JS_ASSERT(ocb == CopyChars);
        key = js_NewStringCopyN(cx, chars, length);
        if (!key)
            return NULL;
    }

    /*
     * We have to relookup the key as the last ditch GC invoked from the
     * string allocation or OOM handling unlocks the atomsCompartment.
     *
     * N.B. this avoids recomputing the hash but still has a potential
     * (# collisions * # chars) comparison cost in the case of a hash
     * collision!
     */
    AtomHasher::Lookup lookup(chars, length);
    if (!atoms.relookupOrAdd(p, lookup, AtomStateEntry((JSAtom *) key, bool(ib)))) {
        JS_ReportOutOfMemory(cx); /* SystemAllocPolicy does not report */
        return NULL;
    }

    return key->morphAtomizedStringIntoAtom();
}

static JSAtom *
Atomize(JSContext *cx, const jschar **pchars, size_t length,
        InternBehavior ib, OwnCharsBehavior ocb = CopyChars)
{
    return AtomizeInline(cx, pchars, length, ib, ocb);
}

JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, InternBehavior ib)
{
    if (str->isAtom()) {
        JSAtom &atom = str->asAtom();
        /* N.B. static atoms are effectively always interned. */
        if (ib != InternAtom || js::StaticStrings::isStatic(&atom))
            return &atom;

        AtomSet &atoms = cx->runtime->atomState.atoms;
        AtomSet::Ptr p = atoms.lookup(AtomHasher::Lookup(&atom));
        JS_ASSERT(p); /* Non-static atom must exist in atom state set. */
        JS_ASSERT(p->asPtr() == &atom);
        JS_ASSERT(ib == InternAtom);
        p->setTagged(bool(ib));
        return &atom;
    }

    if (str->isAtom())
        return &str->asAtom();

    size_t length = str->length();
    const jschar *chars = str->getChars(cx);
    if (!chars)
        return NULL;

    JS_ASSERT(length <= JSString::MAX_LENGTH);
    return Atomize(cx, &chars, length, ib);
}

JSAtom *
js_Atomize(JSContext *cx, const char *bytes, size_t length, InternBehavior ib, FlationCoding fc)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    /*
     * Avoiding the malloc in InflateString on shorter strings saves us
     * over 20,000 malloc calls on mozilla browser startup. This compares to
     * only 131 calls where the string is longer than a 31 char (net) buffer.
     * The vast majority of atomized strings are already in the hashtable. So
     * js_AtomizeString rarely has to copy the temp string we make.
     */
    static const unsigned ATOMIZE_BUF_MAX = 32;
    jschar inflated[ATOMIZE_BUF_MAX];
    size_t inflatedLength = ATOMIZE_BUF_MAX - 1;

    const jschar *chars;
    OwnCharsBehavior ocb = CopyChars;
    if (length < ATOMIZE_BUF_MAX) {
        if (fc == CESU8Encoding)
            InflateUTF8StringToBuffer(cx, bytes, length, inflated, &inflatedLength, fc);
        else
            InflateStringToBuffer(cx, bytes, length, inflated, &inflatedLength);
        inflated[inflatedLength] = 0;
        chars = inflated;
    } else {
        inflatedLength = length;
        chars = InflateString(cx, bytes, &inflatedLength, fc);
        if (!chars)
            return NULL;
        ocb = TakeCharOwnership;
    }

    JSAtom *atom = Atomize(cx, &chars, inflatedLength, ib, ocb);
    if (ocb == TakeCharOwnership && chars)
        cx->free_((void *)chars);
    return atom;
}

JSAtom *
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length, InternBehavior ib)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    return AtomizeInline(cx, &chars, length, ib);
}

JSAtom *
js_GetExistingStringAtom(JSContext *cx, const jschar *chars, size_t length)
{
    if (JSAtom *atom = cx->runtime->staticStrings.lookup(chars, length))
        return atom;
    if (AtomSet::Ptr p = cx->runtime->atomState.atoms.lookup(AtomHasher::Lookup(chars, length)))
        return p->asPtr();
    return NULL;
}

#ifdef DEBUG
JS_FRIEND_API(void)
js_DumpAtoms(JSContext *cx, FILE *fp)
{
    JSAtomState *state = &cx->runtime->atomState;

    fprintf(fp, "atoms table contents:\n");
    unsigned number = 0;
    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront()) {
        AtomStateEntry entry = r.front();
        fprintf(fp, "%3u ", number++);
        JSAtom *key = entry.asPtr();
        FileEscapedString(fp, key, '"');
        if (entry.isTagged())
            fputs(" interned", fp);
        putc('\n', fp);
    }
    putc('\n', fp);
}
#endif

#if JS_BITS_PER_WORD == 32
# define TEMP_SIZE_START_LOG2   5
#else
# define TEMP_SIZE_START_LOG2   6
#endif
#define TEMP_SIZE_LIMIT_LOG2    (TEMP_SIZE_START_LOG2 + NUM_TEMP_FREELISTS)

#define TEMP_SIZE_START         JS_BIT(TEMP_SIZE_START_LOG2)
#define TEMP_SIZE_LIMIT         JS_BIT(TEMP_SIZE_LIMIT_LOG2)

JS_STATIC_ASSERT(TEMP_SIZE_START >= sizeof(JSHashTable));

void
js_InitAtomMap(JSContext *cx, AtomIndexMap *indices, JSAtom **atoms)
{
    if (indices->isMap()) {
        typedef AtomIndexMap::WordMap WordMap;
        const WordMap &wm = indices->asMap();
        for (WordMap::Range r = wm.all(); !r.empty(); r.popFront()) {
            JSAtom *atom = r.front().key;
            jsatomid index = r.front().value;
            JS_ASSERT(index < indices->count());
            atoms[index] = atom;
        }
    } else {
        for (const AtomIndexMap::InlineElem *it = indices->asInline(), *end = indices->inlineEnd();
             it != end; ++it) {
            JSAtom *atom = it->key;
            if (!atom)
                continue;
            JS_ASSERT(it->value < indices->count());
            atoms[it->value] = atom;
        }
    }
}

namespace js {

bool
IndexToIdSlow(JSContext *cx, uint32_t index, jsid *idp)
{
    JS_ASSERT(index > JSID_INT_MAX);

    jschar buf[UINT32_CHAR_BUFFER_LENGTH];
    RangedPtr<jschar> end(ArrayEnd(buf), buf, ArrayEnd(buf));
    RangedPtr<jschar> start = BackfillIndexInCharBuffer(index, end);

    JSAtom *atom = js_AtomizeChars(cx, start.get(), end - start);
    if (!atom)
        return false;

    *idp = ATOM_TO_JSID(atom);
    JS_ASSERT(js_CheckForStringIndex(*idp) == *idp);
    return true;
}

} /* namespace js */

/* JSBOXEDWORD_INT_MAX as a string */
#define JSBOXEDWORD_INT_MAX_STRING "1073741823"

/*
 * Convert string indexes that convert to int jsvals as ints to save memory.
 * Care must be taken to use this macro every time a property name is used, or
 * else double-sets, incorrect property cache misses, or other mistakes could
 * occur.
 */
jsid
js_CheckForStringIndex(jsid id)
{
    if (!JSID_IS_ATOM(id))
        return id;

    JSAtom *atom = JSID_TO_ATOM(id);
    const jschar *s = atom->chars();
    jschar ch = *s;

    JSBool negative = (ch == '-');
    if (negative)
        ch = *++s;

    if (!JS7_ISDEC(ch))
        return id;

    size_t n = atom->length() - negative;
    if (n > sizeof(JSBOXEDWORD_INT_MAX_STRING) - 1)
        return id;

    const jschar *cp = s;
    const jschar *end = s + n;

    uint32_t index = JS7_UNDEC(*cp++);
    uint32_t oldIndex = 0;
    uint32_t c = 0;

    if (index != 0) {
        while (JS7_ISDEC(*cp)) {
            oldIndex = index;
            c = JS7_UNDEC(*cp);
            index = 10 * index + c;
            cp++;
        }
    }

    /*
     * Non-integer indexes can't be represented as integers.  Also, distinguish
     * index "-0" from "0", because JSBOXEDWORD_INT cannot.
     */
    if (cp != end || (negative && index == 0))
        return id;

    if (negative) {
        if (oldIndex < -(JSID_INT_MIN / 10) ||
            (oldIndex == -(JSID_INT_MIN / 10) && c <= (-JSID_INT_MIN % 10)))
        {
            id = INT_TO_JSID(-int32_t(index));
        }
    } else {
        if (oldIndex < JSID_INT_MAX / 10 ||
            (oldIndex == JSID_INT_MAX / 10 && c <= (JSID_INT_MAX % 10)))
        {
            id = INT_TO_JSID(int32_t(index));
        }
    }

    return id;
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

    if (js_GetLocalNameFromFunctionQName(&idval.toObject(), idp, cx))
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

    if (js_GetLocalNameFromFunctionQName(&idval.toObject(), idp, cx)) {
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
