/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextEncoder.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsReadableUtils.h"

namespace mozilla {
namespace dom {

void TextEncoder::Encode(JSContext* aCx, JS::Handle<JSObject*> aObj,
                         JS::Handle<JSString*> aString,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv) {
  CheckedInt<size_t> bufLen(JS::GetStringLength(aString));
  bufLen *= 3;  // from the contract for JS_EncodeStringToUTF8BufferPartial
  // Uint8Array::Create takes uint32_t as the length.
  if (!bufLen.isValid() || bufLen.value() > UINT32_MAX) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // TODO: Avoid malloc and use a stack-allocated buffer if bufLen
  // is small.
  auto data = mozilla::MakeUniqueFallible<uint8_t[]>(bufLen.value());
  if (!data) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  size_t read;
  size_t written;
  auto maybe = JS_EncodeStringToUTF8BufferPartial(
      aCx, aString, AsWritableChars(MakeSpan(data.get(), bufLen.value())));
  if (!maybe) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  Tie(read, written) = *maybe;
  MOZ_ASSERT(written <= bufLen.value());
  MOZ_ASSERT(read == JS::GetStringLength(aString));

  JSAutoRealm ar(aCx, aObj);
  JSObject* outView = Uint8Array::Create(aCx, written, data.get());
  if (!outView) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aRetval.set(outView);
}

void TextEncoder::EncodeInto(JSContext* aCx, JS::Handle<JSString*> aSrc,
                             const Uint8Array& aDst,
                             TextEncoderEncodeIntoResult& aResult,
                             OOMReporter& aError) {
  aDst.ComputeLengthAndData();
  size_t read;
  size_t written;
  auto maybe = JS_EncodeStringToUTF8BufferPartial(
      aCx, aSrc, AsWritableChars(MakeSpan(aDst.Data(), aDst.Length())));
  if (!maybe) {
    aError.ReportOOM();
    return;
  }
  Tie(read, written) = *maybe;
  MOZ_ASSERT(written <= aDst.Length());
  aResult.mRead.Construct() = read;
  aResult.mWritten.Construct() = written;
}

void TextEncoder::GetEncoding(nsACString& aEncoding) {
  aEncoding.AssignLiteral("utf-8");
}

}  // namespace dom
}  // namespace mozilla
