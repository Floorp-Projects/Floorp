/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStreamReader.h"
#include "InternalResponse.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/StaticAnalysisFunctions.h"
#include "mozilla/dom/AutoEntryScript.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamDefaultReader.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsIAsyncInputStream.h"
#include "nsIPipe.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"
#include "jsapi.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(OutputStreamHolder, nsIOutputStreamCallback)

OutputStreamHolder::OutputStreamHolder(FetchStreamReader* aReader,
                                       nsIAsyncOutputStream* aOutput)
    : mReader(aReader), mOutput(aOutput) {}

nsresult OutputStreamHolder::Init(JSContext* aCx) {
  if (NS_IsMainThread()) {
    return NS_OK;
  }

  // We're in a worker
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  workerPrivate->AssertIsOnWorkerThread();

  // Note, this will create a ref-cycle between the holder and the stream.
  // The cycle is broken when the stream is closed or the worker begins
  // shutting down.
  mWorkerRef =
      StrongWorkerRef::Create(workerPrivate, "OutputStreamHolder",
                              [self = RefPtr{this}]() { self->Shutdown(); });
  if (NS_WARN_IF(!mWorkerRef)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

OutputStreamHolder::~OutputStreamHolder() = default;

void OutputStreamHolder::Shutdown() {
  if (mOutput) {
    mOutput->Close();
  }
  // If we have an AsyncWait running, we'll get a callback and clear
  // the mAsyncWaitWorkerRef
  mWorkerRef = nullptr;
}

nsresult OutputStreamHolder::AsyncWait(uint32_t aFlags,
                                       uint32_t aRequestedCount,
                                       nsIEventTarget* aEventTarget) {
  mAsyncWaitWorkerRef = mWorkerRef;
  // Grab the strong reference for the reader but only when we are waiting for
  // the output stream, because it means we still have things to write.
  // (WAIT_CLOSURE_ONLY happens when waiting for ReadableStream to respond, at
  // which point the pull callback should get an indirect strong reference via
  // the controller argument.)
  mAsyncWaitReader =
      aFlags == nsIAsyncOutputStream::WAIT_CLOSURE_ONLY ? nullptr : mReader;
  nsresult rv = mOutput->AsyncWait(this, aFlags, aRequestedCount, aEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mAsyncWaitWorkerRef = nullptr;
    mAsyncWaitReader = nullptr;
  }
  return rv;
}

NS_IMETHODIMP OutputStreamHolder::OnOutputStreamReady(
    nsIAsyncOutputStream* aStream) {
  // We may get called back after ::Shutdown()
  if (!mReader) {
    mAsyncWaitWorkerRef = nullptr;
    MOZ_ASSERT(!mAsyncWaitReader);
    return NS_OK;
  }

  // mAsyncWaitReader may be reset during OnOutputStreamReady, make sure to let
  // it live during the call
  RefPtr<FetchStreamReader> reader = mReader.get();
  if (!reader->OnOutputStreamReady()) {
    mAsyncWaitWorkerRef = nullptr;
    mAsyncWaitReader = nullptr;
    return NS_OK;
  }
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(FetchStreamReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FetchStreamReader)

NS_IMPL_CYCLE_COLLECTION_WEAK_PTR(FetchStreamReader, mGlobal, mReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchStreamReader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
nsresult FetchStreamReader::Create(JSContext* aCx, nsIGlobalObject* aGlobal,
                                   FetchStreamReader** aStreamReader,
                                   nsIInputStream** aInputStream) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aStreamReader);
  MOZ_ASSERT(aInputStream);

  RefPtr<FetchStreamReader> streamReader = new FetchStreamReader(aGlobal);

  nsCOMPtr<nsIAsyncInputStream> pipeIn;
  nsCOMPtr<nsIAsyncOutputStream> pipeOut;

  NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut), true, true, 0,
              0);

  streamReader->mOutput = new OutputStreamHolder(streamReader, pipeOut);

  pipeIn.forget(aInputStream);
  streamReader.forget(aStreamReader);
  return NS_OK;
}

