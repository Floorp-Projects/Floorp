/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        return NULL;

    /* For medium/big buffers, avoid wasting more than 1/4 of the memory. */
    JS_ASSERT(capacity >= length);
    if (length > CharBuffer::sMaxInlineStorage && capacity - length > length / 4) {
        size_t bytes = sizeof(jschar) * (length + 1);
        JSContext *cx = context();
        jschar *tmp = (jschar *)cx->realloc_(buf, bytes);
        if (!tmp) {
            cx->free_(buf);
            return NULL;
        }
        buf = tmp;
    }

    return buf;
}

JSFixedString *
StringBuffer::finishString()
{
    JSContext *cx = context();
    if (cb.empty())
        return cx->runtime->atomState.emptyAtom;

    size_t length = cb.length();
    if (!JSString::validateLength(cx, length))
        return NULL;

    JS_STATIC_ASSERT(JSShortString::MAX_SHORT_LENGTH < CharBuffer::InlineLength);
    if (JSShortString::lengthFits(length))
        return NewShortString(cx, cb.begin(), length);

    if (!cb.append('\0'))
        return NULL;

    jschar *buf = extractWellSized();
    if (!buf)
        return NULL;

    JSFixedString *str = js_NewString(cx, buf, length);
    if (!str)
        cx->free_(buf);
    return str;
}

JSAtom *
StringBuffer::finishAtom()
{
    JSContext *cx = context();

    size_t length = cb.length();
    if (length == 0)
        return cx->runtime->atomState.emptyAtom;

    JSAtom *atom = js_AtomizeChars(cx, cb.begin(), length);
    cb.clear();
    return atom;
}

bool
js::ValueToStringBufferSlow(JSContext *cx, const Value &arg, StringBuffer &sb)
{
    Value v = arg;
    if (!ToPrimitive(cx, JSTYPE_STRING, &v))
        return false;

    if (v.isString())
        return sb.append(v.toString());
    if (v.isNumber())
        return NumberValueToStringBuffer(cx, v, sb);
    if (v.isBoolean())
        return BooleanToStringBuffer(cx, v.toBoolean(), sb);
    if (v.isNull())
        return sb.append(cx->runtime->atomState.nullAtom);
    JS_ASSERT(v.isUndefined());
    return sb.append(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]);
}
