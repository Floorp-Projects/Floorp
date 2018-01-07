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

using namespace workers;

namespace {

class FetchStreamReaderWorkerHolder final : public WorkerHolder
{
public:
  explicit FetchStreamReaderWorkerHolder(FetchStreamReader* aReader)
    : WorkerHolder("FetchStreamReaderWorkerHolder",
                   WorkerHolder::Behavior::AllowIdleShutdownStart)
    , mReader(aReader)
    , mWasNotified(false)
  {}

  bool Notify(Status aStatus) override
  {
    if (!mWasNotified) {
      mWasNotified = true;
      mReader->CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
    }

    return true;
  }

private:
  RefPtr<FetchStreamReader> mReader;
  bool mWasNotified;
};

} // anonymous

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

    // We need to know when the worker goes away.
    UniquePtr<FetchStreamReaderWorkerHolder> holder(
      new FetchStreamReaderWorkerHolder(streamReader));
    if (NS_WARN_IF(!holder->HoldWorker(workerPrivate, Closing))) {
      streamReader->mPipeOut->CloseWithStatus(NS_ERROR_DOM_INVALID_STATE_ERR);
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    // These 2 objects create a ref-cycle here that is broken when the stream is
    // closed or the worker shutsdown.
    streamReader->mWorkerHolder = Move(holder);
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
  CloseAndRelease(NS_BASE_STREAM_CLOSED);
}

void
FetchStreamReader::CloseAndRelease(nsresult aStatus)
{
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);

  if (mStreamClosed) {
    // Already closed.
    return;
  }

  RefPtr<FetchStreamReader> kungFuDeathGrip = this;

  mStreamClosed = true;

  mGlobal = nullptr;

  mPipeOut->CloseWithStatus(aStatus);
  mPipeOut = nullptr;

  mWorkerHolder = nullptr;

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
    CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
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
  AutoEntryScript aes(mGlobal, "ReadableStreamReader.read", !mWorkerHolder);

  JS::Rooted<JSObject*> reader(aes.cx(), mReader);
  JS::Rooted<JSObject*> promise(aes.cx(),
                                JS::ReadableStreamDefaultReaderRead(aes.cx(),
                                                                    reader));
  if (NS_WARN_IF(!promise)) {
    // Let's close the stream.
    CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
    return NS_ERROR_FAILURE;
  }

  RefPtr<Promise> domPromise = Promise::CreateFromExisting(mGlobal, promise);
  if (NS_WARN_IF(!domPromise)) {
    // Let's close the stream.
    CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
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
    CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (valueDone.mDone) {
    // Stream is completed.
    CloseAndRelease(NS_BASE_STREAM_CLOSED);
    return;
  }

  UniquePtr<FetchReadableStreamReadDataArray> value(
    new FetchReadableStreamReadDataArray);
  if (!value->Init(aCx, aValue) || !value->mValue.WasPassed()) {
    JS_ClearPendingException(aCx);
    CloseAndRelease(NS_ERROR_DOM_INVALID_STATE_ERR);
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
  mBuffer = Move(value);

  mBufferOffset = 0;
  mBufferRemaining = len;

  WriteBuffer();
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
      CloseAndRelease(rv);
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
    CloseAndRelease(rv);
    return rv;
  }

  return NS_OK;
}

void
FetchStreamReader::RejectedCallback(JSContext* aCx,
                                    JS::Handle<JS::Value> aValue)
{
  ReportErrorToConsole(aCx, aValue);
  CloseAndRelease(NS_ERROR_FAILURE);
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
