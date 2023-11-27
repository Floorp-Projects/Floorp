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

NS_IMPL_CYCLE_COLLECTING_ADDREF(FetchStreamReader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FetchStreamReader)

NS_IMPL_CYCLE_COLLECTION(FetchStreamReader, mGlobal, mReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchStreamReader)
  NS_INTERFACE_MAP_ENTRY(nsIOutputStreamCallback)
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

  NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(streamReader->mPipeOut),
              true, true, 0, 0);

  pipeIn.forget(aInputStream);
  streamReader.forget(aStreamReader);
  return NS_OK;
}

nsresult FetchStreamReader::MaybeGrabStrongWorkerRef(JSContext* aCx) {
  if (NS_IsMainThread()) {
    return NS_OK;
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(workerPrivate);

  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      workerPrivate, "FetchStreamReader", [streamReader = RefPtr(this)]() {
        MOZ_ASSERT(streamReader);

        // mAsyncWaitWorkerRef may keep the (same) StrongWorkerRef alive even
        // when mWorkerRef has already been nulled out by a previous call to
        // CloseAndRelease, we can just safely ignore this callback then
        // (as would the CloseAndRelease do on a second call).
        if (streamReader->mWorkerRef) {
          streamReader->CloseAndRelease(
              streamReader->mWorkerRef->Private()->GetJSContext(),
              NS_ERROR_DOM_INVALID_STATE_ERR);
        } else {
          MOZ_DIAGNOSTIC_ASSERT(streamReader->mAsyncWaitWorkerRef);
        }
      });

  if (NS_WARN_IF(!workerRef)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // These 2 objects create a ref-cycle here that is broken when the stream is
  // closed or the worker shutsdown.
  mWorkerRef = std::move(workerRef);

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

  if (mPipeOut) {
    mPipeOut->CloseWithStatus(aStatus);
  }
  mPipeOut = nullptr;

  mWorkerRef = nullptr;

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

  aRv = MaybeGrabStrongWorkerRef(aCx);
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

  mAsyncWaitWorkerRef = mWorkerRef;
  aRv = mPipeOut->AsyncWait(this, 0, 0, mOwningEventTarget);
  if (NS_WARN_IF(aRv.Failed())) {
    mAsyncWaitWorkerRef = nullptr;
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
NS_IMETHODIMP
FetchStreamReader::OnOutputStreamReady(nsIAsyncOutputStream* aStream) {
  NS_ASSERT_OWNINGTHREAD(FetchStreamReader);
  if (mStreamClosed) {
    mAsyncWaitWorkerRef = nullptr;
    return NS_OK;
  }

  AutoEntryScript aes(mGlobal, "ReadableStreamReader.read", !mWorkerRef);
  if (!Process(aes.cx())) {
    // We're done processing data, and haven't queued up a new AsyncWait - we
    // can clear our mAsyncWaitWorkerRef.
    mAsyncWaitWorkerRef = nullptr;
  }
  return NS_OK;
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
  if (NS_WARN_IF(NS_FAILED(mPipeOut->StreamStatus()))) {
    CloseAndRelease(aCx, NS_ERROR_DOM_ABORT_ERR);
    return false;
  }

  // We're waiting on new data - set up a WAIT_CLOSURE_ONLY callback so we
  // notice if the reader closes.
  nsresult rv = mPipeOut->AsyncWait(
      this, nsIAsyncOutputStream::WAIT_CLOSURE_ONLY, 0, mOwningEventTarget);
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
      mBuffer.Clear();
      break;
    }
  }

  nsresult rv = mPipeOut->AsyncWait(this, 0, 0, mOwningEventTarget);
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
