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
#include "vm/Xdr.h"

#include "jsstrinlines.h"
#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;
using mozilla::ArrayLength;
using mozilla::RangedPtr;

const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom, JSAutoByteString *bytes)
{
    return js_ValueToPrintable(cx, StringValue(atom), bytes);
}

const char * js::TypeStrings[] = {
    js_undefined_str,
    js_object_str,
    js_function_str,
    js_string_str,
    js_number_str,
    js_boolean_str,
    js_null_str,
    js_xml_str,
};

#define DEFINE_PROTO_STRING(name,code,init) const char js_##name##_str[] = #name;
JS_FOR_EACH_PROTOTYPE(DEFINE_PROTO_STRING)
#undef DEFINE_PROTO_STRING

#define CONST_CHAR_STR(idpart, id, text) const char js_##idpart##_str[] = text;
FOR_EACH_COMMON_PROPERTYNAME(CONST_CHAR_STR)
#undef CONST_CHAR_STR

/* Constant strings that are not atomized. */
const char js_break_str[]           = "break";
const char js_case_str[]            = "case";
const char js_catch_str[]           = "catch";
const char js_class_str[]           = "class";
const char js_const_str[]           = "const";
const char js_continue_str[]        = "continue";
const char js_debugger_str[]        = "debugger";
const char js_default_str[]         = "default";
const char js_do_str[]              = "do";
const char js_else_str[]            = "else";
const char js_enum_str[]            = "enum";
const char js_export_str[]          = "export";
const char js_extends_str[]         = "extends";
const char js_finally_str[]         = "finally";
const char js_for_str[]             = "for";
const char js_getter_str[]          = "getter";
const char js_if_str[]              = "if";
const char js_implements_str[]      = "implements";
const char js_import_str[]          = "import";
const char js_in_str[]              = "in";
const char js_instanceof_str[]      = "instanceof";
const char js_interface_str[]       = "interface";
const char js_let_str[]             = "let";
const char js_new_str[]             = "new";
const char js_package_str[]         = "package";
const char js_private_str[]         = "private";
const char js_protected_str[]       = "protected";
const char js_public_str[]          = "public";
const char js_setter_str[]          = "setter";
const char js_static_str[]          = "static";
const char js_super_str[]           = "super";
const char js_switch_str[]          = "switch";
const char js_this_str[]            = "this";
const char js_try_str[]             = "try";
const char js_typeof_str[]          = "typeof";
const char js_void_str[]            = "void";
const char js_while_str[]           = "while";
const char js_with_str[]            = "with";
const char js_yield_str[]           = "yield";
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
js::InitAtoms(JSRuntime *rt)
{
    return rt->atoms.init(JS_STRING_HASH_COUNT);
}

void
js::FinishAtoms(JSRuntime *rt)
{
    AtomSet &atoms = rt->atoms;
    if (!atoms.initialized()) {
        /*
         * We are called with uninitialized state when JS_NewRuntime fails and
         * calls JS_DestroyRuntime on a partially initialized runtime.
         */
        return;
    }

    FreeOp fop(rt, false);
    for (AtomSet::Range r = atoms.all(); !r.empty(); r.popFront())
        r.front().asPtr()->finalize(&fop);
}

struct CommonNameInfo
{
    const char *str;
    size_t length;
};

bool
js::InitCommonNames(JSContext *cx)
{
    static const CommonNameInfo cachedNames[] = {
#define COMMON_NAME_INFO(idpart, id, text) { js_##idpart##_str, sizeof(text) - 1 },
        FOR_EACH_COMMON_PROPERTYNAME(COMMON_NAME_INFO)
#undef COMMON_NAME_INFO
#define COMMON_NAME_INFO(name, code, init) { js_##name##_str, sizeof(#name) - 1 },
        JS_FOR_EACH_PROTOTYPE(COMMON_NAME_INFO)
#undef COMMON_NAME_INFO
    };

    FixedHeapPtr<PropertyName> *names = &cx->runtime->firstCachedName;
    for (size_t i = 0; i < ArrayLength(cachedNames); i++, names++) {
        JSAtom *atom = Atomize(cx, cachedNames[i].str, cachedNames[i].length, InternAtom);
        if (!atom)
            return false;
        names->init(atom->asPropertyName());
    }
    JS_ASSERT(uintptr_t(names) == uintptr_t(&cx->runtime->atomState + 1));

    cx->runtime->emptyString = cx->names().empty;
    return true;
}

void
js::FinishCommonNames(JSRuntime *rt)
{
    rt->emptyString = NULL;
#ifdef DEBUG
    memset(&rt->atomState, JS_FREE_PATTERN, sizeof(JSAtomState));
#endif
}

void
js::MarkAtoms(JSTracer *trc)
{
    JSRuntime *rt = trc->runtime;
    for (AtomSet::Range r = rt->atoms.all(); !r.empty(); r.popFront()) {
        AtomStateEntry entry = r.front();
        if (!entry.isTagged())
            continue;

        JSAtom *tmp = entry.asPtr();
        MarkStringRoot(trc, &tmp, "interned_atom");
        JS_ASSERT(tmp == entry.asPtr());
    }
}

void
js::SweepAtoms(JSRuntime *rt)
{
    for (AtomSet::Enum e(rt->atoms); !e.empty(); e.popFront()) {
        AtomStateEntry entry = e.front();
        JSAtom *atom = entry.asPtr();
        bool isDying = IsStringAboutToBeFinalized(&atom);

        /* Pinned or interned key cannot be finalized. */
        JS_ASSERT_IF(entry.isTagged(), !isDying);

        if (isDying)
            e.removeFront();
    }
}

