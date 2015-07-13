/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextEncoder.h"
#include "mozilla/dom/EncodingUtils.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

void
TextEncoder::Init(const nsAString& aEncoding, ErrorResult& aRv)
{
  nsAutoString label(aEncoding);
  EncodingUtils::TrimSpaceCharacters(label);

  // Let encoding be the result of getting an encoding from label.
  // If encoding is failure, or is none of utf-8, utf-16, and utf-16be,
  // throw a RangeError (https://encoding.spec.whatwg.org/#dom-textencoder).
  if (!EncodingUtils::FindEncodingForLabel(label, mEncoding)) {
    aRv.ThrowRangeError(MSG_ENCODING_NOT_SUPPORTED, &label);
    return;
  }

  if (!mEncoding.EqualsLiteral("UTF-8") &&
      !mEncoding.EqualsLiteral("UTF-16LE") &&
      !mEncoding.EqualsLiteral("UTF-16BE")) {
    aRv.ThrowRangeError(MSG_DOM_ENCODING_NOT_UTF);
    return;
  }

  // Create an encoder object for mEncoding.
  mEncoder = EncodingUtils::EncoderForEncoding(mEncoding);
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
  const char16_t* data = PromiseFlatString(aString).get();
  nsresult rv = mEncoder->GetMaxLength(data, srcLen, &maxLen);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  // Need a fallible allocator because the caller may be a content
  // and the content can specify the length of the string.
  nsAutoArrayPtr<char> buf(new (fallible) char[maxLen + 1]);
  if (!buf) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  int32_t dstLen = maxLen;
  rv = mEncoder->Convert(data, &srcLen, buf, &dstLen);

  // Now reset the encoding algorithm state to the default values for encoding.
  int32_t finishLen = maxLen - dstLen;
  rv = mEncoder->Finish(buf + dstLen, &finishLen);
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
  CopyASCIItoUTF16(mEncoding, aEncoding);
  nsContentUtils::ASCIIToLower(aEncoding);
}

} // namespace dom
} // namespace mozilla
