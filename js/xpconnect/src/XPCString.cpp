/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Infrastructure for sharing DOMString data with JSStrings.
 *
 * Importing an nsAString into JS:
 * If possible (GetSharedBufferHandle works) use the external string support in
 * JS to create a JSString that points to the readable's buffer.  We keep a
 * reference to the buffer handle until the JSString is finalized.
 *
 * Exporting a JSString as an nsAReadable:
 * Wrap the JSString with a root-holding XPCJSReadableStringWrapper, which roots
 * the string and exposes its buffer via the nsAString interface, as
 * well as providing refcounting support.
 */

#include "xpcprivate.h"
#include "nsStringBuffer.h"

static void
FinalizeDOMString(const JSStringFinalizer *fin, jschar *chars)
{
    nsStringBuffer::FromData(chars)->Release();
}

static const JSStringFinalizer sDOMStringFinalizer = { FinalizeDOMString };


// convert a readable to a JSString, copying string data
// static
jsval
XPCStringConvert::ReadableToJSVal(JSContext *cx,
                                  const nsAString &readable,
                                  nsStringBuffer** sharedBuffer)
{
    JSString *str;
    *sharedBuffer = nullptr;

    PRUint32 length = readable.Length();

    if (length == 0)
        return JS_GetEmptyStringValue(cx);

    nsStringBuffer *buf = nsStringBuffer::FromString(readable);
    if (buf) {
        // yay, we can share the string's buffer!

        str = JS_NewExternalString(cx,
                                   reinterpret_cast<jschar *>(buf->Data()),
                                   length, &sDOMStringFinalizer);

        if (str) {
            *sharedBuffer = buf;
        }
    } else {
        // blech, have to copy.

        jschar *chars = reinterpret_cast<jschar *>
                                        (JS_malloc(cx, (length + 1) *
                                                   sizeof(jschar)));
        if (!chars)
            return JSVAL_NULL;

        if (length && !CopyUnicodeTo(readable, 0,
                                     reinterpret_cast<PRUnichar *>(chars),
                                     length)) {
            JS_free(cx, chars);
            return JSVAL_NULL;
        }

        chars[length] = 0;

        str = JS_NewUCString(cx, chars, length);
        if (!str)
            JS_free(cx, chars);
    }
    return str ? STRING_TO_JSVAL(str) : JSVAL_NULL;
}
