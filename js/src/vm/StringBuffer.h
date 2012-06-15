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
 */
class StringBuffer
{
    /* cb's buffer is taken by the new string so use ContextAllocPolicy. */
    typedef Vector<jschar, 32, ContextAllocPolicy> CharBuffer;

    CharBuffer cb;

    JSContext *context() const { return cb.allocPolicy().context(); }

    StringBuffer(const StringBuffer &other) MOZ_DELETE;
    void operator=(const StringBuffer &other) MOZ_DELETE;

  public:
    explicit StringBuffer(JSContext *cx) : cb(cx) { }

    inline bool reserve(size_t len) { return cb.reserve(len); }
    inline bool resize(size_t len) { return cb.resize(len); }
    inline bool append(const jschar c) { return cb.append(c); }
    inline bool append(const jschar *chars, size_t len) { return cb.append(chars, len); }
    inline bool append(const jschar *begin, const jschar *end) { return cb.append(begin, end); }
    inline bool append(JSString *str);
    inline bool append(JSLinearString *str);
    inline bool appendN(const jschar c, size_t n) { return cb.appendN(c, n); }
    inline bool appendInflated(const char *cstr, size_t len);

    template <size_t ArrayLength>
    bool append(const char (&array)[ArrayLength]) {
        return cb.append(array, array + ArrayLength - 1); /* No trailing '\0'. */
    }

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

    jschar *begin() { return cb.begin(); }
    jschar *end() { return cb.end(); }
    const jschar *begin() const { return cb.begin(); }
    const jschar *end() const { return cb.end(); }
    bool empty() const { return cb.empty(); }
    size_t length() const { return cb.length(); }

    /*
     * Creates a string from the characters in this buffer, then (regardless
     * whether string creation succeeded or failed) empties the buffer.
     */
    JSFixedString *finishString();

    /* Identical to finishString() except that an atom is created. */
    JSAtom *finishAtom();

    /*
     * Creates a raw string from the characters in this buffer.  The string is
     * exactly the characters in this buffer: it is *not* null-terminated
     * unless the last appended character was |(jschar)0|.
     */
    jschar *extractWellSized();
};

inline bool
StringBuffer::append(JSLinearString *str)
{
    JS::Anchor<JSString *> anch(str);
    return cb.append(str->chars(), str->length());
}

inline bool
StringBuffer::append(JSString *str)
{
    JSLinearString *linear = str->ensureLinear(context());
    if (!linear)
        return false;
    return append(linear);
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

/* ES5 9.8 ToString, appending the result to the string buffer. */
extern bool
ValueToStringBufferSlow(JSContext *cx, const Value &v, StringBuffer &sb);

inline bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb)
{
    if (v.isString())
        return sb.append(v.toString());

    return ValueToStringBufferSlow(cx, v, sb);
}

/* ES5 9.8 ToString for booleans, appending the result to the string buffer. */
inline bool
BooleanToStringBuffer(JSContext *cx, bool b, StringBuffer &sb)
{
    return b ? sb.append("true") : sb.append("false");
}

}  /* namespace js */

#endif /* StringBuffer_h___ */
