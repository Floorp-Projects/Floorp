/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS atom table.
 */
#include "jsatominlines.h"

#include "mozilla/RangedPtr.h"
#include "mozilla/Util.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsstr.h"
#include "jstypes.h"

#include "gc/Marking.h"
#include "vm/Xdr.h"

#include "jscompartmentinlines.h"

#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;
using mozilla::ArrayLength;
using mozilla::RangedPtr;

const char *
js::AtomToPrintableString(ExclusiveContext *cx, JSAtom *atom, JSAutoByteString *bytes)
{
    // The only uses for this method when running off the main thread are for
    // parse errors/warnings, which will not actually be reported in such cases.
    if (!cx->isJSContext())
        return "";
    return js_ValueToPrintable(cx->asJSContext(), StringValue(atom), bytes);
}

const char * const js::TypeStrings[] = {
    js_undefined_str,
    js_object_str,
    js_function_str,
    js_string_str,
    js_number_str,
    js_boolean_str,
    js_null_str,
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
const char js_close_str[]           = "close";
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
const char js_send_str[]            = "send";
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

    FixedHeapPtr<PropertyName> *names = &cx->runtime()->firstCachedName;
    for (size_t i = 0; i < ArrayLength(cachedNames); i++, names++) {
        JSAtom *atom = Atomize(cx, cachedNames[i].str, cachedNames[i].length, InternAtom);
        if (!atom)
            return false;
        names->init(atom->asPropertyName());
    }
    JS_ASSERT(uintptr_t(names) == uintptr_t(&cx->runtime()->atomState + 1));

    cx->runtime()->emptyString = cx->names().empty;
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

    AtomSet::Ptr p = cx->runtime()->atoms.lookup(atom);
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
 * When the jschars reside in a freshly allocated buffer the memory can be used
 * as a new JSAtom's storage without copying. The contract is that the caller no
 * longer owns the memory and this method is responsible for freeing the memory.
 */
JS_ALWAYS_INLINE
static JSAtom *
AtomizeAndTakeOwnership(ExclusiveContext *cx, jschar *tbchars, size_t length, InternBehavior ib)
{
    JS_ASSERT(tbchars[length] == 0);

    if (JSAtom *s = cx->staticStrings().lookup(tbchars, length)) {
        js_free(tbchars);
        return s;
    }

    /*
     * If a GC occurs at js_NewStringCopy then |p| will still have the correct
     * hash, allowing us to avoid rehashing it. Even though the hash is
     * unchanged, we need to re-lookup the table position because a last-ditch
     * GC will potentially free some table entries.
     */
    AtomSet& atoms = cx->atoms();
    AtomSet::AddPtr p = atoms.lookupForAdd(AtomHasher::Lookup(tbchars, length));
    SkipRoot skipHash(cx, &p); /* Prevent the hash from being poisoned. */
    if (p) {
        JSAtom *atom = p->asPtr();
        p->setTagged(bool(ib));
        js_free(tbchars);
        return atom;
    }

    AutoCompartment ac(cx, cx->asJSContext()->runtime()->atomsCompartment);

    JSFlatString *flat = js_NewString<CanGC>(cx, tbchars, length);
    if (!flat) {
        js_free(tbchars);
        return NULL;
    }

    JSAtom *atom = flat->morphAtomizedStringIntoAtom();

    if (!atoms.relookupOrAdd(p, AtomHasher::Lookup(tbchars, length),
                             AtomStateEntry(atom, bool(ib)))) {
        js_ReportOutOfMemory(cx); /* SystemAllocPolicy does not report OOM. */
        return NULL;
    }

    return atom;
}

/* |tbchars| must not point into an inline or short string. */
template <AllowGC allowGC>
JS_ALWAYS_INLINE
static JSAtom *
AtomizeAndCopyChars(ExclusiveContext *cx, const jschar *tbchars, size_t length, InternBehavior ib)
{
    if (JSAtom *s = cx->staticStrings().lookup(tbchars, length))
         return s;

    /*
     * If a GC occurs at js_NewStringCopy then |p| will still have the correct
     * hash, allowing us to avoid rehashing it. Even though the hash is
     * unchanged, we need to re-lookup the table position because a last-ditch
     * GC will potentially free some table entries.
     */

    AtomSet& atoms = cx->atoms();
    AtomSet::AddPtr p = atoms.lookupForAdd(AtomHasher::Lookup(tbchars, length));
    SkipRoot skipHash(cx, &p); /* Prevent the hash from being poisoned. */
    if (p) {
        JSAtom *atom = p->asPtr();
        p->setTagged(bool(ib));
        return atom;
    }

    AutoCompartment ac(cx, cx->asJSContext()->runtime()->atomsCompartment);

    JSFlatString *flat = js_NewStringCopyN<allowGC>(cx, tbchars, length);
    if (!flat)
        return NULL;

    JSAtom *atom = flat->morphAtomizedStringIntoAtom();

    if (!atoms.relookupOrAdd(p, AtomHasher::Lookup(tbchars, length),
                             AtomStateEntry(atom, bool(ib)))) {
        if (allowGC)
            js_ReportOutOfMemory(cx); /* SystemAllocPolicy does not report OOM. */
        return NULL;
    }

    return atom;
}

template <AllowGC allowGC>
JSAtom *
js::AtomizeString(ExclusiveContext *cx, JSString *str,
                  js::InternBehavior ib /* = js::DoNotInternAtom */)
{
    if (str->isAtom()) {
        JSAtom &atom = str->asAtom();
        /* N.B. static atoms are effectively always interned. */
        if (ib != InternAtom || js::StaticStrings::isStatic(&atom))
            return &atom;

        AtomSet::Ptr p = cx->atoms().lookup(AtomHasher::Lookup(&atom));
        JS_ASSERT(p); /* Non-static atom must exist in atom state set. */
        JS_ASSERT(p->asPtr() == &atom);
        JS_ASSERT(ib == InternAtom);
        p->setTagged(bool(ib));
        return &atom;
    }

    const jschar *chars;
    if (str->isLinear()) {
        chars = str->asLinear().chars();
    } else {
        if (!cx->shouldBeJSContext())
            return NULL;
        chars = str->getChars(cx->asJSContext());
        if (!chars)
            return NULL;
    }

    if (JSAtom *atom = AtomizeAndCopyChars<NoGC>(cx, chars, str->length(), ib))
        return atom;

    if (!cx->isJSContext() || !allowGC)
        return NULL;

    JSLinearString *linear = str->ensureLinear(cx->asJSContext());
    if (!linear)
        return NULL;

    JS_ASSERT(linear->length() <= JSString::MAX_LENGTH);
    return AtomizeAndCopyChars<CanGC>(cx, linear->chars(), linear->length(), ib);
}

template JSAtom *
js::AtomizeString<CanGC>(ExclusiveContext *cx, JSString *str, InternBehavior ib);

template JSAtom *
js::AtomizeString<NoGC>(ExclusiveContext *cx, JSString *str, InternBehavior ib);

JSAtom *
js::Atomize(ExclusiveContext *cx, const char *bytes, size_t length, InternBehavior ib)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    static const unsigned ATOMIZE_BUF_MAX = 32;
    if (length < ATOMIZE_BUF_MAX) {
        /*
         * Avoiding the malloc in InflateString on shorter strings saves us
         * over 20,000 malloc calls on mozilla browser startup. This compares to
         * only 131 calls where the string is longer than a 31 char (net) buffer.
         * The vast majority of atomized strings are already in the hashtable. So
         * js::AtomizeString rarely has to copy the temp string we make.
         */
        jschar inflated[ATOMIZE_BUF_MAX];
        size_t inflatedLength = ATOMIZE_BUF_MAX - 1;
        if (!InflateStringToBuffer(cx->maybeJSContext(),
                                   bytes, length, inflated, &inflatedLength))
        {
            return NULL;
        }
        return AtomizeAndCopyChars<CanGC>(cx, inflated, inflatedLength, ib);
    }

    jschar *tbcharsZ = InflateString(cx, bytes, &length);
    if (!tbcharsZ)
        return NULL;
    return AtomizeAndTakeOwnership(cx, tbcharsZ, length, ib);
}

