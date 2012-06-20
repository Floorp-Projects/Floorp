/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsatominlines_h___
#define jsatominlines_h___

#include "jsatom.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

#include "mozilla/RangedPtr.h"
#include "vm/String.h"

inline JSAtom *
js::AtomStateEntry::asPtr() const
{
    JS_ASSERT(bits != 0);
    JSAtom *atom = reinterpret_cast<JSAtom *>(bits & NO_TAG_MASK);
    JSString::readBarrier(atom);
    return atom;
}

namespace js {

inline JSAtom *
ToAtom(JSContext *cx, const js::Value &v)
{
    if (!v.isString()) {
        JSString *str = js::ToStringSlow(cx, v);
        if (!str)
            return NULL;
        JS::Anchor<JSString *> anchor(str);
        return js_AtomizeString(cx, str);
    }

    JSString *str = v.toString();
    if (str->isAtom())
        return &str->asAtom();

    JS::Anchor<JSString *> anchor(str);
    return js_AtomizeString(cx, str);
}

inline bool
ValueToId(JSContext* cx, JSObject *obj, const Value &v, jsid *idp)
{
    int32_t i;
    if (ValueFitsInInt32(v, &i) && INT_FITS_IN_JSID(i)) {
        *idp = INT_TO_JSID(i);
        return true;
    }

    return InternNonIntElementId(cx, obj, v, idp);
}

inline bool
ValueToId(JSContext* cx, const Value &v, jsid *idp)
{
    return ValueToId(cx, NULL, v, idp);
}

/*
 * Write out character representing |index| to the memory just before |end|.
 * Thus |*end| is not touched, but |end[-1]| and earlier are modified as
 * appropriate.  There must be at least js::UINT32_CHAR_BUFFER_LENGTH elements
 * before |end| to avoid buffer underflow.  The start of the characters written
 * is returned and is necessarily before |end|.
 */
template <typename T>
inline mozilla::RangedPtr<T>
BackfillIndexInCharBuffer(uint32_t index, mozilla::RangedPtr<T> end)
{
#ifdef DEBUG
    /*
     * Assert that the buffer we're filling will hold as many characters as we
     * could write out, by dereferencing the index that would hold the most
     * significant digit.
     */
    (void) *(end - UINT32_CHAR_BUFFER_LENGTH);
#endif

    do {
        uint32_t next = index / 10, digit = index % 10;
        *--end = '0' + digit;
        index = next;
    } while (index > 0);

    return end;
}

inline bool
IndexToId(JSContext *cx, uint32_t index, jsid *idp)
{
    MaybeCheckStackRoots(cx);

    if (index <= JSID_INT_MAX) {
        *idp = INT_TO_JSID(index);
        return true;
    }

    extern bool IndexToIdSlow(JSContext *cx, uint32_t index, jsid *idp);
    return IndexToIdSlow(cx, index, idp);
}

inline jsid
AtomToId(JSAtom *atom)
{
    JS_STATIC_ASSERT(JSID_INT_MIN == 0);

    uint32_t index;
    if (atom->isIndex(&index) && index <= JSID_INT_MAX)
        return INT_TO_JSID((int32_t) index);

    return JSID_FROM_BITS((size_t)atom);
}

static JS_ALWAYS_INLINE JSFlatString *
IdToString(JSContext *cx, jsid id)
{
    if (JSID_IS_STRING(id))
        return JSID_TO_ATOM(id);

    if (JS_LIKELY(JSID_IS_INT(id)))
        return Int32ToString(cx, JSID_TO_INT(id));

    JSString *str = ToStringSlow(cx, IdToValue(id));
    if (!str)
        return NULL;

    return str->ensureFlat(cx);
}

inline
AtomHasher::Lookup::Lookup(const JSAtom *atom)
  : chars(atom->chars()), length(atom->length()), atom(atom)
{}

inline bool
AtomHasher::match(const AtomStateEntry &entry, const Lookup &lookup)
{
    JSAtom *key = entry.asPtr();
    if (lookup.atom)
        return lookup.atom == key;
    if (key->length() != lookup.length)
        return false;
    return PodEqual(key->chars(), lookup.chars, lookup.length);
}

} // namespace js

#endif /* jsatominlines_h___ */
