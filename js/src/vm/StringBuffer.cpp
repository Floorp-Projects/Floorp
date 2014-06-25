/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StringBuffer.h"

#include "mozilla/Range.h"

#include "jsobjinlines.h"

#include "vm/String-inl.h"

using namespace js;

using mozilla::Range;

template <typename CharT, class Buffer>
static CharT *
ExtractWellSized(ExclusiveContext *cx, Buffer &cb)
{
    size_t capacity = cb.capacity();
    size_t length = cb.length();

    CharT *buf = cb.extractRawBuffer();
    if (!buf)
        return nullptr;

    /* For medium/big buffers, avoid wasting more than 1/4 of the memory. */
    JS_ASSERT(capacity >= length);
    if (length > Buffer::sMaxInlineStorage && capacity - length > length / 4) {
        size_t bytes = sizeof(CharT) * (length + 1);
        CharT *tmp = (CharT *)cx->realloc_(buf, bytes);
        if (!tmp) {
            js_free(buf);
            return nullptr;
        }
        buf = tmp;
    }

    return buf;
}

jschar *
StringBuffer::stealChars()
{
    if (isLatin1() && !inflateChars())
        return nullptr;

    return ExtractWellSized<jschar>(cx, twoByteChars());
}

bool
StringBuffer::inflateChars()
{
    MOZ_ASSERT(isLatin1());

    TwoByteCharBuffer twoByte(cx);
    if (!twoByte.append(latin1Chars().begin(), latin1Chars().length()))
        return false;

    cb.destroy();
    cb.construct<TwoByteCharBuffer>(Move(twoByte));
    return true;
}

template <typename CharT, class Buffer>
static JSFlatString *
FinishStringFlat(ExclusiveContext *cx, StringBuffer &sb, Buffer &cb)
{
    size_t len = sb.length();
    if (!sb.append('\0'))
        return nullptr;

    ScopedJSFreePtr<CharT> buf(ExtractWellSized<CharT>(cx, cb));
    if (!buf)
        return nullptr;

    JSFlatString *str = NewString<CanGC>(cx, buf.get(), len);
    if (!str)
        return nullptr;

    buf.forget();
    return str;
}

JSFlatString *
StringBuffer::finishString()
{
    size_t len = length();
    if (len == 0)
        return cx->names().empty;

    if (!JSString::validateLength(cx, len))
        return nullptr;

    JS_STATIC_ASSERT(JSFatInlineString::MAX_LENGTH_TWO_BYTE < TwoByteCharBuffer::InlineLength);
    JS_STATIC_ASSERT(JSFatInlineString::MAX_LENGTH_LATIN1 < Latin1CharBuffer::InlineLength);

    if (isLatin1()) {
        if (JSFatInlineString::latin1LengthFits(len))
            return NewFatInlineString<CanGC>(cx, Range<const Latin1Char>(latin1Chars().begin(), len));
    } else {
        if (JSFatInlineString::twoByteLengthFits(len))
            return NewFatInlineString<CanGC>(cx, Range<const jschar>(twoByteChars().begin(), len));
    }

    return isLatin1()
        ? FinishStringFlat<Latin1Char>(cx, *this, latin1Chars())
        : FinishStringFlat<jschar>(cx, *this, twoByteChars());
}

JSAtom *
StringBuffer::finishAtom()
{
    size_t len = length();
    if (len == 0)
        return cx->names().empty;

    if (isLatin1()) {
        JSAtom *atom = AtomizeChars(cx, latin1Chars().begin(), len);
        latin1Chars().clear();
        return atom;
    }

    JSAtom *atom = AtomizeChars(cx, twoByteChars().begin(), len);
    twoByteChars().clear();
    return atom;
}

bool
js::ValueToStringBufferSlow(JSContext *cx, const Value &arg, StringBuffer &sb)
{
    RootedValue v(cx, arg);
    if (!ToPrimitive(cx, JSTYPE_STRING, &v))
        return false;

    if (v.isString())
        return sb.append(v.toString());
    if (v.isNumber())
        return NumberValueToStringBuffer(cx, v, sb);
    if (v.isBoolean())
        return BooleanToStringBuffer(v.toBoolean(), sb);
    if (v.isNull())
        return sb.append(cx->names().null);
    if (v.isSymbol()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SYMBOL_TO_STRING);
        return false;
    }
    JS_ASSERT(v.isUndefined());
    return sb.append(cx->names().undefined);
}
