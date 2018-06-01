/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStreamReader.h"
#include "InternalResponse.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/TaskCategory.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(FetchStreamReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FetchStreamReader)

NS_IMPL_CYCLE_COLLECTION_CLASS(FetchStreamReader)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FetchStreamReader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FetchStreamReader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(FetchStreamReader)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReader)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchStreamReader)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIOutputStreamCallback)
NS_INTERFACE_MAP_END

/* static */ nsresult
FetchStreamReader::Create(JSContext* aCx, nsIGlobalObject* aGlobal,
                          FetchStreamReader** aStreamReader,
                          nsIInputStream** aInputStream)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aStreamReader);
  MOZ_ASSERT(aInputStream);

  RefPtr<FetchStreamReader> streamReader = new FetchStreamReader(aGlobal);

  nsCOMPtr<nsIAsyncInputStream> pipeIn;

  nsresult rv = NS_NewPipe2(getter_AddRefs(pipeIn),
                            getter_AddRefs(streamReader->mPipeOut),
                            true, true, 0, 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);

    RefPtr<WeakWorkerRef> workerRef =
      WeakWorkerRef::Create(workerPrivate, [streamReader]() {
        // The WorkerPrivate does have a context available, and we could pass
        // it here to trigger cancellation of the reader, but the author of
        // this comment chickened out.
        streamReader->CloseAndRelease(nullptr, NS_ERROR_DOM_INVALID_STATE_ERR);
      });

    if (NS_WARN_IF(!workerRef)) {
      streamReader->mPipeOut->CloseWithStatus(NS_ERROR_DOM_INVALID_STATE_ERR);
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    // These 2 objects create a ref-cycle here that is broken when the stream is
    // closed or the worker shutsdown.
    streamReader->mWorkerRef = workerRef.forget();
  }

  pipeIn.forget(aInputStream);
  streamReader.forget(aStreamReader);
  return NS_OK;
}

FetchStreamReader::FetchStreamReader(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mOwningEventTarget(mGlobal->EventTargetFor(TaskCategory::Other))
  , mBufferRemaining(0)
  , mBufferOffset(0)
  , mStreamClosed(false)
{
  MOZ_ASSERT(aGlobal);
}

FetchStreamReader::~FetchStreamReader()
{
  CloseAndRelease(nullptr, NS_BASE_STREAM_CLOSED);
}

// If a context is provided, an attempt will be made to cancel the reader.  The
// only situation where we don't expect to have a context is when closure is
// being triggered from the destructor or the WorkerRef is notifying.  If
// we're at the destructor, it's far too late to cancel anything.  And if the
// WorkerRef is being notified, the global is going away, so there's also
// no need to do further JS work.
void
FetchStreamReader::CloseAndRelease(JSContext* aCx, nsresult aStatus)
{
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);

  if (mStreamClosed) {
    // Already closed.
    return;
  }

  RefPtr<FetchStreamReader> kungFuDeathGrip = this;

  if (aCx) {
    MOZ_ASSERT(mReader);

    RefPtr<DOMException> error = DOMException::Create(aStatus);

    JS::Rooted<JS::Value> errorValue(aCx);
    if (ToJSValue(aCx, error, &errorValue)) {
      JS::Rooted<JSObject*> reader(aCx, mReader);
      // It's currently safe to cancel an already closed reader because, per the
      // comments in ReadableStream::cancel() conveying the spec, step 2 of
      // 3.4.3 that specified ReadableStreamCancel is: If stream.[[state]] is
      // "closed", return a new promise resolved with undefined.
      JS::ReadableStreamReaderCancel(aCx, reader, errorValue);
    }
  }

  mStreamClosed = true;

  mGlobal = nullptr;

  mPipeOut->CloseWithStatus(aStatus);
  mPipeOut = nullptr;

  mWorkerRef = nullptr;

  mReader = nullptr;
  mBuffer = nullptr;
}

