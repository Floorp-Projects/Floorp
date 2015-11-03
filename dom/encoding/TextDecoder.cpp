/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextDecoder.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsContentUtils.h"
#include <stdint.h>

namespace mozilla {
namespace dom {

static const char16_t kReplacementChar = static_cast<char16_t>(0xFFFD);

void
TextDecoder::Init(const nsAString& aLabel, const bool aFatal,
                  ErrorResult& aRv)
{
  nsAutoCString encoding;
  // Let encoding be the result of getting an encoding from label.
  // If encoding is failure or replacement, throw a RangeError
  // (https://encoding.spec.whatwg.org/#dom-textdecoder).
  if (!EncodingUtils::FindEncodingForLabelNoReplacement(aLabel, encoding)) {
    nsAutoString label(aLabel);
    EncodingUtils::TrimSpaceCharacters(label);
    aRv.ThrowRangeError<MSG_ENCODING_NOT_SUPPORTED>(label);
    return;
  }
  InitWithEncoding(encoding, aFatal);
}

void
TextDecoder::InitWithEncoding(const nsACString& aEncoding, const bool aFatal)
{
  mEncoding = aEncoding;
  // If the constructor is called with an options argument,
  // and the fatal property of the dictionary is set,
  // set the internal fatal flag of the decoder object.
  mFatal = aFatal;

  // Create a decoder object for mEncoding.
  mDecoder = EncodingUtils::DecoderForEncoding(mEncoding);

  if (mFatal) {
    mDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Signal);
  }
}

void
TextDecoder::Decode(const char* aInput, const int32_t aLength,
                    const bool aStream, nsAString& aOutDecodedString,
                    ErrorResult& aRv)
{
  aOutDecodedString.Truncate();

  // Run or resume the decoder algorithm of the decoder object's encoder.
  int32_t outLen;
  nsresult rv = mDecoder->GetMaxLength(aInput, aLength, &outLen);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  // Need a fallible allocator because the caller may be a content
  // and the content can specify the length of the string.
  auto buf = MakeUniqueFallible<char16_t[]>(outLen + 1);
  if (!buf) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  int32_t length = aLength;
  rv = mDecoder->Convert(aInput, &length, buf.get(), &outLen);
  MOZ_ASSERT(mFatal || rv != NS_ERROR_ILLEGAL_INPUT);
  buf[outLen] = 0;

  if (!aOutDecodedString.Append(buf.get(), outLen, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // If the internal streaming flag of the decoder object is not set,
  // then reset the encoding algorithm state to the default values
  if (!aStream) {
    mDecoder->Reset();
    if (rv == NS_OK_UDEC_MOREINPUT) {
      if (mFatal) {
        aRv.ThrowTypeError<MSG_DOM_DECODING_FAILED>();
      } else {
        // Need to emit a decode error manually
        // to simulate the EOF handling of the Encoding spec.
        aOutDecodedString.Append(kReplacementChar);
      }
    }
  }

  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_DOM_DECODING_FAILED>();
  }
}

void
TextDecoder::Decode(const Optional<ArrayBufferViewOrArrayBuffer>& aBuffer,
                    const TextDecodeOptions& aOptions,
                    nsAString& aOutDecodedString,
                    ErrorResult& aRv)
{
  if (!aBuffer.WasPassed()) {
    Decode(nullptr, 0, aOptions.mStream, aOutDecodedString, aRv);
    return;
  }
  const ArrayBufferViewOrArrayBuffer& buf = aBuffer.Value();
  uint8_t* data;
  uint32_t length;
  if (buf.IsArrayBufferView()) {
    buf.GetAsArrayBufferView().ComputeLengthAndData();
    data = buf.GetAsArrayBufferView().Data();
    length = buf.GetAsArrayBufferView().Length();
  } else {
    MOZ_ASSERT(buf.IsArrayBuffer());
    buf.GetAsArrayBuffer().ComputeLengthAndData();
    data = buf.GetAsArrayBuffer().Data();
    length = buf.GetAsArrayBuffer().Length();
  }
  // The other Decode signature takes a signed int, because that's
  // what nsIUnicodeDecoder::Convert takes as the length.  Throw if
  // our length is too big.
  if (length > INT32_MAX) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  Decode(reinterpret_cast<char*>(data), length, aOptions.mStream,
         aOutDecodedString, aRv);
}

void
TextDecoder::GetEncoding(nsAString& aEncoding)
{
  CopyASCIItoUTF16(mEncoding, aEncoding);
  nsContentUtils::ASCIIToLower(aEncoding);
}

} // namespace dom
} // namespace mozilla
