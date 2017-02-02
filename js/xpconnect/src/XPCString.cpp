/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

#include "nsAutoPtr.h"
#include "nscore.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "jsapi.h"
#include "xpcpublic.h"

using namespace JS;

// static
void
XPCStringConvert::FreeZoneCache(JS::Zone* zone)
{
    // Put the zone user data into an AutoPtr (which will do the cleanup for us),
    // and null out the user data (which may already be null).
    nsAutoPtr<ZoneStringCache> cache(static_cast<ZoneStringCache*>(JS_GetZoneUserData(zone)));
    JS_SetZoneUserData(zone, nullptr);
}

// static
void
XPCStringConvert::ClearZoneCache(JS::Zone* zone)
{
    ZoneStringCache* cache = static_cast<ZoneStringCache*>(JS_GetZoneUserData(zone));
    if (cache) {
        cache->mBuffer = nullptr;
        cache->mString = nullptr;
    }
}

// static
void
XPCStringConvert::FinalizeLiteral(const JSStringFinalizer* fin, char16_t* chars)
{
}

const JSStringFinalizer XPCStringConvert::sLiteralFinalizer =
    { XPCStringConvert::FinalizeLiteral };

// static
void
XPCStringConvert::FinalizeDOMString(const JSStringFinalizer* fin, char16_t* chars)
{
    nsStringBuffer* buf = nsStringBuffer::FromData(chars);
    buf->Release();
}

const JSStringFinalizer XPCStringConvert::sDOMStringFinalizer =
    { XPCStringConvert::FinalizeDOMString };

// convert a readable to a JSString, copying string data
// static
bool
XPCStringConvert::ReadableToJSVal(JSContext* cx,
                                  const nsAString& readable,
                                  nsStringBuffer** sharedBuffer,
                                  MutableHandleValue vp)
{
    *sharedBuffer = nullptr;

    uint32_t length = readable.Length();

    if (readable.IsLiteral()) {
        JSString* str = JS_NewExternalString(cx,
                                             static_cast<const char16_t*>(readable.BeginReading()),
                                             length, &sLiteralFinalizer);
        if (!str)
            return false;
        vp.setString(str);
        return true;
    }

    nsStringBuffer* buf = nsStringBuffer::FromString(readable);
    if (buf) {
        bool shared;
        if (!StringBufferToJSVal(cx, buf, length, vp, &shared))
            return false;
        if (shared)
            *sharedBuffer = buf;
        return true;
    }

    // blech, have to copy.
    JSString* str = JS_NewUCStringCopyN(cx, readable.BeginReading(), length);
    if (!str)
        return false;
    vp.setString(str);
    return true;
}

namespace xpc {

bool
NonVoidStringToJsval(JSContext* cx, nsAString& str, MutableHandleValue rval)
{
    nsStringBuffer* sharedBuffer;
    if (!XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer, rval))
      return false;

    if (sharedBuffer) {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return true;
}

} // namespace xpc
