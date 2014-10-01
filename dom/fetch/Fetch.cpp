/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

#include "nsIStringStream.h"
#include "nsIUnicodeEncoder.h"

#include "nsStringStream.h"

#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/URLSearchParams.h"

namespace mozilla {
namespace dom {

nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  nsresult rv;
  nsCOMPtr<nsIInputStream> byteStream;
  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    buf.ComputeLengthAndData();
    //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
    rv = NS_NewByteInputStream(getter_AddRefs(byteStream),
                               reinterpret_cast<char*>(buf.Data()),
                               buf.Length(), NS_ASSIGNMENT_COPY);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    buf.ComputeLengthAndData();
    //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
    rv = NS_NewByteInputStream(getter_AddRefs(byteStream),
                               reinterpret_cast<char*>(buf.Data()),
                               buf.Length(), NS_ASSIGNMENT_COPY);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (aBodyInit.IsScalarValueString()) {
    nsString str = aBodyInit.GetAsScalarValueString();

    nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
    if (!encoder) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    int32_t destBufferLen;
    rv = encoder->GetMaxLength(str.get(), str.Length(), &destBufferLen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString encoded;
    if (!encoded.SetCapacity(destBufferLen, fallible_t())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    char* destBuffer = encoded.BeginWriting();
    int32_t srcLen = (int32_t) str.Length();
    int32_t outLen = destBufferLen;
    rv = encoder->Convert(str.get(), &srcLen, destBuffer, &outLen);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(outLen <= destBufferLen);
    encoded.SetLength(outLen);
    rv = NS_NewCStringInputStream(getter_AddRefs(byteStream), encoded);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    aContentType = NS_LITERAL_CSTRING("text/plain;charset=UTF-8");
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    nsString serialized;
    params.Stringify(serialized);
    rv = NS_NewStringInputStream(getter_AddRefs(byteStream), serialized);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aContentType = NS_LITERAL_CSTRING("application/x-www-form-urlencoded;charset=UTF-8");
  }

  MOZ_ASSERT(byteStream);
  byteStream.forget(aStream);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
