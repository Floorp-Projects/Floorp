/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReaderSync.h"

#include "jsfriendapi.h"
#include "mozilla/Unused.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/File.h"
#include "mozilla/Encoding.h"
#include "mozilla/dom/FileReaderSyncBinding.h"
#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsIAsyncInputStream.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::Optional;
using mozilla::dom::GlobalObject;

// static
already_AddRefed<FileReaderSync>
FileReaderSync::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  RefPtr<FileReaderSync> frs = new FileReaderSync();

  return frs.forget();
}

bool
FileReaderSync::WrapObject(JSContext* aCx,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector)
{
  return FileReaderSync_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

void
FileReaderSync::ReadAsArrayBuffer(JSContext* aCx,
                                  JS::Handle<JSObject*> aScopeObj,
                                  Blob& aBlob,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv)
{
  uint64_t blobSize = aBlob.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  UniquePtr<char[], JS::FreePolicy> bufferData(js_pod_malloc<char>(blobSize));
  if (!bufferData) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  aBlob.CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint32_t numRead;
  aRv = SyncRead(stream, bufferData.get(), blobSize, &numRead);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The file is changed in the meantime?
  if (numRead != blobSize) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  JSObject* arrayBuffer = JS_NewArrayBufferWithContents(aCx, blobSize, bufferData.get());
  if (!arrayBuffer) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  // arrayBuffer takes the ownership when it is not null. Otherwise we
  // need to release it explicitly.
  mozilla::Unused << bufferData.release();

  aRetval.set(arrayBuffer);
}

void
FileReaderSync::ReadAsBinaryString(Blob& aBlob,
                                   nsAString& aResult,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream;
  aBlob.CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint32_t numRead;
  do {
    char readBuf[4096];
    aRv = SyncRead(stream, readBuf, sizeof(readBuf), &numRead);
    if (NS_WARN_IF(aRv.Failed())) {
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
FileReaderSync::ReadAsText(Blob& aBlob,
                           const Optional<nsAString>& aEncoding,
                           nsAString& aResult,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> stream;
  aBlob.CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCString sniffBuf;
  if (!sniffBuf.SetLength(3, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  uint32_t numRead = 0;
  aRv = SyncRead(stream, sniffBuf.BeginWriting(), sniffBuf.Length(), &numRead);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // No data, we don't need to continue.
  if (numRead == 0) {
    aResult.Truncate();
    return;
  }

  // Try the API argument.
  const Encoding* encoding = aEncoding.WasPassed() ?
    Encoding::ForLabel(aEncoding.Value()) : nullptr;
  if (!encoding) {
    // API argument failed. Try the type property of the blob.
    nsAutoString type16;
    aBlob.GetType(type16);
    NS_ConvertUTF16toUTF8 type(type16);
    nsAutoCString specifiedCharset;
    bool haveCharset;
    int32_t charsetStart, charsetEnd;
    NS_ExtractCharsetFromContentType(type,
                                     specifiedCharset,
                                     &haveCharset,
                                     &charsetStart,
                                     &charsetEnd);
    encoding = Encoding::ForLabel(specifiedCharset);
    if (!encoding) {
      // Type property failed. Use UTF-8.
      encoding = UTF_8_ENCODING;
    }
  }

  if (numRead < sniffBuf.Length()) {
    sniffBuf.Truncate(numRead);
  }

  // Let's recreate the full stream using a:
  // multiplexStream(syncStream + original stream)
  // In theory, we could try to see if the inputStream is a nsISeekableStream,
  // but this doesn't work correctly for nsPipe3 - See bug 1349570.

  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  if (NS_WARN_IF(!multiplexStream)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIInputStream> sniffStringStream;
  aRv = NS_NewCStringInputStream(getter_AddRefs(sniffStringStream), sniffBuf);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = multiplexStream->AppendStream(sniffStringStream);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint64_t blobSize = aBlob.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  nsCOMPtr<nsIInputStream> syncStream;
  aRv = ConvertAsyncToSyncStream(blobSize - sniffBuf.Length(), stream.forget(),
                                 getter_AddRefs(syncStream));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // ConvertAsyncToSyncStream returns a null syncStream if the stream has been
  // already closed or there is nothing to read.
  if (syncStream) {
    aRv = multiplexStream->AppendStream(syncStream);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  nsAutoCString charset;
  encoding->Name(charset);

  nsCOMPtr<nsIInputStream> multiplex(do_QueryInterface(multiplexStream));
  aRv = ConvertStream(multiplex, charset.get(), aResult);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

void
FileReaderSync::ReadAsDataURL(Blob& aBlob, nsAString& aResult,
                              ErrorResult& aRv)
{
  nsAutoString scratchResult;
  scratchResult.AssignLiteral("data:");

  nsString contentType;
  aBlob.GetType(contentType);

  if (contentType.IsEmpty()) {
    scratchResult.AppendLiteral("application/octet-stream");
  } else {
    scratchResult.Append(contentType);
  }
  scratchResult.AppendLiteral(";base64,");

  nsCOMPtr<nsIInputStream> stream;
  aBlob.CreateInputStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  uint64_t blobSize = aBlob.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())){
    return;
  }

  nsCOMPtr<nsIInputStream> syncStream;
  aRv = ConvertAsyncToSyncStream(blobSize, stream.forget(),
                                 getter_AddRefs(syncStream));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(syncStream);

  uint64_t size;
  aRv = syncStream->Available(&size);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The file is changed in the meantime?
  if (blobSize != size) {
    return;
  }

  nsAutoString encodedData;
  aRv = Base64EncodeInputStream(syncStream, encodedData, size);
  if (NS_WARN_IF(aRv.Failed())){
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

namespace {

// This runnable is used to terminate the sync event loop.
class ReadReadyRunnable final : public WorkerSyncRunnable
{
public:
  ReadReadyRunnable(WorkerPrivate* aWorkerPrivate,
                    nsIEventTarget* aSyncLoopTarget)
    : WorkerSyncRunnable(aWorkerPrivate, aSyncLoopTarget)
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(mSyncLoopTarget);

    nsCOMPtr<nsIEventTarget> syncLoopTarget;
    mSyncLoopTarget.swap(syncLoopTarget);

    aWorkerPrivate->StopSyncLoop(syncLoopTarget, true);
    return true;
  }

private:
  ~ReadReadyRunnable()
  {}
};

// This class implements nsIInputStreamCallback and it will be called when the
// stream is ready to be read.
class ReadCallback final : public nsIInputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  ReadCallback(WorkerPrivate* aWorkerPrivate, nsIEventTarget* aEventTarget)
    : mWorkerPrivate(aWorkerPrivate)
    , mEventTarget(aEventTarget)
  {}

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override
  {
    // I/O Thread. Now we need to block the sync event loop.
    RefPtr<ReadReadyRunnable> runnable =
      new ReadReadyRunnable(mWorkerPrivate, mEventTarget);
    return mEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }

private:
  ~ReadCallback()
  {}

  // The worker is kept alive because of the sync event loop.
  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

NS_IMPL_ADDREF(ReadCallback);
NS_IMPL_RELEASE(ReadCallback);

NS_INTERFACE_MAP_BEGIN(ReadCallback)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStreamCallback)
NS_INTERFACE_MAP_END

} // anonymous

nsresult
FileReaderSync::SyncRead(nsIInputStream* aStream, char* aBuffer,
                         uint32_t aBufferSize, uint32_t* aRead)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aBuffer);
  MOZ_ASSERT(aRead);

  // Let's try to read, directly.
  nsresult rv = aStream->Read(aBuffer, aBufferSize, aRead);

  // Nothing else to read.
  if (rv == NS_BASE_STREAM_CLOSED ||
      (NS_SUCCEEDED(rv) && *aRead == 0)) {
    return NS_OK;
  }

  // An error.
  if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK) {
    return rv;
  }

  // All good.
  if (NS_SUCCEEDED(rv)) {
    // Not enough data, let's read recursively.
    if (*aRead != aBufferSize) {
      uint32_t byteRead = 0;
      rv = SyncRead(aStream, aBuffer + *aRead, aBufferSize - *aRead, &byteRead);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      *aRead += byteRead;
    }

    return NS_OK;
  }

  // We need to proceed async.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aStream);
  if (!asyncStream) {
    return rv;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  AutoSyncLoopHolder syncLoop(workerPrivate, Terminating);

  nsCOMPtr<nsIEventTarget> syncLoopTarget = syncLoop.GetEventTarget();
  if (!syncLoopTarget) {
    // SyncLoop creation can fail if the worker is shutting down.
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  RefPtr<ReadCallback> callback =
    new ReadCallback(workerPrivate, syncLoopTarget);

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);

  rv = asyncStream->AsyncWait(callback, 0, aBufferSize, target);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!syncLoop.Run()) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Now, we can try to read again.
  return SyncRead(aStream, aBuffer, aBufferSize, aRead);
}

nsresult
FileReaderSync::ConvertAsyncToSyncStream(uint64_t aStreamSize,
                                         already_AddRefed<nsIInputStream> aAsyncStream,
                                         nsIInputStream** aSyncStream)
{
  nsCOMPtr<nsIInputStream> asyncInputStream = std::move(aAsyncStream);

  // If the stream is not async, we just need it to be bufferable.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(asyncInputStream);
  if (!asyncStream) {
    return NS_NewBufferedInputStream(aSyncStream, asyncInputStream.forget(), 4096);
  }

  nsAutoCString buffer;
  if (!buffer.SetLength(aStreamSize, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t read;
  nsresult rv =
    SyncRead(asyncInputStream, buffer.BeginWriting(), aStreamSize, &read);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (read != aStreamSize) {
    return NS_ERROR_FAILURE;
  }

  rv = NS_NewCStringInputStream(aSyncStream, std::move(buffer));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
