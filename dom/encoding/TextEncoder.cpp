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
                         const nsAString& aString,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv) {
  // Given nsTSubstring<char16_t>::kMaxCapacity, it should be
  // impossible for the length computation to overflow, but
  // let's use checked math in case someone changes something
  // in the future.
  // Uint8Array::Create takes uint32_t as the length.
  CheckedInt<uint32_t> bufLen(aString.Length());
  bufLen *= 3;  // from the contract for ConvertUTF16toUTF8
  if (!bufLen.isValid()) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  auto data = mozilla::MakeUniqueFallible<uint8_t[]>(bufLen.value());
  if (!data) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  size_t utf8Len = ConvertUTF16toUTF8(
      aString, MakeSpan(reinterpret_cast<char*>(data.get()), bufLen.value()));
  MOZ_ASSERT(utf8Len <= bufLen.value());

  JSAutoRealm ar(aCx, aObj);
  JSObject* outView = Uint8Array::Create(aCx, utf8Len, data.get());
  if (!outView) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  aRetval.set(outView);
}

void TextEncoder::EncodeInto(const nsAString& aSrc, const Uint8Array& aDst,
                             TextEncoderEncodeIntoResult& aResult) {
  aDst.ComputeLengthAndData();
  size_t read;
  size_t written;
  Tie(read, written) = ConvertUTF16toUTF8Partial(
      aSrc, MakeSpan(reinterpret_cast<char*>(aDst.Data()), aDst.Length()));
  aResult.mRead.Construct() = read;
  aResult.mWritten.Construct() = written;
}

void TextEncoder::GetEncoding(nsAString& aEncoding) {
  aEncoding.AssignLiteral("utf-8");
}

}  // namespace dom
}  // namespace mozilla
