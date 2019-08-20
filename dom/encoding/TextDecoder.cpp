/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextDecoder.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/Encoding.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsContentUtils.h"
#include <stdint.h>

namespace mozilla {
namespace dom {

void TextDecoder::Init(const nsAString& aLabel,
                       const TextDecoderOptions& aOptions, ErrorResult& aRv) {
  // Let encoding be the result of getting an encoding from label.
  // If encoding is failure or replacement, throw a RangeError
  // (https://encoding.spec.whatwg.org/#dom-textdecoder).
  const Encoding* encoding = Encoding::ForLabelNoReplacement(aLabel);
  if (!encoding) {
    nsAutoString label(aLabel);
    label.Trim(" \t\n\f\r");
    aRv.ThrowRangeError<MSG_ENCODING_NOT_SUPPORTED>(label);
    return;
  }
  InitWithEncoding(WrapNotNull(encoding), aOptions);
}

void TextDecoder::InitWithEncoding(NotNull<const Encoding*> aEncoding,
                                   const TextDecoderOptions& aOptions) {
  aEncoding->Name(mEncoding);
  // Store the flags passed via our options dictionary.
  mFatal = aOptions.mFatal;
  mIgnoreBOM = aOptions.mIgnoreBOM;

  // Create a decoder object for mEncoding.
  if (mIgnoreBOM) {
    mDecoder = aEncoding->NewDecoderWithoutBOMHandling();
  } else {
    mDecoder = aEncoding->NewDecoderWithBOMRemoval();
  }
}

void TextDecoder::Decode(Span<const uint8_t> aInput, const bool aStream,
                         nsAString& aOutDecodedString, ErrorResult& aRv) {
  aOutDecodedString.Truncate();

  CheckedInt<size_t> needed = mDecoder->MaxUTF16BufferLength(aInput.Length());
  if (!needed.isValid() ||
      needed.value() > MaxValue<nsAString::size_type>::value) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  if (!aOutDecodedString.SetLength(needed.value(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  uint32_t result;
  size_t read;
  size_t written;
  bool hadErrors;
  if (mFatal) {
    Tie(result, read, written) = mDecoder->DecodeToUTF16WithoutReplacement(
        aInput, aOutDecodedString, !aStream);
    if (result != kInputEmpty) {
      aRv.ThrowTypeError<MSG_DOM_DECODING_FAILED>();
      return;
    }
  } else {
    Tie(result, read, written, hadErrors) =
        mDecoder->DecodeToUTF16(aInput, aOutDecodedString, !aStream);
  }
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == aInput.Length());
  MOZ_ASSERT(written <= aOutDecodedString.Length());
  Unused << hadErrors;

  if (!aOutDecodedString.SetLength(written, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // If the internal streaming flag of the decoder object is not set,
  // then reset the encoding algorithm state to the default values
  if (!aStream) {
    if (mIgnoreBOM) {
      mDecoder->Encoding()->NewDecoderWithoutBOMHandlingInto(*mDecoder);
    } else {
      mDecoder->Encoding()->NewDecoderWithBOMRemovalInto(*mDecoder);
    }
  }
}

void TextDecoder::Decode(const Optional<ArrayBufferViewOrArrayBuffer>& aBuffer,
                         const TextDecodeOptions& aOptions,
                         nsAString& aOutDecodedString, ErrorResult& aRv) {
  if (!aBuffer.WasPassed()) {
    Decode(nullptr, aOptions.mStream, aOutDecodedString, aRv);
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
  Decode(MakeSpan(data, length), aOptions.mStream, aOutDecodedString, aRv);
}

void TextDecoder::GetEncoding(nsAString& aEncoding) {
  CopyASCIItoUTF16(mEncoding, aEncoding);
  nsContentUtils::ASCIIToLower(aEncoding);
}

}  // namespace dom
}  // namespace mozilla
