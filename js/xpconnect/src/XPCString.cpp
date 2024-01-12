/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "nscore.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "jsapi.h"
#include "xpcpublic.h"

using namespace JS;

const XPCStringConvert::LiteralExternalString
    XPCStringConvert::sLiteralExternalString;

const XPCStringConvert::DOMStringExternalString
    XPCStringConvert::sDOMStringExternalString;

void XPCStringConvert::LiteralExternalString::finalize(
    JS::Latin1Char* aChars) const {
  // Nothing to do.
}

void XPCStringConvert::LiteralExternalString::finalize(char16_t* aChars) const {
  // Nothing to do.
}

size_t XPCStringConvert::LiteralExternalString::sizeOfBuffer(
    const JS::Latin1Char* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // This string's buffer is not heap-allocated, so its malloc size is 0.
  return 0;
}

size_t XPCStringConvert::LiteralExternalString::sizeOfBuffer(
    const char16_t* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // This string's buffer is not heap-allocated, so its malloc size is 0.
  return 0;
}

void XPCStringConvert::DOMStringExternalString::finalize(
    JS::Latin1Char* aChars) const {
  nsStringBuffer* buf = nsStringBuffer::FromData(aChars);
  buf->Release();
}

void XPCStringConvert::DOMStringExternalString::finalize(
    char16_t* aChars) const {
  nsStringBuffer* buf = nsStringBuffer::FromData(aChars);
  buf->Release();
}

size_t XPCStringConvert::DOMStringExternalString::sizeOfBuffer(
    const JS::Latin1Char* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // We promised the JS engine we would not GC.  Enforce that:
  JS::AutoCheckCannotGC autoCannotGC;

  const nsStringBuffer* buf =
      nsStringBuffer::FromData(const_cast<JS::Latin1Char*>(aChars));
  // We want sizeof including this, because the entire string buffer is owned by
  // the external string.  But only report here if we're unshared; if we're
  // shared then we don't know who really owns this data.
  return buf->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
}

size_t XPCStringConvert::DOMStringExternalString::sizeOfBuffer(
    const char16_t* aChars, mozilla::MallocSizeOf aMallocSizeOf) const {
  // We promised the JS engine we would not GC.  Enforce that:
  JS::AutoCheckCannotGC autoCannotGC;

  const nsStringBuffer* buf =
      nsStringBuffer::FromData(const_cast<char16_t*>(aChars));
  // We want sizeof including this, because the entire string buffer is owned by
  // the external string.  But only report here if we're unshared; if we're
  // shared then we don't know who really owns this data.
  return buf->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
}

// convert a readable to a JSString, copying string data
// static
bool XPCStringConvert::ReadableToJSVal(JSContext* cx, const nsAString& readable,
                                       nsStringBuffer** sharedBuffer,
                                       MutableHandleValue vp) {
  *sharedBuffer = nullptr;

  uint32_t length = readable.Length();

  if (readable.IsLiteral()) {
    return StringLiteralToJSVal(cx, readable.BeginReading(), length, vp);
  }

  nsStringBuffer* buf = nsStringBuffer::FromString(readable);
  if (buf) {
    bool shared;
    if (!UCStringBufferToJSVal(cx, buf, length, vp, &shared)) {
      return false;
    }
    if (shared) {
      *sharedBuffer = buf;
    }
    return true;
  }

  // blech, have to copy.
  JSString* str = JS_NewUCStringCopyN(cx, readable.BeginReading(), length);
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}

bool XPCStringConvert::Latin1ToJSVal(JSContext* cx, const nsACString& latin1,
                                     nsStringBuffer** sharedBuffer,
                                     MutableHandleValue vp) {
  *sharedBuffer = nullptr;

  uint32_t length = latin1.Length();

  if (latin1.IsLiteral()) {
    return StringLiteralToJSVal(
        cx, reinterpret_cast<const JS::Latin1Char*>(latin1.BeginReading()),
        length, vp);
  }

  nsStringBuffer* buf = nsStringBuffer::FromString(latin1);
  if (buf) {
    bool shared;
    if (!Latin1StringBufferToJSVal(cx, buf, length, vp, &shared)) {
      return false;
    }
    if (shared) {
      *sharedBuffer = buf;
    }
    return true;
  }

  JSString* str = JS_NewStringCopyN(cx, latin1.BeginReading(), length);
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}

bool XPCStringConvert::UTF8ToJSVal(JSContext* cx, const nsACString& utf8,
                                   nsStringBuffer** sharedBuffer,
                                   MutableHandleValue vp) {
  *sharedBuffer = nullptr;

  uint32_t length = utf8.Length();

  if (utf8.IsLiteral()) {
    return UTF8StringLiteralToJSVal(
        cx, JS::UTF8Chars(utf8.BeginReading(), length), vp);
  }

  nsStringBuffer* buf = nsStringBuffer::FromString(utf8);
  if (buf) {
    bool shared;
    if (!UTF8StringBufferToJSVal(cx, buf, length, vp, &shared)) {
      return false;
    }
    if (shared) {
      *sharedBuffer = buf;
    }
    return true;
  }

  JSString* str =
      JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(utf8.BeginReading(), length));
  if (!str) {
    return false;
  }
  vp.setString(str);
  return true;
}

namespace xpc {

bool NonVoidStringToJsval(JSContext* cx, nsAString& str,
                          MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but ReadableToJSVal didn't addref it.
    // Move the ownership from str to jsstr.
    str.ForgetSharedBuffer();
  }
  return true;
}

bool NonVoidStringToJsval(JSContext* cx, const nsAString& str,
                          MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but ReadableToJSVal didn't addref it.
    sharedBuffer->AddRef();
  }
  return true;
}

bool NonVoidLatin1StringToJsval(JSContext* cx, nsACString& str,
                                MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::Latin1ToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but Latin1ToJSVal didn't addref it.
    // Move the ownership from str to jsstr.
    str.ForgetSharedBuffer();
  }
  return true;
}

bool NonVoidLatin1StringToJsval(JSContext* cx, const nsACString& str,
                                MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::Latin1ToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but Latin1ToJSVal didn't addref it.
    sharedBuffer->AddRef();
  }
  return true;
}

bool NonVoidUTF8StringToJsval(JSContext* cx, nsACString& str,
                              MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::UTF8ToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but UTF8ToJSVal didn't addref it.
    // Move the ownership from str to jsstr.
    str.ForgetSharedBuffer();
  }
  return true;
}

bool NonVoidUTF8StringToJsval(JSContext* cx, const nsACString& str,
                              MutableHandleValue rval) {
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::UTF8ToJSVal(cx, str, &sharedBuffer, rval)) {
    return false;
  }

  if (sharedBuffer) {
    // The string was shared but UTF8ToJSVal didn't addref it.
    sharedBuffer->AddRef();
  }
  return true;
}

}  // namespace xpc