void
FetchStreamReader::StartConsuming(JSContext* aCx,
                                  JS::HandleObject aStream,
                                  JS::MutableHandle<JSObject*> aReader,
                                  ErrorResult& aRv)
{
  MOZ_DIAGNOSTIC_ASSERT(!mReader);
  MOZ_DIAGNOSTIC_ASSERT(aStream);

  JS::Rooted<JSObject*> reader(aCx,
                               JS::ReadableStreamGetReader(aCx, aStream,
                                                           JS::ReadableStreamReaderMode::Default));
  if (!reader) {
    aRv.StealExceptionFromJSContext(aCx);
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mReader = reader;
  aReader.set(reader);

  aRv = mPipeOut->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

// nsIOutputStreamCallback interface

NS_IMETHODIMP
FetchStreamReader::OnOutputStreamReady(nsIAsyncOutputStream* aStream)
{
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);
  MOZ_ASSERT(aStream == mPipeOut);
  MOZ_ASSERT(mReader);

  if (mStreamClosed) {
    return NS_OK;
  }

  if (mBuffer) {
    return WriteBuffer();
  }

  // TODO: We need to verify this is the correct global per the spec.
  //       See bug 1385890.
  AutoEntryScript aes(mGlobal, "ReadableStreamReader.read", !mWorkerRef);

  JS::Rooted<JSObject*> reader(aes.cx(), mReader);
  JS::Rooted<JSObject*> promise(aes.cx(),
                                JS::ReadableStreamDefaultReaderRead(aes.cx(),
                                                                    reader));
  if (NS_WARN_IF(!promise)) {
    // Let's close the stream.
    CloseAndRelease(aes.cx(), NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_ERROR_FAILURE;
  }

  RefPtr<Promise> domPromise = Promise::CreateFromExisting(mGlobal, promise);
  if (NS_WARN_IF(!domPromise)) {
    // Let's close the stream.
    CloseAndRelease(aes.cx(), NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_ERROR_FAILURE;
  }

  // Let's wait.
  domPromise->AppendNativeHandler(this);
  return NS_OK;
}

void
FetchStreamReader::ResolvedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue)
{
  if (mStreamClosed) {
    return;
  }

  // This promise should be resolved with { done: boolean, value: something },
  // "value" is interesting only if done is false.

  // We don't want to play with JS api, let's WebIDL bindings doing it for us.
  // FetchReadableStreamReadDataDone is a dictionary with just a boolean, if the
  // parsing succeeded, we can proceed with the parsing of the "value", which it
  // must be a Uint8Array.
  FetchReadableStreamReadDataDone valueDone;
  if (!valueDone.Init(aCx, aValue)) {
    JS_ClearPendingException(aCx);
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (valueDone.mDone) {
    // Stream is completed.
    CloseAndRelease(aCx, NS_BASE_STREAM_CLOSED);
    return;
  }

  UniquePtr<FetchReadableStreamReadDataArray> value(
    new FetchReadableStreamReadDataArray);
  if (!value->Init(aCx, aValue) || !value->mValue.WasPassed()) {
    JS_ClearPendingException(aCx);
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  Uint8Array& array = value->mValue.Value();
  array.ComputeLengthAndData();
  uint32_t len = array.Length();

  if (len == 0) {
    // If there is nothing to read, let's do another reading.
    OnOutputStreamReady(mPipeOut);
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mBuffer);
  mBuffer = std::move(value);

  mBufferOffset = 0;
  mBufferRemaining = len;

  nsresult rv = WriteBuffer();
  if (NS_FAILED(rv)) {
    // DOMException only understands errors from domerr.msg, so we normalize to
    // identifying an abort if the write fails.
    CloseAndRelease(aCx, NS_ERROR_DOM_ABORT_ERR);
  }
}

nsresult
FetchStreamReader::WriteBuffer()
{
  MOZ_ASSERT(mBuffer);
  MOZ_ASSERT(mBuffer->mValue.WasPassed());

  Uint8Array& array = mBuffer->mValue.Value();
  char* data = reinterpret_cast<char*>(array.Data());

  while (1) {
    uint32_t written = 0;
    nsresult rv =
      mPipeOut->Write(data + mBufferOffset, mBufferRemaining, &written);

    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      break;
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(written <= mBufferRemaining);
    mBufferRemaining -= written;
    mBufferOffset += written;

    if (mBufferRemaining == 0) {
      mBuffer = nullptr;
      break;
    }
  }

  nsresult rv = mPipeOut->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
FetchStreamReader::RejectedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue)
{
  ReportErrorToConsole(aCx, aValue);
  CloseAndRelease(aCx, NS_ERROR_FAILURE);
}

void
FetchStreamReader::ReportErrorToConsole(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue)
{
  nsCString sourceSpec;
  uint32_t line = 0;
  uint32_t column = 0;
  nsString valueString;

  nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line,
                                     &column, valueString);

  nsTArray<nsString> params;
  params.AppendElement(valueString);

  RefPtr<ConsoleReportCollector> reporter = new ConsoleReportCollector();
  reporter->AddConsoleReport(nsIScriptError::errorFlag,
                             NS_LITERAL_CSTRING("ReadableStreamReader.read"),
                             nsContentUtils::eDOM_PROPERTIES,
                             sourceSpec, line, column,
                             NS_LITERAL_CSTRING("ReadableStreamReadingFailed"),
                             params);

  uint64_t innerWindowId = 0;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mGlobal);
    if (window) {
      innerWindowId = window->WindowID();
    }
    reporter->FlushReportsToConsole(innerWindowId);
    return;
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (workerPrivate) {
    innerWindowId = workerPrivate->WindowID();
  }

  RefPtr<Runnable> r = NS_NewRunnableFunction(
    "FetchStreamReader::ReportErrorToConsole",
    [reporter, innerWindowId] () {
      reporter->FlushReportsToConsole(innerWindowId);
    });

  workerPrivate->DispatchToMainThread(r.forget());
}

} // dom namespace
} // mozilla namespace