bool
AtomIsInterned(JSContext *cx, JSAtom *atom)
{
    /* We treat static strings as interned because they're never collected. */
    if (StaticStrings::isStatic(atom))
        return true;

    AtomSet::Ptr p = cx->runtime->atoms.lookup(atom);
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

    AtomSet &atoms = cx->runtime->atoms;
    AtomSet::AddPtr p = atoms.lookupForAdd(AtomHasher::Lookup(chars, length));

    if (p) {
        JSAtom *atom = p->asPtr();
        p->setTagged(bool(ib));
        return atom;
    }

    AutoEnterAtomsCompartment ac(cx);

    SkipRoot skip(cx, &chars);

    /* Workaround for hash values in AddPtr being inadvertently poisoned. */
    SkipRoot skip2(cx, &p);

    JSFlatString *key;
    if (ocb == TakeCharOwnership) {
        key = js_NewString(cx, const_cast<jschar *>(chars), length);
        *pchars = NULL; /* Called should not free *pchars. */
    } else {
        JS_ASSERT(ocb == CopyChars);
        key = js_NewStringCopyN(cx, chars, length);
    }
    if (!key)
        return NULL;

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

JSAtom *
js::AtomizeString(JSContext *cx, JSString *str, InternBehavior ib)
{
    if (str->isAtom()) {
        JSAtom &atom = str->asAtom();
        /* N.B. static atoms are effectively always interned. */
        if (ib != InternAtom || js::StaticStrings::isStatic(&atom))
            return &atom;

        AtomSet::Ptr p = cx->runtime->atoms.lookup(AtomHasher::Lookup(&atom));
        JS_ASSERT(p); /* Non-static atom must exist in atom state set. */
        JS_ASSERT(p->asPtr() == &atom);
        JS_ASSERT(ib == InternAtom);
        p->setTagged(bool(ib));
        return &atom;
    }

    JSStableString *stable = str->ensureStable(cx);
    if (!stable)
        return NULL;

    const jschar *chars = stable->chars().get();
    size_t length = stable->length();
    JS_ASSERT(length <= JSString::MAX_LENGTH);
    return AtomizeInline(cx, &chars, length, ib);
}

JSAtom *
js::Atomize(JSContext *cx, const char *bytes, size_t length, InternBehavior ib)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    /*
     * Avoiding the malloc in InflateString on shorter strings saves us
     * over 20,000 malloc calls on mozilla browser startup. This compares to
     * only 131 calls where the string is longer than a 31 char (net) buffer.
     * The vast majority of atomized strings are already in the hashtable. So
     * js::AtomizeString rarely has to copy the temp string we make.
     */
    static const unsigned ATOMIZE_BUF_MAX = 32;
    jschar inflated[ATOMIZE_BUF_MAX];
    size_t inflatedLength = ATOMIZE_BUF_MAX - 1;

    const jschar *chars;
    OwnCharsBehavior ocb = CopyChars;
    if (length < ATOMIZE_BUF_MAX) {
        InflateStringToBuffer(cx, bytes, length, inflated, &inflatedLength);
        inflated[inflatedLength] = 0;
        chars = inflated;
    } else {
        inflatedLength = length;
        chars = InflateString(cx, bytes, &inflatedLength);
        if (!chars)
            return NULL;
        ocb = TakeCharOwnership;
    }

    JSAtom *atom = AtomizeInline(cx, &chars, inflatedLength, ib, ocb);
    if (ocb == TakeCharOwnership && chars)
        js_free((void *)chars);
    return atom;
}

JSAtom *
js::AtomizeChars(JSContext *cx, const jschar *chars, size_t length, InternBehavior ib)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    return AtomizeInline(cx, &chars, length, ib);
}

bool
js::IndexToIdSlow(JSContext *cx, uint32_t index, jsid *idp)
{
    JS_ASSERT(index > JSID_INT_MAX);

    jschar buf[UINT32_CHAR_BUFFER_LENGTH];
    RangedPtr<jschar> end(ArrayEnd(buf), buf, ArrayEnd(buf));
    RangedPtr<jschar> start = BackfillIndexInCharBuffer(index, end);

    JSAtom *atom = AtomizeChars(cx, start.get(), end - start);
    if (!atom)
        return false;

    *idp = JSID_FROM_BITS((size_t)atom);
    return true;
}

bool
js::InternNonIntElementId(JSContext *cx, JSObject *obj, const Value &idval,
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

template<XDRMode mode>
bool
js::XDRAtom(XDRState<mode> *xdr, MutableHandleAtom atomp)
{
    AssertCanGC();
    if (mode == XDR_ENCODE) {
        uint32_t nchars = atomp->length();
        if (!xdr->codeUint32(&nchars))
            return false;

        jschar *chars = const_cast<jschar *>(atomp->getChars(xdr->cx()));
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
    atom = AtomizeChars(cx, chars, nchars);
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
        chars = cx->runtime->pod_malloc<jschar>(nchars);
        if (!chars)
            return false;
    }

    JS_ALWAYS_TRUE(xdr->codeChars(chars, nchars));
    atom = AtomizeChars(cx, chars, nchars);
    if (chars != stackChars)
        js_free(chars);
#endif /* !IS_LITTLE_ENDIAN */

    if (!atom)
        return false;
    atomp.set(atom);
    return true;
}

template bool
js::XDRAtom(XDRState<XDR_ENCODE> *xdr, MutableHandleAtom atomp);

template bool
js::XDRAtom(XDRState<XDR_DECODE> *xdr, MutableHandleAtom atomp);