FetchStreamReader::FetchStreamReader(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mOwningEventTarget(mGlobal->SerialEventTarget()) {
  MOZ_ASSERT(aGlobal);
}

FetchStreamReader::~FetchStreamReader() {
  CloseAndRelease(nullptr, NS_BASE_STREAM_CLOSED);
}

// If a context is provided, an attempt will be made to cancel the reader.  The
// only situation where we don't expect to have a context is when closure is
// being triggered from the destructor or the WorkerRef is notifying.  If
// we're at the destructor, it's far too late to cancel anything.  And if the
// WorkerRef is being notified, the global is going away, so there's also
// no need to do further JS work.
void FetchStreamReader::CloseAndRelease(JSContext* aCx, nsresult aStatus) {
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);

  if (mStreamClosed) {
    // Already closed.
    return;
  }

  RefPtr<FetchStreamReader> kungFuDeathGrip = this;
  if (aCx && mReader) {
    ErrorResult rv;
    if (aStatus == NS_ERROR_DOM_WRONG_TYPE_ERR) {
      rv.ThrowTypeError<MSG_FETCH_BODY_WRONG_TYPE>();
    } else {
      rv = aStatus;
    }
    JS::Rooted<JS::Value> errorValue(aCx);
    if (ToJSValue(aCx, std::move(rv), &errorValue)) {
      IgnoredErrorResult ignoredError;
      // It's currently safe to cancel an already closed reader because, per the
      // comments in ReadableStream::cancel() conveying the spec, step 2 of
      // 3.4.3 that specified ReadableStreamCancel is: If stream.[[state]] is
      // "closed", return a new promise resolved with undefined.
      RefPtr<Promise> cancelResultPromise =
          MOZ_KnownLive(mReader)->Cancel(aCx, errorValue, ignoredError);
      NS_WARNING_ASSERTION(!ignoredError.Failed(),
                           "Failed to cancel stream during close and release");
      if (cancelResultPromise) {
        bool setHandled = cancelResultPromise->SetAnyPromiseIsHandled();
        NS_WARNING_ASSERTION(setHandled,
                             "Failed to mark cancel promise as handled.");
        (void)setHandled;
      }
    }

    // We don't want to propagate exceptions during the cleanup.
    JS_ClearPendingException(aCx);
  }

  mStreamClosed = true;

  mGlobal = nullptr;

  if (mOutput) {
    mOutput->CloseWithStatus(aStatus);
    mOutput->Shutdown();
    mOutput = nullptr;
  }

  mReader = nullptr;
  mBuffer.Clear();
}

