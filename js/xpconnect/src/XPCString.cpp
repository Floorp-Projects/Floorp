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

#include "nscore.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "jsapi.h"
#include "xpcpublic.h"


// static
void
XPCStringConvert::FreeZoneCache(JS::Zone *zone)
{
    // Put the zone user data into an AutoPtr (which will do the cleanup for us),
    // and null out the user data (which may already be null).
    nsAutoPtr<ZoneStringCache> cache(static_cast<ZoneStringCache*>(JS_GetZoneUserData(zone)));
    JS_SetZoneUserData(zone, nullptr);
}

// static
void
XPCStringConvert::ClearZoneCache(JS::Zone *zone)
{
    ZoneStringCache *cache = static_cast<ZoneStringCache*>(JS_GetZoneUserData(zone));
    if (cache) {
        cache->mBuffer = nullptr;
        cache->mString = nullptr;
    }
}

// static
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
        bool ok = StringBufferToJSVal(cx, buf, length, &val, &shared);
        if (!ok) {
            return JS::NullValue();
        }

        if (shared) {
            *sharedBuffer = buf;
        }
        return val;
    }

    // blech, have to copy.
    str = JS_NewUCStringCopyN(cx, readable.BeginReading(), length);
    return str ? JS::StringValue(str) : JS::NullValue();
}
