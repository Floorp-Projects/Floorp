/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_String_inl_h
#define vm_String_inl_h

#include "vm/String.h"

#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "gc/Marking.h"

#include "jsgcinlines.h"

namespace js {

template <AllowGC allowGC>
static JS_ALWAYS_INLINE JSInlineString *
NewShortString(ThreadSafeContext *cx, JS::Latin1Chars chars)
{
    size_t len = chars.length();
    JS_ASSERT(JSShortString::lengthFits(len));
    JSInlineString *str = JSInlineString::lengthFits(len)
                          ? JSInlineString::new_<allowGC>(cx)
                          : JSShortString::new_<allowGC>(cx);
    if (!str)
        return NULL;

    jschar *p = str->init(len);
    for (size_t i = 0; i < len; ++i)
        p[i] = static_cast<jschar>(chars[i]);
    p[len] = '\0';
    return str;
}

template <AllowGC allowGC>
static JS_ALWAYS_INLINE JSInlineString *
NewShortString(ExclusiveContext *cx, JS::StableTwoByteChars chars)
{
    size_t len = chars.length();

    /*
     * Don't bother trying to find a static atom; measurement shows that not
     * many get here (for one, Atomize is catching them).
     */
    JS_ASSERT(JSShortString::lengthFits(len));
    JSInlineString *str = JSInlineString::lengthFits(len)
                          ? JSInlineString::new_<allowGC>(cx)
                          : JSShortString::new_<allowGC>(cx);
    if (!str)
        return NULL;

    jschar *storage = str->init(len);
    mozilla::PodCopy(storage, chars.start().get(), len);
    storage[len] = 0;
    return str;
}

template <AllowGC allowGC>
static JS_ALWAYS_INLINE JSInlineString *
NewShortString(ExclusiveContext *cx, JS::TwoByteChars chars)
{
    size_t len = chars.length();

    /*
     * Don't bother trying to find a static atom; measurement shows that not
     * many get here (for one, Atomize is catching them).
     */
    JS_ASSERT(JSShortString::lengthFits(len));
    JSInlineString *str = JSInlineString::lengthFits(len)
                          ? JSInlineString::new_<NoGC>(cx)
                          : JSShortString::new_<NoGC>(cx);
    if (!str) {
        if (!allowGC)
            return NULL;
        jschar tmp[JSShortString::MAX_SHORT_LENGTH];
        mozilla::PodCopy(tmp, chars.start().get(), len);
        return NewShortString<CanGC>(cx, JS::StableTwoByteChars(tmp, len));
    }

    jschar *storage = str->init(len);
    mozilla::PodCopy(storage, chars.start().get(), len);
    storage[len] = 0;
    return str;
}

static inline void
StringWriteBarrierPost(js::ThreadSafeContext *maybecx, JSString **strp)
{
}

static inline void
StringWriteBarrierPostRemove(js::ThreadSafeContext *maybecx, JSString **strp)
{
}

} /* namespace js */

inline void
JSString::writeBarrierPre(JSString *str)
{
#ifdef JSGC_INCREMENTAL
    if (!str || !str->runtime()->needsBarrier())
        return;

    JS::Zone *zone = str->zone();
    if (zone->needsBarrier()) {
        JSString *tmp = str;
        MarkStringUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == str);
    }
#endif
}

inline bool
JSString::needWriteBarrierPre(JS::Zone *zone)
{
#ifdef JSGC_INCREMENTAL
    return zone->needsBarrier();
#else
    return false;
#endif
}

inline void
JSString::readBarrier(JSString *str)
{
#ifdef JSGC_INCREMENTAL
    JS::Zone *zone = str->zone();
    if (zone->needsBarrier()) {
        JSString *tmp = str;
        MarkStringUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == str);
    }
#endif
}

JS_ALWAYS_INLINE bool
JSString::validateLength(js::ThreadSafeContext *maybecx, size_t length)
{
    if (JS_UNLIKELY(length > JSString::MAX_LENGTH)) {
        js_ReportAllocationOverflow(maybecx);
        return false;
    }

    return true;
}

