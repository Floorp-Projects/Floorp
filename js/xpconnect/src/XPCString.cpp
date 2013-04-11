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

// One-slot cache, because it turns out it's common for web pages to
// get the same string a few times in a row.  We get about a 40% cache
// hit rate on this cache last it was measured.  We'd get about 70%
// hit rate with a hashtable with removal on finalization, but that
// would take a lot more machinery.
nsStringBuffer* XPCStringConvert::sCachedBuffer = nullptr;
JSString* XPCStringConvert::sCachedString = nullptr;

// Called from GC finalize callback to make sure we don't hand out a pointer to
// a JSString that's about to be finalized by incremental sweeping.
// static
void
XPCStringConvert::ClearCache()
{
    sCachedBuffer = nullptr;
    sCachedString = nullptr;
}

void
XPCStringConvert::FinalizeDOMString(const JSStringFinalizer *fin, jschar *chars)
{
    nsStringBuffer* buf = nsStringBuffer::FromData(chars);
    buf->Release();
}

const JSStringFinalizer XPCStringConvert::sDOMStringFinalizer =
    { XPCStringConvert::FinalizeDOMString };

// convert a readable to a JSString, copying string data
// static
jsval
XPCStringConvert::ReadableToJSVal(JSContext *cx,
                                  const nsAString &readable,
                                  nsStringBuffer** sharedBuffer)
{
    JSString *str;
    *sharedBuffer = nullptr;

    uint32_t length = readable.Length();

    if (length == 0)
        return JS_GetEmptyStringValue(cx);

    nsStringBuffer *buf = nsStringBuffer::FromString(readable);
    if (buf) {
        JS::RootedValue val(cx);
        bool shared;
        bool ok = StringBufferToJSVal(cx, buf, length, val.address(), &shared);
        if (!ok) {
            return JS::NullValue();
        }

        if (shared) {
            *sharedBuffer = buf;
        }
        return val;
    }

    // blech, have to copy.

    jschar *chars = reinterpret_cast<jschar *>
                                    (JS_malloc(cx, (length + 1) *
                                               sizeof(jschar)));
    if (!chars)
        return JS::NullValue();

    if (length && !CopyUnicodeTo(readable, 0,
                                 reinterpret_cast<PRUnichar *>(chars),
                                 length)) {
        JS_free(cx, chars);
        return JS::NullValue();
    }

    chars[length] = 0;

    str = JS_NewUCString(cx, chars, length);
    if (!str) {
        JS_free(cx, chars);
    }

    return str ? STRING_TO_JSVAL(str) : JSVAL_NULL;
}