// https://fetch.spec.whatwg.org/#body-incrementally-read
void FetchStreamReader::StartConsuming(JSContext* aCx, ReadableStream* aStream,
                                       ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(!mReader);
  MOZ_DIAGNOSTIC_ASSERT(aStream);
  MOZ_ASSERT(!aStream->MaybeGetInputStreamIfUnread(),
             "FetchStreamReader is for JS streams but we got a stream based on "
             "nsIInputStream here. Extract nsIInputStream and read it instead "
             "to reduce overhead.");

  aRv = mOutput->Init(aCx);
  if (aRv.Failed()) {
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Step 2: Let reader be the result of getting a reader for body’s stream.
  RefPtr<ReadableStreamDefaultReader> reader = aStream->GetReader(aRv);
  if (aRv.Failed()) {
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mReader = reader;

  aRv = mOutput->AsyncWait(0, 0, mOwningEventTarget);
  if (NS_WARN_IF(aRv.Failed())) {
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

struct FetchReadRequest : public ReadRequest {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FetchReadRequest, ReadRequest)

  explicit FetchReadRequest(FetchStreamReader* aReader)
      : mFetchStreamReader(aReader) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override {
    mFetchStreamReader->ChunkSteps(aCx, aChunk, aRv);
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void CloseSteps(JSContext* aCx, ErrorResult& aRv) override {
    mFetchStreamReader->CloseSteps(aCx, aRv);
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                  ErrorResult& aRv) override {
    mFetchStreamReader->ErrorSteps(aCx, aError, aRv);
  }

 protected:
  virtual ~FetchReadRequest() = default;

  MOZ_KNOWN_LIVE RefPtr<FetchStreamReader> mFetchStreamReader;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(FetchReadRequest, ReadRequest,
                                   mFetchStreamReader)
NS_IMPL_ADDREF_INHERITED(FetchReadRequest, ReadRequest)
NS_IMPL_RELEASE_INHERITED(FetchReadRequest, ReadRequest)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchReadRequest)
NS_INTERFACE_MAP_END_INHERITING(ReadRequest)

// nsIOutputStreamCallback interface
MOZ_CAN_RUN_SCRIPT_BOUNDARY
bool FetchStreamReader::OnOutputStreamReady() {
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);
  if (mStreamClosed) {
    return false;
  }

  AutoEntryScript aes(mGlobal, "ReadableStreamReader.read");
  return Process(aes.cx());
}

bool FetchStreamReader::Process(JSContext* aCx) {
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);
  MOZ_ASSERT(mReader);

  if (!mBuffer.IsEmpty()) {
    nsresult rv = WriteBuffer();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      CloseAndRelease(aCx, NS_ERROR_DOM_ABORT_ERR);
      return false;
    }
    return true;
  }

  // Check if the output stream has already been closed. This lets us propagate
  // errors eagerly, and detect output stream closures even when we have no data
  // to write.
  if (NS_WARN_IF(NS_FAILED(mOutput->StreamStatus()))) {
    CloseAndRelease(aCx, NS_ERROR_DOM_ABORT_ERR);
    return false;
  }

  // We're waiting on new data - set up a WAIT_CLOSURE_ONLY callback so we
  // notice if the reader closes.
  nsresult rv = mOutput->AsyncWait(nsIAsyncOutputStream::WAIT_CLOSURE_ONLY, 0,
                                   mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  // If we already have an outstanding read request, don't start another one
  // concurrently.
  if (!mHasOutstandingReadRequest) {
    // https://fetch.spec.whatwg.org/#incrementally-read-loop
    // The below very loosely tries to implement the incrementally-read-loop
    // from the fetch spec.
    // Step 2: Read a chunk from reader given readRequest.
    RefPtr<ReadRequest> readRequest = new FetchReadRequest(this);
    RefPtr<ReadableStreamDefaultReader> reader = mReader;
    mHasOutstandingReadRequest = true;

    IgnoredErrorResult err;
    reader->ReadChunk(aCx, *readRequest, err);
    if (NS_WARN_IF(err.Failed())) {
      // Let's close the stream.
      mHasOutstandingReadRequest = false;
      CloseAndRelease(aCx, NS_ERROR_DOM_INVALID_STATE_ERR);
      // Don't return false, as we've already called `AsyncWait`.
    }
  }
  return true;
}

void FetchStreamReader::ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                   ErrorResult& aRv) {
  // This roughly implements the chunk steps from
  // https://fetch.spec.whatwg.org/#incrementally-read-loop.

  mHasOutstandingReadRequest = false;

  // Step 2. If chunk is not a Uint8Array object, then set continueAlgorithm to
  // this step: run processBodyError given a TypeError.
  RootedSpiderMonkeyInterface<Uint8Array> chunk(aCx);
  if (!aChunk.isObject() || !chunk.Init(&aChunk.toObject())) {
    CloseAndRelease(aCx, NS_ERROR_DOM_WRONG_TYPE_ERR);
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(mBuffer.IsEmpty());

  // Let's take a copy of the data.
  // FIXME: We could sometimes avoid this copy by trying to write `chunk`
  // directly into `mPipeOut` eagerly, and only filling `mBuffer` if there isn't
  // enough space in the pipe's buffer.
  if (!chunk.AppendDataTo(mBuffer)) {
    CloseAndRelease(aCx, NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  mBufferOffset = 0;
  mBufferRemaining = mBuffer.Length();

  nsresult rv = WriteBuffer();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CloseAndRelease(aCx, NS_ERROR_DOM_ABORT_ERR);
  }
}

void FetchStreamReader::CloseSteps(JSContext* aCx, ErrorResult& aRv) {
  mHasOutstandingReadRequest = false;
  CloseAndRelease(aCx, NS_BASE_STREAM_CLOSED);
}

void FetchStreamReader::ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> aError,
                                   ErrorResult& aRv) {
  mHasOutstandingReadRequest = false;
  ReportErrorToConsole(aCx, aError);
  CloseAndRelease(aCx, NS_ERROR_FAILURE);
}

nsresult FetchStreamReader::WriteBuffer() {
  MOZ_ASSERT(mBuffer.Length() == (mBufferOffset + mBufferRemaining));

  char* data = reinterpret_cast<char*>(mBuffer.Elements());

  while (mBufferRemaining > 0) {
    uint32_t written = 0;
    nsresult rv =
        mOutput->Write(data + mBufferOffset, mBufferRemaining, &written);

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
      mBuffer.Clear();
      break;
    }
  }

  nsresult rv = mOutput->AsyncWait(0, 0, mOwningEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void FetchStreamReader::ReportErrorToConsole(JSContext* aCx,
                                             JS::Handle<JS::Value> aValue) {
  nsCString sourceSpec;
  uint32_t line = 0;
  uint32_t column = 0;
  nsString valueString;

  nsContentUtils::ExtractErrorValues(aCx, aValue, sourceSpec, &line, &column,
                                     valueString);

  nsTArray<nsString> params;
  params.AppendElement(valueString);

  RefPtr<ConsoleReportCollector> reporter = new ConsoleReportCollector();
  reporter->AddConsoleReport(nsIScriptError::errorFlag,
                             "ReadableStreamReader.read"_ns,
                             nsContentUtils::eDOM_PROPERTIES, sourceSpec, line,
                             column, "ReadableStreamReadingFailed"_ns, params);

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
      "FetchStreamReader::ReportErrorToConsole", [reporter, innerWindowId]() {
        reporter->FlushReportsToConsole(innerWindowId);
      });

  workerPrivate->DispatchToMainThread(r.forget());
}

}  // namespace mozilla::dom
