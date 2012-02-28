/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsstrinlines_h___
#define jsstrinlines_h___

#include "mozilla/Attributes.h"

#include "jsatom.h"
#include "jsstr.h"

#include "jscntxtinlines.h"
#include "jsgcinlines.h"
#include "vm/String-inl.h"

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
    bool reserve(size_t len);
    bool resize(size_t len);
    bool append(const jschar c);
    bool append(const jschar *chars, size_t len);
    bool append(const jschar *begin, const jschar *end);
    bool append(JSString *str);
    bool append(JSLinearString *str);
    bool appendN(const jschar c, size_t n);
    bool appendInflated(const char *cstr, size_t len);

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
    inline jsint length() const;

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
StringBuffer::reserve(size_t len)
{
    if (!checkLength(len))
        return false;
    return cb.reserve(len);
}

inline bool
StringBuffer::resize(size_t len)
{
    if (!checkLength(len))
        return false;
    return cb.resize(len);
}

inline bool
StringBuffer::append(const jschar c)
{
    if (!checkLength(cb.length() + 1))
        return false;
    return cb.append(c);
}

inline bool
StringBuffer::append(const jschar *chars, size_t len)
{
    if (!checkLength(cb.length() + len))
        return false;
    return cb.append(chars, len);
}

inline bool
StringBuffer::append(const jschar *begin, const jschar *end)
{
    if (!checkLength(cb.length() + (end - begin)))
        return false;
    return cb.append(begin, end);
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
StringBuffer::append(JSLinearString *str)
{
    JS::Anchor<JSString *> anch(str);
    return cb.append(str->chars(), str->length());
}

inline bool
StringBuffer::appendN(const jschar c, size_t n)
{
    if (!checkLength(cb.length() + n))
        return false;
    return cb.appendN(c, n);
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

inline jsint
StringBuffer::length() const
{
    JS_STATIC_ASSERT(jsint(JSString::MAX_LENGTH) == JSString::MAX_LENGTH);
    JS_ASSERT(cb.length() <= JSString::MAX_LENGTH);
    return jsint(cb.length());
}

inline bool
StringBuffer::checkLength(size_t length)
{
    return JSString::validateLength(context(), length);
}

extern bool
ValueToStringBufferSlow(JSContext *cx, const Value &v, StringBuffer &sb);

inline bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb)
{
    if (v.isString())
        return sb.append(v.toString());

    return ValueToStringBufferSlow(cx, v, sb);
}

class RopeBuilder {
    JSContext *cx;
    JSString *res;

    RopeBuilder(const RopeBuilder &other) MOZ_DELETE;
    void operator=(const RopeBuilder &other) MOZ_DELETE;

  public:
    RopeBuilder(JSContext *cx)
      : cx(cx), res(cx->runtime->emptyString)
    {}

    inline bool append(JSString *str) {
        res = js_ConcatStrings(cx, res, str);
        return !!res;
    }

    inline JSString *result() {
        return res;
    }
};

class StringSegmentRange
{
    /*
     * If malloc() shows up in any profiles from this vector, we can add a new
     * StackAllocPolicy which stashes a reusable freed-at-gc buffer in the cx.
     */
    Vector<JSString *, 32> stack;
    JSLinearString *cur;

    bool settle(JSString *str) {
        while (str->isRope()) {
            JSRope &rope = str->asRope();
            if (!stack.append(rope.rightChild()))
                return false;
            str = rope.leftChild();
        }
        cur = &str->asLinear();
        return true;
    }

  public:
    StringSegmentRange(JSContext *cx)
      : stack(cx), cur(NULL)
    {}

    JS_WARN_UNUSED_RESULT bool init(JSString *str) {
        JS_ASSERT(stack.empty());
        return settle(str);
    }

    bool empty() const {
        return cur == NULL;
    }

    JSLinearString *front() const {
        JS_ASSERT(!cur->isRope());
        return cur;
    }

    JS_WARN_UNUSED_RESULT bool popFront() {
        JS_ASSERT(!empty());
        if (stack.empty()) {
            cur = NULL;
            return true;
        }
        return settle(stack.popCopy());
    }
};

/*
 * Return s advanced past any Unicode white space characters.
 */
static inline const jschar *
SkipSpace(const jschar *s, const jschar *end)
{
    JS_ASSERT(s <= end);

    while (s < end && unicode::IsSpace(*s))
        s++;

    return s;
}

/*
 * Return less than, equal to, or greater than zero depending on whether
 * s1 is less than, equal to, or greater than s2.
 */
inline bool
CompareChars(const jschar *s1, size_t l1, const jschar *s2, size_t l2, int32_t *result)
{
    size_t n = JS_MIN(l1, l2);
    for (size_t i = 0; i < n; i++) {
        if (int32_t cmp = s1[i] - s2[i]) {
            *result = cmp;
            return true;
        }
    }

    *result = (int32_t)(l1 - l2);
    return true;
}

}  /* namespace js */

#endif /* jsstrinlines_h___ */
