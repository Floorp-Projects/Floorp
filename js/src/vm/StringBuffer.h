/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StringBuffer_h___
#define StringBuffer_h___

#include "mozilla/Attributes.h"

#include "jscntxt.h"
#include "jspubtd.h"

#include "js/Vector.h"

namespace js {

/*
 * String builder that eagerly checks for over-allocation past the maximum
 * string length.
 *
 * Any operation which would exceed the maximum string length causes an
 * exception report on the context and results in a failed return value.
 *
 * Well-sized extractions (which waste no more than 1/4 of their char
 * buffer space) are guaranteed for strings built by this interface.
 * See |extractWellSized|.
 *
 * Note: over-allocation is not checked for when using the infallible
 * |replaceRawBuffer|, so the implementation of |finishString| also must check
 * for over-allocation.
 */
class StringBuffer
{
    /* cb's buffer is taken by the new string so use ContextAllocPolicy. */
    typedef Vector<jschar, 32, ContextAllocPolicy> CharBuffer;

    CharBuffer cb;

    static inline bool checkLength(JSContext *cx, size_t length);
    inline bool checkLength(size_t length);
    JSContext *context() const { return cb.allocPolicy().context(); }
    jschar *extractWellSized();

    StringBuffer(const StringBuffer &other) MOZ_DELETE;
    void operator=(const StringBuffer &other) MOZ_DELETE;

  public:
    explicit inline StringBuffer(JSContext *cx);
    inline bool reserve(size_t len);
    inline bool resize(size_t len);
    inline bool append(const jschar c);
    inline bool append(const jschar *chars, size_t len);
    inline bool append(const jschar *begin, const jschar *end);
    inline bool append(JSString *str);
    inline bool append(JSLinearString *str);
    inline bool appendN(const jschar c, size_t n);
    inline bool appendInflated(const char *cstr, size_t len);

    /* Infallible variants usable when the corresponding space is reserved. */
    void infallibleAppend(const jschar c) {
        cb.infallibleAppend(c);
    }
    void infallibleAppend(const jschar *chars, size_t len) {
        cb.infallibleAppend(chars, len);
    }
    void infallibleAppend(const jschar *begin, const jschar *end) {
        cb.infallibleAppend(begin, end);
    }
    void infallibleAppendN(const jschar c, size_t n) {
        cb.infallibleAppendN(c, n);
    }

    JSAtom *atomize(unsigned flags = 0);
    static JSAtom *atomize(JSContext *cx, const CharBuffer &cb, unsigned flags = 0);
    static JSAtom *atomize(JSContext *cx, const jschar *begin, size_t length, unsigned flags = 0);

    void replaceRawBuffer(jschar *chars, size_t len) { cb.replaceRawBuffer(chars, len); }
    jschar *begin() { return cb.begin(); }
    jschar *end() { return cb.end(); }
    const jschar *begin() const { return cb.begin(); }
    const jschar *end() const { return cb.end(); }
    bool empty() const { return cb.empty(); }
    inline size_t length() const;

    /*
     * Creates a string from the characters in this buffer, then (regardless
     * whether string creation succeeded or failed) empties the buffer.
     */
    JSFixedString *finishString();

    /* Identical to finishString() except that an atom is created. */
    JSAtom *finishAtom();

    template <size_t ArrayLength>
    bool append(const char (&array)[ArrayLength]) {
        return cb.append(array, array + ArrayLength - 1); /* No trailing '\0'. */
    }
};

inline
StringBuffer::StringBuffer(JSContext *cx)
  : cb(cx)
{}

inline bool
StringBuffer::append(JSString *str)
{
    JSLinearString *linear = str->ensureLinear(context());
    if (!linear)
        return false;
    return append(linear);
}

inline bool
StringBuffer::append(JSLinearString *str)
{
    JS::Anchor<JSString *> anch(str);
    return cb.append(str->chars(), str->length());
}

inline size_t
StringBuffer::length() const
{
    JS_ASSERT(cb.length() <= JSString::MAX_LENGTH);
    return cb.length();
}

inline bool
StringBuffer::appendInflated(const char *cstr, size_t cstrlen)
{
    size_t lengthBefore = length();
    if (!cb.growByUninitialized(cstrlen))
        return false;
    DebugOnly<size_t> oldcstrlen = cstrlen;
    DebugOnly<bool> ok = InflateStringToBuffer(context(), cstr, cstrlen,
                                               begin() + lengthBefore, &cstrlen);
    JS_ASSERT(ok && oldcstrlen == cstrlen);
    return true;
}

}  /* namespace js */

#endif /* StringBuffer_h___ */
