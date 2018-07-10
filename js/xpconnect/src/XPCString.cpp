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

// static
void
XPCStringConvert::FinalizeDynamicAtom(const JSStringFinalizer* fin,
                                      char16_t* chars)
{
    nsDynamicAtom* atom = nsDynamicAtom::FromChars(chars);
    // nsDynamicAtom::Release is always-inline and defined in a translation unit
    // we can't get to here.  So we need to go through nsAtom::Release to call
    // it.
    static_cast<nsAtom*>(atom)->Release();
}

const JSStringFinalizer XPCStringConvert::sDynamicAtomFinalizer =
    { XPCStringConvert::FinalizeDynamicAtom };

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
        return StringLiteralToJSVal(cx, readable.BeginReading(), length, vp);
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
