/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReaderSync.h"

#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsIDOMFile.h"
#include "nsICharsetDetector.h"
#include "nsIConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsIPlatformCharset.h"
#include "nsISeekableStream.h"
#include "nsISupportsImpl.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "File.h"
#include "RuntimeService.h"
#include "DOMBindingInlines.h"

#include "mozilla/Base64.h"
#include "mozilla/dom/EncodingUtils.h"

USING_WORKERS_NAMESPACE
using namespace mozilla;
using mozilla::dom::Optional;
using mozilla::dom::WorkerGlobalObject;

NS_IMPL_ADDREF_INHERITED(FileReaderSync, DOMBindingBase)
NS_IMPL_RELEASE_INHERITED(FileReaderSync, DOMBindingBase)
NS_INTERFACE_MAP_BEGIN(FileReaderSync)
  NS_INTERFACE_MAP_ENTRY(nsICharsetDetectionObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMBindingBase)

FileReaderSync::FileReaderSync(JSContext* aCx)
  : DOMBindingBase(aCx)
{
}

void
FileReaderSync::_trace(JSTracer* aTrc)
{
  DOMBindingBase::_trace(aTrc);
}

void
FileReaderSync::_finalize(JSFreeOp* aFop)
{
  DOMBindingBase::_finalize(aFop);
}

