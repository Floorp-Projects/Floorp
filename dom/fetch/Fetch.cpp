/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Fetch.h"

#include "nsIStringStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

#include "nsDOMString.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/URLSearchParams.h"

namespace mozilla {
namespace dom {

namespace {
nsresult
ExtractFromArrayBuffer(const ArrayBuffer& aBuffer,
                       nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromArrayBufferView(const ArrayBufferView& aBuffer,
                           nsIInputStream** aStream)
{
  aBuffer.ComputeLengthAndData();
  //XXXnsm reinterpret_cast<> is used in DOMParser, should be ok.
  return NS_NewByteInputStream(aStream,
                               reinterpret_cast<char*>(aBuffer.Data()),
                               aBuffer.Length(), NS_ASSIGNMENT_COPY);
}

nsresult
ExtractFromBlob(const File& aFile, nsIInputStream** aStream,
                nsCString& aContentType)
{
  nsRefPtr<FileImpl> impl = aFile.Impl();
  nsresult rv = impl->GetInternalStream(aStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString type;
  impl->GetType(type);
  aContentType = NS_ConvertUTF16toUTF8(type);
  return NS_OK;
}

nsresult
ExtractFromScalarValueString(const nsString& aStr,
                             nsIInputStream** aStream,
                             nsCString& aContentType)
{
  nsCOMPtr<nsIUnicodeEncoder> encoder = EncodingUtils::EncoderForEncoding("UTF-8");
  if (!encoder) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  int32_t destBufferLen;
  nsresult rv = encoder->GetMaxLength(aStr.get(), aStr.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString encoded;
  if (!encoded.SetCapacity(destBufferLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char* destBuffer = encoded.BeginWriting();
  int32_t srcLen = (int32_t) aStr.Length();
  int32_t outLen = destBufferLen;
  rv = encoder->Convert(aStr.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  encoded.SetLength(outLen);

  aContentType = NS_LITERAL_CSTRING("text/plain;charset=UTF-8");

  return NS_NewCStringInputStream(aStream, encoded);
}

nsresult
ExtractFromURLSearchParams(const URLSearchParams& aParams,
                           nsIInputStream** aStream,
                           nsCString& aContentType)
{
  nsAutoString serialized;
  aParams.Stringify(serialized);
  aContentType = NS_LITERAL_CSTRING("application/x-www-form-urlencoded;charset=UTF-8");
  return NS_NewStringInputStream(aStream, serialized);
}
}

nsresult
ExtractByteStreamFromBody(const OwningArrayBufferOrArrayBufferViewOrBlobOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsScalarValueString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsScalarValueString());
    return ExtractFromScalarValueString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

nsresult
ExtractByteStreamFromBody(const ArrayBufferOrArrayBufferViewOrBlobOrScalarValueStringOrURLSearchParams& aBodyInit,
                          nsIInputStream** aStream,
                          nsCString& aContentType)
{
  MOZ_ASSERT(aStream);

  if (aBodyInit.IsArrayBuffer()) {
    const ArrayBuffer& buf = aBodyInit.GetAsArrayBuffer();
    return ExtractFromArrayBuffer(buf, aStream);
  } else if (aBodyInit.IsArrayBufferView()) {
    const ArrayBufferView& buf = aBodyInit.GetAsArrayBufferView();
    return ExtractFromArrayBufferView(buf, aStream);
  } else if (aBodyInit.IsBlob()) {
    const File& blob = aBodyInit.GetAsBlob();
    return ExtractFromBlob(blob, aStream, aContentType);
  } else if (aBodyInit.IsScalarValueString()) {
    nsAutoString str;
    str.Assign(aBodyInit.GetAsScalarValueString());
    return ExtractFromScalarValueString(str, aStream, aContentType);
  } else if (aBodyInit.IsURLSearchParams()) {
    URLSearchParams& params = aBodyInit.GetAsURLSearchParams();
    return ExtractFromURLSearchParams(params, aStream, aContentType);
  }

  NS_NOTREACHED("Should never reach here");
  return NS_ERROR_FAILURE;
}

namespace {
nsresult
DecodeUTF8(const nsCString& aBuffer, nsString& aDecoded)
{
  nsCOMPtr<nsIUnicodeDecoder> decoder =
    EncodingUtils::DecoderForEncoding("UTF-8");
  if (!decoder) {
    return NS_ERROR_FAILURE;
  }

  int32_t destBufferLen;
  nsresult rv =
    decoder->GetMaxLength(aBuffer.get(), aBuffer.Length(), &destBufferLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aDecoded.SetCapacity(destBufferLen, fallible_t())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char16_t* destBuffer = aDecoded.BeginWriting();
  int32_t srcLen = (int32_t) aBuffer.Length();
  int32_t outLen = destBufferLen;
  rv = decoder->Convert(aBuffer.get(), &srcLen, destBuffer, &outLen);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(outLen <= destBufferLen);
  aDecoded.SetLength(outLen);
  return NS_OK;
}
}

template <class Derived>
already_AddRefed<Promise>
FetchBody<Derived>::ConsumeBody(ConsumeType aType, ErrorResult& aRv)
{
  nsRefPtr<Promise> promise = Promise::Create(DerivedClass()->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (BodyUsed()) {
    aRv.ThrowTypeError(MSG_REQUEST_BODY_CONSUMED_ERROR);
    return nullptr;
  }

  SetBodyUsed();

  // While the spec says to do this asynchronously, all the body constructors
  // right now only accept bodies whose streams are backed by an in-memory
  // buffer that can be read without blocking. So I think this is fine.
  nsCOMPtr<nsIInputStream> stream;
  DerivedClass()->GetBody(getter_AddRefs(stream));

  if (!stream) {
    aRv = NS_NewByteInputStream(getter_AddRefs(stream), "", 0,
                                NS_ASSIGNMENT_COPY);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  AutoJSAPI api;
  api.Init(DerivedClass()->GetParentObject());
  JSContext* cx = api.cx();

  // We can make this assertion because for now we only support memory backed
  // structures for the body argument for a Request.
  MOZ_ASSERT(NS_InputStreamIsBuffered(stream));
  nsCString buffer;
  uint64_t len;
  aRv = stream->Available(&len);
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = NS_ReadInputStreamToString(stream, buffer, len);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  buffer.SetLength(len);

  switch (aType) {
    case CONSUME_ARRAYBUFFER: {
      JS::Rooted<JSObject*> arrayBuffer(cx);
      arrayBuffer =
        ArrayBuffer::Create(cx, buffer.Length(),
                            reinterpret_cast<const uint8_t*>(buffer.get()));
      JS::Rooted<JS::Value> val(cx);
      val.setObjectOrNull(arrayBuffer);
      promise->MaybeResolve(cx, val);
      return promise.forget();
    }
    case CONSUME_BLOB: {
      // XXXnsm it is actually possible to avoid these duplicate allocations
      // for the Blob case by having the Blob adopt the stream's memory
      // directly, but I've not added a special case for now.
      //
      // FIXME(nsm): Use nsContentUtils::CreateBlobBuffer once blobs have been fixed on
      // workers.
      uint32_t blobLen = buffer.Length();
      void* blobData = moz_malloc(blobLen);
      nsRefPtr<File> blob;
      if (blobData) {
        memcpy(blobData, buffer.BeginReading(), blobLen);
        blob = File::CreateMemoryFile(DerivedClass()->GetParentObject(), blobData, blobLen,
                                      NS_ConvertUTF8toUTF16(mMimeType));
      } else {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return nullptr;
      }

      promise->MaybeResolve(blob);
      return promise.forget();
    }
    case CONSUME_JSON: {
      nsAutoString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      JS::Rooted<JS::Value> json(cx);
      if (!JS_ParseJSON(cx, decoded.get(), decoded.Length(), &json)) {
        JS::Rooted<JS::Value> exn(cx);
        if (JS_GetPendingException(cx, &exn)) {
          JS_ClearPendingException(cx);
          promise->MaybeReject(cx, exn);
        }
      }
      promise->MaybeResolve(cx, json);
      return promise.forget();
    }
    case CONSUME_TEXT: {
      nsAutoString decoded;
      aRv = DecodeUTF8(buffer, decoded);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      promise->MaybeResolve(decoded);
      return promise.forget();
    }
  }

  NS_NOTREACHED("Unexpected consume body type");
  // Silence warnings.
  return nullptr;
}

template
already_AddRefed<Promise>
FetchBody<Request>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template
already_AddRefed<Promise>
FetchBody<Response>::ConsumeBody(ConsumeType aType, ErrorResult& aRv);

template <class Derived>
void
FetchBody<Derived>::SetMimeType(ErrorResult& aRv)
{
  // Extract mime type.
  nsTArray<nsCString> contentTypeValues;
  MOZ_ASSERT(DerivedClass()->GetInternalHeaders());
  DerivedClass()->GetInternalHeaders()->GetAll(NS_LITERAL_CSTRING("Content-Type"), contentTypeValues, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // HTTP ABNF states Content-Type may have only one value.
  // This is from the "parse a header value" of the fetch spec.
  if (contentTypeValues.Length() == 1) {
    mMimeType = contentTypeValues[0];
    ToLowerCase(mMimeType);
  }
}

template
void
FetchBody<Request>::SetMimeType(ErrorResult& aRv);

template
void
FetchBody<Response>::SetMimeType(ErrorResult& aRv);
} // namespace dom
} // namespace mozilla
