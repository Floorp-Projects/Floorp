/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextEncoder.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

void
TextEncoder::Init()
{
  // Create an encoder object for utf-8.
  mEncoder = EncodingUtils::EncoderForEncoding(NS_LITERAL_CSTRING("UTF-8"));
}

void
TextEncoder::Encode(JSContext* aCx,
                    JS::Handle<JSObject*> aObj,
                    const nsAString& aString,
                    JS::MutableHandle<JSObject*> aRetval,
                    ErrorResult& aRv)
{
  // Run the steps of the encoding algorithm.
  int32_t srcLen = aString.Length();
  int32_t maxLen;
  const char16_t* data = aString.BeginReading();
  nsresult rv = mEncoder->GetMaxLength(data, srcLen, &maxLen);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  // Need a fallible allocator because the caller may be a content
  // and the content can specify the length of the string.
  auto buf = MakeUniqueFallible<char[]>(maxLen + 1);
  if (!buf) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  int32_t dstLen = maxLen;
  rv = mEncoder->Convert(data, &srcLen, buf.get(), &dstLen);

  // Now reset the encoding algorithm state to the default values for encoding.
  int32_t finishLen = maxLen - dstLen;
  rv = mEncoder->Finish(&buf[dstLen], &finishLen);
  if (NS_SUCCEEDED(rv)) {
    dstLen += finishLen;
  }

  JSObject* outView = nullptr;
  if (NS_SUCCEEDED(rv)) {
    buf[dstLen] = '\0';
    JSAutoCompartment ac(aCx, aObj);
    outView = Uint8Array::Create(aCx, dstLen,
                                 reinterpret_cast<uint8_t*>(buf.get()));
    if (!outView) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  aRetval.set(outView);
}

void
TextEncoder::GetEncoding(nsAString& aEncoding)
{
  aEncoding.AssignLiteral("utf-8");
}

} // namespace dom
} // namespace mozilla