JS_ALWAYS_INLINE void
JSRope::init(js::ThreadSafeContext *cx, JSString *left, JSString *right, size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, ROPE_FLAGS);
    d.u1.left = left;
    d.s.u2.right = right;
    js::StringWriteBarrierPost(cx, &d.u1.left);
    js::StringWriteBarrierPost(cx, &d.s.u2.right);
}

template <js::AllowGC allowGC>
JS_ALWAYS_INLINE JSRope *
JSRope::new_(js::ThreadSafeContext *cx,
             typename js::MaybeRooted<JSString*, allowGC>::HandleType left,
             typename js::MaybeRooted<JSString*, allowGC>::HandleType right,
             size_t length)
{
    if (!validateLength(cx, length))
        return NULL;
    JSRope *str = (JSRope *) js_NewGCString<allowGC>(cx);
    if (!str)
        return NULL;
    str->init(cx, left, right, length);
    return str;
}

inline void
JSRope::markChildren(JSTracer *trc)
{
    js::gc::MarkStringUnbarriered(trc, &d.u1.left, "left child");
    js::gc::MarkStringUnbarriered(trc, &d.s.u2.right, "right child");
}

JS_ALWAYS_INLINE void
JSDependentString::init(js::ThreadSafeContext *cx, JSLinearString *base, const jschar *chars,
                        size_t length)
{
    JS_ASSERT(!js::IsPoisonedPtr(base));
    d.lengthAndFlags = buildLengthAndFlags(length, DEPENDENT_FLAGS);
    d.u1.chars = chars;
    d.s.u2.base = base;
    js::StringWriteBarrierPost(cx, reinterpret_cast<JSString **>(&d.s.u2.base));
}

JS_ALWAYS_INLINE JSLinearString *
JSDependentString::new_(js::ExclusiveContext *cx,
                        JSLinearString *baseArg, const jschar *chars, size_t length)
{
    /* Try to avoid long chains of dependent strings. */
    while (baseArg->isDependent())
        baseArg = baseArg->asDependent().base();

    JS_ASSERT(baseArg->isFlat());

    /*
     * The chars we are pointing into must be owned by something in the chain
     * of dependent or undepended strings kept alive by our base pointer.
     */
#ifdef DEBUG
    for (JSLinearString *b = baseArg; ; b = b->base()) {
        if (chars >= b->chars() && chars < b->chars() + b->length() &&
            length <= b->length() - (chars - b->chars()))
        {
            break;
        }
    }
#endif

    /*
     * Do not create a string dependent on inline chars from another string,
     * both to avoid the awkward moving-GC hazard this introduces and because it
     * is more efficient to immediately undepend here.
     */
    if (JSShortString::lengthFits(length))
        return js::NewShortString<js::CanGC>(cx, JS::TwoByteChars(chars, length));

    JSDependentString *str = (JSDependentString *)js_NewGCString<js::NoGC>(cx);
    if (str) {
        str->init(cx, baseArg, chars, length);
        return str;
    }

    JS::Rooted<JSLinearString*> base(cx, baseArg);

    str = (JSDependentString *)js_NewGCString<js::CanGC>(cx);
    if (!str)
        return NULL;
    str->init(cx, base, chars, length);
    return str;
}

inline void
JSString::markBase(JSTracer *trc)
{
    JS_ASSERT(hasBase());
    js::gc::MarkStringUnbarriered(trc, &d.s.u2.base, "base");
}

inline js::PropertyName *
JSFlatString::toPropertyName(JSContext *cx)
{
#ifdef DEBUG
    uint32_t dummy;
    JS_ASSERT(!isIndex(&dummy));
#endif
    if (isAtom())
        return asAtom().asPropertyName();
    JSAtom *atom = js::AtomizeString<js::CanGC>(cx, this);
    if (!atom)
        return NULL;
    return atom->asPropertyName();
}

JS_ALWAYS_INLINE void
JSStableString::init(const jschar *chars, size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    d.u1.chars = chars;
}

template <js::AllowGC allowGC>
JS_ALWAYS_INLINE JSStableString *
JSStableString::new_(js::ThreadSafeContext *cx, const jschar *chars, size_t length)
{
    JS_ASSERT(chars[length] == jschar(0));

    if (!validateLength(cx, length))
        return NULL;
    JSStableString *str = (JSStableString *)js_NewGCString<allowGC>(cx);
    if (!str)
        return NULL;
    str->init(chars, length);
    return str;
}

