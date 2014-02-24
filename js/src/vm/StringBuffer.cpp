/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

#include "vm/String-inl.h"

using namespace js;

jschar *
StringBuffer::extractWellSized()
{
    size_t capacity = cb.capacity();
    size_t length = cb.length();

    jschar *buf = cb.extractRawBuffer();
    if (!buf)
        return nullptr;

    /* For medium/big buffers, avoid wasting more than 1/4 of the memory. */
    JS_ASSERT(capacity >= length);
    if (length > CharBuffer::sMaxInlineStorage && capacity - length > length / 4) {
        size_t bytes = sizeof(jschar) * (length + 1);
        ExclusiveContext *cx = context();
        jschar *tmp = (jschar *)cx->realloc_(buf, bytes);
        if (!tmp) {
            js_free(buf);
            return nullptr;
        }
        buf = tmp;
    }

    return buf;
}

JSFlatString *
StringBuffer::finishString()
{
    ExclusiveContext *cx = context();
    if (cb.empty())
        return cx->names().empty;

    size_t length = cb.length();
    if (!JSString::validateLength(cx, length))
        return nullptr;

    JS_STATIC_ASSERT(JSShortString::MAX_SHORT_LENGTH < CharBuffer::InlineLength);
    if (JSShortString::lengthFits(length))
        return NewShortString<CanGC>(cx, TwoByteChars(cb.begin(), length));

    if (!cb.append('\0'))
        return nullptr;

    jschar *buf = extractWellSized();
    if (!buf)
        return nullptr;

    JSFlatString *str = js_NewString<CanGC>(cx, buf, length);
    if (!str)
        js_free(buf);
    return str;
}

JSAtom *
StringBuffer::finishAtom()
{
    ExclusiveContext *cx = context();

    size_t length = cb.length();
    if (length == 0)
        return cx->names().empty;

    JSAtom *atom = AtomizeChars(cx, cb.begin(), length);
    cb.clear();
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
    JS_ASSERT(v.isUndefined());
    return sb.append(cx->names().undefined);
}
