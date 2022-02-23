/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamPipeTo_h
#define mozilla_dom_ReadableStreamPipeTo_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AbortFollower.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

struct PipeToReadRequest;
class WriteFinishedPromiseHandler;
class ShutdownActionFinishedPromiseHandler;

class ReadableStreamDefaultReader;
class WritableStreamDefaultWriter;

// https://streams.spec.whatwg.org/#readable-stream-pipe-to (Steps 14-15.)
//
// This class implements everything that is required to read all chunks from
// the reader (source) and write them to writer (destination), while
// following the constraints given in the spec using our implementation-defined
// behavior.
//
// The cycle-collected references look roughly like this:
// clang-format off
//
// Closed promise <-- ReadableStreamDefaultReader <--> ReadableStream
//         |                  ^              |
//         |(PromiseHandler)  |(mReader)     |(ReadRequest)
//         |                  |              |
//         |-------------> PipeToPump <-------
//                         ^  |   |
//         |---------------|  |   |
//         |                  |   |-------(mLastWrite) -------->
//         |(PromiseHandler)  |   |< ---- (PromiseHandler) ---- Promise
//         |                  |                                   ^
//         |                  |(mWriter)                          |(mWriteRequests)
//         |                  v                                   |
// Closed promise <-- WritableStreamDefaultWriter <--------> WritableStream
//
// clang-format on
class PipeToPump final : public AbortFollower {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PipeToPump)

  friend struct PipeToReadRequest;
  friend class WriteFinishedPromiseHandler;
  friend class ShutdownActionFinishedPromiseHandler;

  PipeToPump(Promise* aPromise, ReadableStreamDefaultReader* aReader,
             WritableStreamDefaultWriter* aWriter, bool aPreventClose,
             bool aPreventAbort, bool aPreventCancel)
      : mPromise(aPromise),
        mReader(aReader),
        mWriter(aWriter),
        mPreventClose(aPreventClose),
        mPreventAbort(aPreventAbort),
        mPreventCancel(aPreventCancel) {}

  MOZ_CAN_RUN_SCRIPT void Start(JSContext* aCx, AbortSignal* aSignal);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void RunAbortAlgorithm() override;

 private:
  ~PipeToPump() override = default;

  MOZ_CAN_RUN_SCRIPT void PerformAbortAlgorithm(JSContext* aCx,
                                                AbortSignalImpl* aSignal);

  MOZ_CAN_RUN_SCRIPT bool SourceOrDestErroredOrClosed(JSContext* aCx);

  using ShutdownAction = already_AddRefed<Promise> (*)(
      JSContext*, PipeToPump*, JS::Handle<mozilla::Maybe<JS::Value>>,
      ErrorResult&);

  MOZ_CAN_RUN_SCRIPT void ShutdownWithAction(
      JSContext* aCx, ShutdownAction aAction,
      JS::Handle<mozilla::Maybe<JS::Value>> aError);
  MOZ_CAN_RUN_SCRIPT void ShutdownWithActionAfterFinishedWrite(
      JSContext* aCx, ShutdownAction aAction,
      JS::Handle<mozilla::Maybe<JS::Value>> aError);

  MOZ_CAN_RUN_SCRIPT void Shutdown(
      JSContext* aCx, JS::Handle<mozilla::Maybe<JS::Value>> aError);

  void Finalize(JSContext* aCx, JS::Handle<mozilla::Maybe<JS::Value>> aError);

  MOZ_CAN_RUN_SCRIPT void OnReadFulfilled(JSContext* aCx,
                                          JS::Handle<JS::Value> aChunk,
                                          ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void OnWriterReady(JSContext* aCx, JS::Handle<JS::Value>);
  MOZ_CAN_RUN_SCRIPT void Read(JSContext* aCx);

  MOZ_CAN_RUN_SCRIPT void OnSourceClosed(JSContext* aCx, JS::Handle<JS::Value>);
  MOZ_CAN_RUN_SCRIPT void OnSourceErrored(JSContext* aCx,
                                          JS::Handle<JS::Value> aError);

  MOZ_CAN_RUN_SCRIPT void OnDestClosed(JSContext* aCx, JS::Handle<JS::Value>);
  MOZ_CAN_RUN_SCRIPT void OnDestErrored(JSContext* aCx,
                                        JS::Handle<JS::Value> aError);

  RefPtr<Promise> mPromise;
  RefPtr<ReadableStreamDefaultReader> mReader;
  RefPtr<WritableStreamDefaultWriter> mWriter;
  RefPtr<Promise> mLastWritePromise;
  const bool mPreventClose;
  const bool mPreventAbort;
  const bool mPreventCancel;
  bool mShuttingDown = false;
#ifdef DEBUG
  bool mReadChunk = false;
#endif
};

MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> ReadableStreamPipeTo(
    ReadableStream* aSource, WritableStream* aDest, bool aPreventClose,
    bool aPreventAbort, bool aPreventCancel, AbortSignal* aSignal,
    ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // mozilla_dom_ReadableStreamPipeTo_h
