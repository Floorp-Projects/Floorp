/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "jslock.h"
#include "jsnum.h"
#include "jsstr.h"
#include "jsversion.h"
#include "jsxml.h"

#include "frontend/Parser.h"
#include "gc/Marking.h"

#include "jsstrinlines.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/String-inl.h"
#include "vm/Xdr.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;

const size_t JSAtomState::commonAtomsOffset = offsetof(JSAtomState, emptyAtom);

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
    js_null_str                 /* nullAtom                     */

#define JS_PROTO(name,code,init) ,js_##name##_str
#include "jsproto.tbl"
#undef JS_PROTO

#define DEFINE_ATOM(id, text)          ,js_##id##_str
#define DEFINE_PROTOTYPE_ATOM(id)      ,js_##id##_str
#define DEFINE_KEYWORD_ATOM(id)        ,js_##id##_str
#include "jsatom.tbl"
#undef DEFINE_ATOM
#undef DEFINE_PROTOTYPE_ATOM
#undef DEFINE_KEYWORD_ATOM
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
}

/*
 * Interpreter macros called by the trace recorder assume common atom indexes
 * fit in one byte of immediate operand.
 */
JS_STATIC_ASSERT(JS_ARRAY_LENGTH(js_common_atom_names) < 256);

const size_t js_common_atom_count = JS_ARRAY_LENGTH(js_common_atom_names);

const char js_undefined_str[]       = "undefined";
const char js_object_str[]          = "object";

#define DEFINE_ATOM(id, text)          const char js_##id##_str[] = text;
#define DEFINE_PROTOTYPE_ATOM(id)
#define DEFINE_KEYWORD_ATOM(id)
#include "jsatom.tbl"
#undef DEFINE_ATOM
#undef DEFINE_PROTOTYPE_ATOM
#undef DEFINE_KEYWORD_ATOM

#if JS_HAS_GENERATORS
const char js_close_str[]           = "close";
const char js_send_str[]            = "send";
#endif

/* Constant strings that are not atomized. */
const char js_getter_str[]          = "getter";
const char js_setter_str[]          = "setter";

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

    FreeOp fop(rt, false, false);
    for (AtomSet::Range r = state->atoms.all(); !r.empty(); r.popFront())
        r.front().asPtr()->finalize(&fop);
}

bool
js::InitCommonAtoms(JSContext *cx)
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

    cx->runtime->emptyString = state->emptyAtom;
    return true;
}

void
js::FinishCommonAtoms(JSRuntime *rt)
{
    rt->emptyString = NULL;
    rt->atomState.junkAtoms();
}

void
js::MarkAtomState(JSTracer *trc, bool markAll)
{
    JSRuntime *rt = trc->runtime;
    JSAtomState *state = &rt->atomState;

    if (markAll) {
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
js::SweepAtomState(JSRuntime *rt)
{
    JSAtomState *state = &rt->atomState;

    for (AtomSet::Enum e(state->atoms); !e.empty(); e.popFront()) {
        AtomStateEntry entry = e.front();
        JSAtom *atom = entry.asPtr();
        bool isMarked = IsStringMarked(&atom);

        /* Pinned or interned key cannot be finalized. */
        JS_ASSERT_IF(entry.isTagged(), isMarked);

        if (!isMarked)
            e.removeFront();
        else
            e.rekeyFront(AtomHasher::Lookup(atom), AtomStateEntry(atom, entry.isTagged()));
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

    SkipRoot skip(cx, &chars);

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

    *idp = JSID_FROM_BITS((size_t)atom);
    return true;
}

bool
InternNonIntElementId(JSContext *cx, JSObject *obj, const Value &idval,
                      jsid *idp, MutableHandleValue vp)
{
#if JS_HAS_XML_SUPPORT
    if (idval.isObject()) {
        JSObject *idobj = &idval.toObject();

        if (obj && obj->isXML()) {
            *idp = OBJECT_TO_JSID(idobj);
            vp.set(idval);
            return true;
        }

        if (js_GetLocalNameFromFunctionQName(idobj, idp, cx)) {
            vp.set(IdToValue(*idp));
            return true;
        }

        if (!obj && idobj->isXMLId()) {
            *idp = OBJECT_TO_JSID(idobj);
            vp.set(idval);
            return JS_TRUE;
        }
    }
#endif

    JSAtom *atom = ToAtom(cx, idval);
    if (!atom)
        return false;

    *idp = AtomToId(atom);
    vp.setString(atom);
    return true;
}

} /* namespace js */

template<XDRMode mode>
bool
js::XDRAtom(XDRState<mode> *xdr, JSAtom **atomp)
{
    if (mode == XDR_ENCODE) {
        uint32_t nchars = (*atomp)->length();
        if (!xdr->codeUint32(&nchars))
            return false;

        jschar *chars = const_cast<jschar *>((*atomp)->getChars(xdr->cx()));
        if (!chars)
            return false;

        return xdr->codeChars(chars, nchars);
    }

    /* Avoid JSString allocation for already existing atoms. See bug 321985. */
    uint32_t nchars;
    if (!xdr->codeUint32(&nchars))
        return false;

    JSContext *cx = xdr->cx();
    JSAtom *atom;
#if IS_LITTLE_ENDIAN
    /* Directly access the little endian chars in the XDR buffer. */
    const jschar *chars = reinterpret_cast<const jschar *>(xdr->buf.read(nchars * sizeof(jschar)));
    atom = js_AtomizeChars(cx, chars, nchars);
#else
    /*
     * We must copy chars to a temporary buffer to convert between little and
     * big endian data.
     */
    jschar *chars;
    jschar stackChars[256];
    if (nchars <= ArrayLength(stackChars)) {
        chars = stackChars;
    } else {
        /*
         * This is very uncommon. Don't use the tempLifoAlloc arena for this as
         * most allocations here will be bigger than tempLifoAlloc's default
         * chunk size.
         */
        chars = static_cast<jschar *>(cx->runtime->malloc_(nchars * sizeof(jschar)));
        if (!chars)
            return false;
    }

    JS_ALWAYS_TRUE(xdr->codeChars(chars, nchars));
    atom = js_AtomizeChars(cx, chars, nchars);
    if (chars != stackChars)
        Foreground::free_(chars);
#endif /* !IS_LITTLE_ENDIAN */

    if (!atom)
        return false;
    *atomp = atom;
    return true;
}

template bool
js::XDRAtom(XDRState<XDR_ENCODE> *xdr, JSAtom **atomp);

template bool
js::XDRAtom(XDRState<XDR_DECODE> *xdr, JSAtom **atomp);