template <AllowGC allowGC>
JSAtom *
js::AtomizeChars(ExclusiveContext *cx, const jschar *chars, size_t length, InternBehavior ib)
{
    CHECK_REQUEST(cx);

    if (!JSString::validateLength(cx, length))
        return NULL;

    return AtomizeAndCopyChars<allowGC>(cx, chars, length, ib);
}

template JSAtom *
js::AtomizeChars<CanGC>(ExclusiveContext *cx,
                        const jschar *chars, size_t length, InternBehavior ib);

template JSAtom *
js::AtomizeChars<NoGC>(ExclusiveContext *cx,
                       const jschar *chars, size_t length, InternBehavior ib);

template <AllowGC allowGC>
bool
js::IndexToIdSlow(ExclusiveContext *cx, uint32_t index,
                  typename MaybeRooted<jsid, allowGC>::MutableHandleType idp)
{
    JS_ASSERT(index > JSID_INT_MAX);

    jschar buf[UINT32_CHAR_BUFFER_LENGTH];
    RangedPtr<jschar> end(ArrayEnd(buf), buf, ArrayEnd(buf));
    RangedPtr<jschar> start = BackfillIndexInCharBuffer(index, end);

    JSAtom *atom = AtomizeChars<allowGC>(cx, start.get(), end - start);
    if (!atom)
        return false;

    idp.set(JSID_FROM_BITS((size_t)atom));
    return true;
}

template bool
js::IndexToIdSlow<CanGC>(ExclusiveContext *cx, uint32_t index, MutableHandleId idp);

template bool
js::IndexToIdSlow<NoGC>(ExclusiveContext *cx, uint32_t index, FakeMutableHandle<jsid> idp);

template <AllowGC allowGC>
JSAtom *
js::ToAtom(ExclusiveContext *cx, typename MaybeRooted<Value, allowGC>::HandleType v)
{
    if (!v.isString()) {
        JSString *str = js::ToStringSlow<allowGC>(cx, v);
        if (!str)
            return NULL;
        JS::Anchor<JSString *> anchor(str);
        return AtomizeString<allowGC>(cx, str);
    }

    JSString *str = v.toString();
    if (str->isAtom())
        return &str->asAtom();

    JS::Anchor<JSString *> anchor(str);
    return AtomizeString<allowGC>(cx, str);
}

template JSAtom *
js::ToAtom<CanGC>(ExclusiveContext *cx, HandleValue v);

template JSAtom *
js::ToAtom<NoGC>(ExclusiveContext *cx, Value v);

template<XDRMode mode>
bool
js::XDRAtom(XDRState<mode> *xdr, MutableHandleAtom atomp)
{
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
    atom = AtomizeChars<CanGC>(cx, chars, nchars);
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
        chars = cx->runtime()->pod_malloc<jschar>(nchars);
        if (!chars)
            return false;
    }

    JS_ALWAYS_TRUE(xdr->codeChars(chars, nchars));
    atom = AtomizeChars<CanGC>(cx, chars, nchars);
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