template <js::AllowGC allowGC>
JS_ALWAYS_INLINE JSInlineString *
JSInlineString::new_(js::ThreadSafeContext *cx)
{
    return (JSInlineString *)js_NewGCString<allowGC>(cx);
}

JS_ALWAYS_INLINE jschar *
JSInlineString::init(size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    d.u1.chars = d.inlineStorage;
    JS_ASSERT(lengthFits(length) || (isShort() && JSShortString::lengthFits(length)));
    return d.inlineStorage;
}

JS_ALWAYS_INLINE void
JSInlineString::resetLength(size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    JS_ASSERT(lengthFits(length) || (isShort() && JSShortString::lengthFits(length)));
}

template <js::AllowGC allowGC>
JS_ALWAYS_INLINE JSShortString *
JSShortString::new_(js::ThreadSafeContext *cx)
{
    return js_NewGCShortString<allowGC>(cx);
}

JS_ALWAYS_INLINE void
JSExternalString::init(const jschar *chars, size_t length, const JSStringFinalizer *fin)
{
    JS_ASSERT(fin);
    JS_ASSERT(fin->finalize);
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    d.u1.chars = chars;
    d.s.u2.externalFinalizer = fin;
}

JS_ALWAYS_INLINE JSExternalString *
JSExternalString::new_(JSContext *cx, const jschar *chars, size_t length,
                       const JSStringFinalizer *fin)
{
    JS_ASSERT(chars[length] == 0);

    if (!validateLength(cx, length))
        return NULL;
    JSExternalString *str = js_NewGCExternalString(cx);
    if (!str)
        return NULL;
    str->init(chars, length, fin);
    cx->runtime()->updateMallocCounter(cx->zone(), (length + 1) * sizeof(jschar));
    return str;
}

inline JSLinearString *
js::StaticStrings::getUnitStringForElement(JSContext *cx, JSString *str, size_t index)
{
    JS_ASSERT(index < str->length());

    jschar c;
    if (!str->getChar(cx, index, &c))
        return NULL;
    if (c < UNIT_STATIC_LIMIT)
        return getUnit(c);
    return js_NewDependentString(cx, str, index, 1);
}

inline JSAtom *
js::StaticStrings::getLength2(jschar c1, jschar c2)
{
    JS_ASSERT(fitsInSmallChar(c1));
    JS_ASSERT(fitsInSmallChar(c2));
    size_t index = (((size_t)toSmallChar[c1]) << 6) + toSmallChar[c2];
    return length2StaticTable[index];
}

JS_ALWAYS_INLINE void
JSString::finalize(js::FreeOp *fop)
{
    /* Shorts are in a different arena. */
    JS_ASSERT(getAllocKind() != js::gc::FINALIZE_SHORT_STRING);

    if (isFlat())
        asFlat().finalize(fop);
    else
        JS_ASSERT(isDependent() || isRope());
}

inline void
JSFlatString::finalize(js::FreeOp *fop)
{
    JS_ASSERT(getAllocKind() != js::gc::FINALIZE_SHORT_STRING);

    if (chars() != d.inlineStorage)
        fop->free_(const_cast<jschar *>(chars()));
}

inline void
JSShortString::finalize(js::FreeOp *fop)
{
    JS_ASSERT(getAllocKind() == js::gc::FINALIZE_SHORT_STRING);

    if (chars() != d.inlineStorage)
        fop->free_(const_cast<jschar *>(chars()));
}

inline void
JSAtom::finalize(js::FreeOp *fop)
{
    JS_ASSERT(JSString::isAtom());
    JS_ASSERT(JSString::isFlat());

    if (chars() != d.inlineStorage)
        fop->free_(const_cast<jschar *>(chars()));
}

inline void
JSExternalString::finalize(js::FreeOp *fop)
{
    const JSStringFinalizer *fin = externalFinalizer();
    fin->finalize(fin, const_cast<jschar *>(chars()));
}

#endif /* vm_String_inl_h */
