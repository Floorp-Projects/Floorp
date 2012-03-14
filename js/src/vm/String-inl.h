/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Luke Wagner <luke@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef String_inl_h__
#define String_inl_h__

#include "jscntxt.h"
#include "jsgcmark.h"
#include "jsprobes.h"

#include "String.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "gc/Barrier-inl.h"

inline void
JSString::writeBarrierPre(JSString *str)
{
#ifdef JSGC_INCREMENTAL
    if (!str)
        return;

    JSCompartment *comp = str->compartment();
    if (comp->needsBarrier()) {
        JSString *tmp = str;
        MarkStringUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == str);
    }
#endif
}

inline void
JSString::writeBarrierPost(JSString *str, void *addr)
{
}

inline bool
JSString::needWriteBarrierPre(JSCompartment *comp)
{
#ifdef JSGC_INCREMENTAL
    return comp->needsBarrier();
#else
    return false;
#endif
}

inline void
JSString::readBarrier(JSString *str)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = str->compartment();
    if (comp->needsBarrier()) {
        JSString *tmp = str;
        MarkStringUnbarriered(comp->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == str);
    }
#endif
}

JS_ALWAYS_INLINE bool
JSString::validateLength(JSContext *cx, size_t length)
{
    if (JS_UNLIKELY(length > JSString::MAX_LENGTH)) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    return true;
}

JS_ALWAYS_INLINE void
JSRope::init(JSString *left, JSString *right, size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, ROPE_BIT);
    d.u1.left = left;
    d.s.u2.right = right;
    JSString::writeBarrierPost(d.u1.left, &d.u1.left);
    JSString::writeBarrierPost(d.s.u2.right, &d.s.u2.right);
}

JS_ALWAYS_INLINE JSRope *
JSRope::new_(JSContext *cx, JSString *left, JSString *right, size_t length)
{
    if (!validateLength(cx, length))
        return NULL;
    JSRope *str = (JSRope *)js_NewGCString(cx);
    if (!str)
        return NULL;
    str->init(left, right, length);
    return str;
}

inline void
JSRope::markChildren(JSTracer *trc)
{
    js::gc::MarkStringUnbarriered(trc, &d.u1.left, "left child");
    js::gc::MarkStringUnbarriered(trc, &d.s.u2.right, "right child");
}

JS_ALWAYS_INLINE void
JSDependentString::init(JSLinearString *base, const jschar *chars, size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, DEPENDENT_BIT);
    d.u1.chars = chars;
    d.s.u2.base = base;
    JSString::writeBarrierPost(d.s.u2.base, &d.s.u2.base);
}

JS_ALWAYS_INLINE JSDependentString *
JSDependentString::new_(JSContext *cx, JSLinearString *base, const jschar *chars, size_t length)
{
    /* Try to avoid long chains of dependent strings. */
    while (base->isDependent())
        base = base->asDependent().base();

    JS_ASSERT(base->isFlat());
    JS_ASSERT(chars >= base->chars() && chars < base->chars() + base->length());
    JS_ASSERT(length <= base->length() - (chars - base->chars()));

    JSDependentString *str = (JSDependentString *)js_NewGCString(cx);
    if (!str)
        return NULL;
    str->init(base, chars, length);
    return str;
}

inline void
JSDependentString::markChildren(JSTracer *trc)
{
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
    JSAtom *atom = js_AtomizeString(cx, this);
    if (!atom)
        return NULL;
    return atom->asPropertyName();
}

JS_ALWAYS_INLINE void
JSFixedString::init(const jschar *chars, size_t length)
{
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    d.u1.chars = chars;
}

JS_ALWAYS_INLINE JSFixedString *
JSFixedString::new_(JSContext *cx, const jschar *chars, size_t length)
{
    JS_ASSERT(chars[length] == jschar(0));

    if (!validateLength(cx, length))
        return NULL;
    JSFixedString *str = (JSFixedString *)js_NewGCString(cx);
    if (!str)
        return NULL;
    str->init(chars, length);
    return str;
}

JS_ALWAYS_INLINE JSAtom *
JSFixedString::morphAtomizedStringIntoAtom()
{
    JS_ASSERT((d.lengthAndFlags & FLAGS_MASK) == JS_BIT(2));
    JS_STATIC_ASSERT(NON_STATIC_ATOM == JS_BIT(3));
    d.lengthAndFlags ^= (JS_BIT(2) | JS_BIT(3));
    return &asAtom();
}

