/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BodyExtractor.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/XMLHttpRequest.h"
#include "nsContentUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSerializer.h"
#include "nsIGlobalObject.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStorageStream.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

static nsresult
GetBufferDataAsStream(const uint8_t* aData, uint32_t aDataLength,
                      nsIInputStream** aResult, uint64_t* aContentLength,
                      nsACString& aContentType, nsACString& aCharset)
{
  aContentType.SetIsVoid(true);
  aCharset.Truncate();

  *aContentLength = aDataLength;
  const char* data = reinterpret_cast<const char*>(aData);

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream), data, aDataLength,
                                      NS_ASSIGNMENT_COPY);
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aResult);

  return NS_OK;
}

template<> nsresult
BodyExtractor<const ArrayBuffer>::GetAsStream(nsIInputStream** aResult,
                                              uint64_t* aContentLength,
                                              nsACString& aContentTypeWithCharset,
                                              nsACString& aCharset) const
{
  mBody->ComputeLengthAndData();
  return GetBufferDataAsStream(mBody->Data(), mBody->Length(),
                               aResult, aContentLength, aContentTypeWithCharset,
                               aCharset);
}

template<> nsresult
BodyExtractor<const ArrayBufferView>::GetAsStream(nsIInputStream** aResult,
                                                  uint64_t* aContentLength,
                                                  nsACString& aContentTypeWithCharset,
                                                  nsACString& aCharset) const
{
  mBody->ComputeLengthAndData();
  return GetBufferDataAsStream(mBody->Data(), mBody->Length(),
                               aResult, aContentLength, aContentTypeWithCharset,
                               aCharset);
}

template<> nsresult
BodyExtractor<nsIDocument>::GetAsStream(nsIInputStream** aResult,
                                        uint64_t* aContentLength,
                                        nsACString& aContentTypeWithCharset,
                                        nsACString& aCharset) const
{
  nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(mBody));
  NS_ENSURE_STATE(domdoc);
  aCharset.AssignLiteral("UTF-8");

  nsresult rv;
  nsCOMPtr<nsIStorageStream> storStream;
  rv = NS_NewStorageStream(4096, UINT32_MAX, getter_AddRefs(storStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> output;
  rv = storStream->GetOutputStream(0, getter_AddRefs(output));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mBody->IsHTMLDocument()) {
    aContentTypeWithCharset.AssignLiteral("text/html;charset=UTF-8");

    nsString serialized;
    if (!nsContentUtils::SerializeNodeToMarkup(mBody, true, serialized)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsAutoCString utf8Serialized;
    if (!AppendUTF16toUTF8(serialized, utf8Serialized, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t written;
    rv = output->Write(utf8Serialized.get(), utf8Serialized.Length(), &written);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(written == utf8Serialized.Length());
  } else {
    aContentTypeWithCharset.AssignLiteral("application/xml;charset=UTF-8");

    nsCOMPtr<nsIDOMSerializer> serializer =
      do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure to use the encoding we'll send
    rv = serializer->SerializeToStream(domdoc, output, aCharset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  output->Close();

  uint32_t length;
  rv = storStream->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  *aContentLength = length;

  rv = storStream->NewInputStream(0, aResult);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

template<> nsresult
BodyExtractor<const nsAString>::GetAsStream(nsIInputStream** aResult,
                                            uint64_t* aContentLength,
                                            nsACString& aContentTypeWithCharset,
                                            nsACString& aCharset) const
{
  nsCString encoded;
  if (!CopyUTF16toUTF8(*mBody, encoded, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = NS_NewCStringInputStream(aResult, encoded);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aContentLength = encoded.Length();
  aContentTypeWithCharset.AssignLiteral("text/plain;charset=UTF-8");
  aCharset.AssignLiteral("UTF-8");
  return NS_OK;
}

template<> nsresult
BodyExtractor<nsIInputStream>::GetAsStream(nsIInputStream** aResult,
                                           uint64_t* aContentLength,
                                           nsACString& aContentTypeWithCharset,
                                           nsACString& aCharset) const
{
  aContentTypeWithCharset.AssignLiteral("text/plain");
  aCharset.Truncate();

  nsresult rv = mBody->Available(aContentLength);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream(mBody);
  stream.forget(aResult);
  return NS_OK;
}

template<> nsresult
BodyExtractor<nsIXHRSendable>::GetAsStream(nsIInputStream** aResult,
                                           uint64_t* aContentLength,
                                           nsACString& aContentTypeWithCharset,
                                           nsACString& aCharset) const
{
  return mBody->GetSendInfo(aResult, aContentLength, aContentTypeWithCharset,
                            aCharset);
}

template<> nsresult
BodyExtractor<const Blob>::GetAsStream(nsIInputStream** aResult,
                                       uint64_t* aContentLength,
                                       nsACString& aContentTypeWithCharset,
                                       nsACString& aCharset) const
{
  return mBody->GetSendInfo(aResult, aContentLength, aContentTypeWithCharset,
                            aCharset);
}

template<> nsresult
BodyExtractor<const FormData>::GetAsStream(nsIInputStream** aResult,
                                           uint64_t* aContentLength,
                                           nsACString& aContentTypeWithCharset,
                                           nsACString& aCharset) const
{
  return mBody->GetSendInfo(aResult, aContentLength, aContentTypeWithCharset,
                            aCharset);
}

template<> nsresult
BodyExtractor<const URLSearchParams>::GetAsStream(nsIInputStream** aResult,
                                                  uint64_t* aContentLength,
                                                  nsACString& aContentTypeWithCharset,
                                                  nsACString& aCharset) const
{
  return mBody->GetSendInfo(aResult, aContentLength, aContentTypeWithCharset,
                            aCharset);
}

} // dom namespace
} // mozilla namespace