// static
FileReaderSync*
FileReaderSync::Constructor(const WorkerGlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<FileReaderSync> frs = new FileReaderSync(aGlobal.GetContext());

  if (!Wrap(aGlobal.GetContext(), aGlobal.Get(), frs)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return frs;
}

JSObject*
FileReaderSync::ReadAsArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aBlob,
                                  ErrorResult& aRv)
{
  nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return nullptr;
  }

  uint64_t blobSize;
  nsresult rv = blob->GetSize(&blobSize);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  JS::Rooted<JSObject*> jsArrayBuffer(aCx, JS_NewArrayBuffer(aCx, blobSize));
  if (!jsArrayBuffer) {
    // XXXkhuey we need a way to indicate to the bindings that the call failed
    // but there's already a pending exception that we should not clobber.
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  uint32_t bufferLength = JS_GetArrayBufferByteLength(jsArrayBuffer);
  uint8_t* arrayBuffer = JS_GetArrayBufferData(jsArrayBuffer);

  nsCOMPtr<nsIInputStream> stream;
  rv = blob->GetInternalStream(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  uint32_t numRead;
  rv = stream->Read((char*)arrayBuffer, bufferLength, &numRead);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  NS_ASSERTION(numRead == bufferLength, "failed to read data");

  return jsArrayBuffer;
}

void
FileReaderSync::ReadAsBinaryString(JS::Handle<JSObject*> aBlob,
                                   nsAString& aResult,
                                   ErrorResult& aRv)
{
  nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = blob->GetInternalStream(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint32_t numRead;
  do {
    char readBuf[4096];
    rv = stream->Read(readBuf, sizeof(readBuf), &numRead);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    uint32_t oldLength = aResult.Length();
    AppendASCIItoUTF16(Substring(readBuf, readBuf + numRead), aResult);
    if (aResult.Length() - oldLength != numRead) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
  } while (numRead > 0);
}

void
FileReaderSync::ReadAsText(JS::Handle<JSObject*> aBlob,
                           const Optional<nsAString>& aEncoding,
                           nsAString& aResult,
                           ErrorResult& aRv)
{
  nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = blob->GetInternalStream(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsCString charsetGuess;
  if (!aEncoding.WasPassed() || aEncoding.Value().IsEmpty()) {
    rv = GuessCharset(stream, charsetGuess);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }

    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(stream);
    if (!seekable) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // Seek to 0 because guessing the charset advances the stream.
    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  } else {
    CopyUTF16toUTF8(aEncoding.Value(), charsetGuess);
  }

  nsCString charset;
  if (!EncodingUtils::FindEncodingForLabel(charsetGuess, charset)) {
    aRv.Throw(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
    return;
  }

  rv = ConvertStream(stream, charset.get(), aResult);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
}

void
FileReaderSync::ReadAsDataURL(JS::Handle<JSObject*> aBlob, nsAString& aResult,
                              ErrorResult& aRv)
{
  nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aBlob);
  if (!blob) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  nsAutoString scratchResult;
  scratchResult.AssignLiteral("data:");

  nsString contentType;
  blob->GetType(contentType);

  if (contentType.IsEmpty()) {
    scratchResult.AppendLiteral("application/octet-stream");
  } else {
    scratchResult.Append(contentType);
  }
  scratchResult.AppendLiteral(";base64,");

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = blob->GetInternalStream(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint64_t size;
  rv = blob->GetSize(&size);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, size);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsAutoString encodedData;
  rv = Base64EncodeInputStream(bufferedStream, encodedData, size);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  scratchResult.Append(encodedData);

  aResult = scratchResult;
}

nsresult
FileReaderSync::ConvertStream(nsIInputStream *aStream,
                              const char *aCharset,
                              nsAString &aResult)
{
  nsCOMPtr<nsIConverterInputStream> converterStream =
    do_CreateInstance("@mozilla.org/intl/converter-input-stream;1");
  NS_ENSURE_TRUE(converterStream, NS_ERROR_FAILURE);

  nsresult rv = converterStream->Init(aStream, aCharset, 8192,
                  nsIConverterInputStream::DEFAULT_REPLACEMENT_CHARACTER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicharInputStream> unicharStream =
    do_QueryInterface(converterStream);
  NS_ENSURE_TRUE(unicharStream, NS_ERROR_FAILURE);

  uint32_t numChars;
  nsString result;
  while (NS_SUCCEEDED(unicharStream->ReadString(8192, result, &numChars)) &&
         numChars > 0) {
    uint32_t oldLength = aResult.Length();
    aResult.Append(result);
    if (aResult.Length() - oldLength != result.Length()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

nsresult
FileReaderSync::GuessCharset(nsIInputStream *aStream, nsACString &aCharset)
{
  // First try the universal charset detector
  nsCOMPtr<nsICharsetDetector> detector
    = do_CreateInstance(NS_CHARSET_DETECTOR_CONTRACTID_BASE
                        "universal_charset_detector");
  if (!detector) {
    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    // No universal charset detector, try the default charset detector
    const nsACString& detectorName = runtime->GetDetectorName();

    if (!detectorName.IsEmpty()) {
      nsAutoCString detectorContractID;
      detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
      detectorContractID += detectorName;
      detector = do_CreateInstance(detectorContractID.get());
    }
  }

  nsresult rv;
  if (detector) {
    detector->Init(this);

    bool done;
    uint32_t numRead;
    do {
      char readBuf[4096];
      rv = aStream->Read(readBuf, sizeof(readBuf), &numRead);
      NS_ENSURE_SUCCESS(rv, rv);
      if (numRead <= 0) {
        break;
      }
      rv = detector->DoIt(readBuf, numRead, &done);
      NS_ENSURE_SUCCESS(rv, rv);
    } while (!done);

    rv = detector->Done();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // no charset detector available, check the BOM
    unsigned char sniffBuf[4];
    uint32_t numRead;
    rv = aStream->Read(reinterpret_cast<char*>(sniffBuf),
                       sizeof(sniffBuf), &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    if (numRead >= 2 &&
        sniffBuf[0] == 0xfe &&
        sniffBuf[1] == 0xff) {
      mCharset = "UTF-16BE";
    } else if (numRead >= 2 &&
               sniffBuf[0] == 0xff &&
               sniffBuf[1] == 0xfe) {
      mCharset = "UTF-16LE";
    } else if (numRead >= 3 &&
               sniffBuf[0] == 0xef &&
               sniffBuf[1] == 0xbb &&
               sniffBuf[2] == 0xbf) {
      mCharset = "UTF-8";
    }
  }

  if (mCharset.IsEmpty()) {
    RuntimeService* runtime = RuntimeService::GetService();
    mCharset = runtime->GetSystemCharset();
  }

  if (mCharset.IsEmpty()) {
    // no sniffed or default charset, try UTF-8
    mCharset.AssignLiteral("UTF-8");
  }

  aCharset = mCharset;
  mCharset.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
FileReaderSync::Notify(const char* aCharset, nsDetectionConfident aConf)
{
  mCharset.Assign(aCharset);

  return NS_OK;
}