JS_ALWAYS_INLINE JSInlineString *
JSInlineString::new_(JSContext *cx)
{
    return (JSInlineString *)js_NewGCString(cx);
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

JS_ALWAYS_INLINE JSShortString *
JSShortString::new_(JSContext *cx)
{
    return js_NewGCShortString(cx);
}

JS_ALWAYS_INLINE void
JSShortString::initAtOffsetInBuffer(const jschar *chars, size_t length)
{
    JS_ASSERT(lengthFits(length + (chars - d.inlineStorage)));
    JS_ASSERT(chars >= d.inlineStorage && chars < d.inlineStorage + MAX_SHORT_LENGTH);
    d.lengthAndFlags = buildLengthAndFlags(length, FIXED_FLAGS);
    d.u1.chars = chars;
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
    cx->runtime->updateMallocCounter(cx, (length + 1) * sizeof(jschar));
    return str;
}

inline bool
js::StaticStrings::fitsInSmallChar(jschar c)
{
    return c < SMALL_CHAR_LIMIT && toSmallChar[c] != INVALID_SMALL_CHAR;
}

inline bool
js::StaticStrings::hasUnit(jschar c)
{
    return c < UNIT_STATIC_LIMIT;
}

inline JSAtom *
js::StaticStrings::getUnit(jschar c)
{
    JS_ASSERT(hasUnit(c));
    return unitStaticTable[c];
}

inline bool
js::StaticStrings::hasUint(uint32_t u)
{
    return u < INT_STATIC_LIMIT;
}

inline JSAtom *
js::StaticStrings::getUint(uint32_t u)
{
    JS_ASSERT(hasUint(u));
    return intStaticTable[u];
}

inline bool
js::StaticStrings::hasInt(int32_t i)
{
    return uint32_t(i) < INT_STATIC_LIMIT;
}

inline JSAtom *
js::StaticStrings::getInt(int32_t i)
{
    JS_ASSERT(hasInt(i));
    return getUint(uint32_t(i));
}

inline JSLinearString *
js::StaticStrings::getUnitStringForElement(JSContext *cx, JSString *str, size_t index)
{
    JS_ASSERT(index < str->length());
    const jschar *chars = str->getChars(cx);
    if (!chars)
        return NULL;
    jschar c = chars[index];
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

inline JSAtom *
js::StaticStrings::getLength2(uint32_t i)
{
    JS_ASSERT(i < 100);
    return getLength2('0' + i / 10, '0' + i % 10);
}

/* Get a static atomized string for chars if possible. */
inline JSAtom *
js::StaticStrings::lookup(const jschar *chars, size_t length)
{
    switch (length) {
      case 1:
        if (chars[0] < UNIT_STATIC_LIMIT)
            return getUnit(chars[0]);
        return NULL;
      case 2:
        if (fitsInSmallChar(chars[0]) && fitsInSmallChar(chars[1]))
            return getLength2(chars[0], chars[1]);
        return NULL;
      case 3:
        /*
         * Here we know that JSString::intStringTable covers only 256 (or at least
         * not 1000 or more) chars. We rely on order here to resolve the unit vs.
         * int string/length-2 string atom identity issue by giving priority to unit
         * strings for "0" through "9" and length-2 strings for "10" through "99".
         */
        JS_STATIC_ASSERT(INT_STATIC_LIMIT <= 999);
        if ('1' <= chars[0] && chars[0] <= '9' &&
            '0' <= chars[1] && chars[1] <= '9' &&
            '0' <= chars[2] && chars[2] <= '9') {
            int i = (chars[0] - '0') * 100 +
                      (chars[1] - '0') * 10 +
                      (chars[2] - '0');

            if (unsigned(i) < INT_STATIC_LIMIT)
                return getInt(i);
        }
        return NULL;
    }

    return NULL;
}

JS_ALWAYS_INLINE void
JSString::finalize(JSContext *cx, bool background)
{
    /* Shorts are in a different arena. */
    JS_ASSERT(!isShort());

    if (isFlat())
        asFlat().finalize(cx->runtime);
    else
        JS_ASSERT(isDependent() || isRope());
}

inline void
JSFlatString::finalize(JSRuntime *rt)
{
    JS_ASSERT(!isShort());

    /*
     * This check depends on the fact that 'chars' is only initialized to the
     * beginning of inlineStorage. E.g., this is not the case for short strings.
     */
    if (chars() != d.inlineStorage)
        rt->free_(const_cast<jschar *>(chars()));
}

inline void
JSShortString::finalize(JSContext *cx, bool background)
{
    JS_ASSERT(JSString::isShort());
}

inline void
JSAtom::finalize(JSRuntime *rt)
{
    JS_ASSERT(JSString::isAtom());
    if (getAllocKind() == js::gc::FINALIZE_STRING)
        JSFlatString::finalize(rt);
    else
        JS_ASSERT(getAllocKind() == js::gc::FINALIZE_SHORT_STRING);
}

inline void
JSExternalString::finalize(JSContext *cx, bool background)
{
    finalize();
}

inline void
JSExternalString::finalize()
{
    const JSStringFinalizer *fin = externalFinalizer();
    fin->finalize(fin, const_cast<jschar *>(chars()));
}

namespace js {

static JS_ALWAYS_INLINE JSFixedString *
NewShortString(JSContext *cx, const jschar *chars, size_t length)
{
    /*
     * Don't bother trying to find a static atom; measurement shows that not
     * many get here (for one, Atomize is catching them).
     */
    JS_ASSERT(JSShortString::lengthFits(length));
    JSInlineString *str = JSInlineString::lengthFits(length)
                          ? JSInlineString::new_(cx)
                          : JSShortString::new_(cx);
    if (!str)
        return NULL;

    jschar *storage = str->init(length);
    PodCopy(storage, chars, length);
    storage[length] = 0;
    Probes::createString(cx, str, length);
    return str;
}

} /* namespace js */

#endif
