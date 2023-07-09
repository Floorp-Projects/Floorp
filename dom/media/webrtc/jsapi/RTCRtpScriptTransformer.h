/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORMER_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORMER_H_

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/Maybe.h"
#include "js/RootingAPI.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include <memory>
#include "nsTHashSet.h"
#include "nsCycleCollectionParticipant.h"

class nsPIDOMWindowInner;

namespace webrtc {
class TransformableFrameInterface;
}

namespace mozilla {
class FrameTransformerProxy;

namespace dom {
class Worker;
class WorkerPrivate;

// Dirt simple source for ReadableStream that accepts nsISupports
// Might be suitable to move someplace else, with some polish.
class nsISupportsStreamSource final : public UnderlyingSourceAlgorithmsWrapper {
 public:
  nsISupportsStreamSource();
  nsISupportsStreamSource(const nsISupportsStreamSource&) = delete;
  nsISupportsStreamSource(nsISupportsStreamSource&&) = delete;
  nsISupportsStreamSource& operator=(const nsISupportsStreamSource&) = delete;
  nsISupportsStreamSource& operator=(nsISupportsStreamSource&&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsISupportsStreamSource,
                                           UnderlyingSourceAlgorithmsWrapper)

  void Init(ReadableStream* aStream);

  void Enqueue(nsISupports* aThing);

  // From UnderlyingSourceAlgorithmsWrapper
  already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

  void EnqueueOneThingFromQueue(JSContext* aCx);

 private:
  virtual ~nsISupportsStreamSource();

  // Calls ReadableStream::EnqueueNative, which is MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void EnqueueToStream(JSContext* aCx,
                                                   nsISupports* aThing);

  RefPtr<ReadableStream> mStream;
  RefPtr<Promise> mThingQueuedPromise;
  // mozilla::Queue is not cycle-collector friendly :(
  nsCOMArray<nsISupports> mQueue;
};

class RTCRtpScriptTransformer;

class WritableStreamRTCFrameSink final
    : public UnderlyingSinkAlgorithmsWrapper {
 public:
  explicit WritableStreamRTCFrameSink(RTCRtpScriptTransformer* aTransformer);
  WritableStreamRTCFrameSink(const WritableStreamRTCFrameSink&) = delete;
  WritableStreamRTCFrameSink(WritableStreamRTCFrameSink&&) = delete;
  WritableStreamRTCFrameSink& operator=(const WritableStreamRTCFrameSink&) =
      delete;
  WritableStreamRTCFrameSink& operator=(WritableStreamRTCFrameSink&&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WritableStreamRTCFrameSink,
                                           UnderlyingSinkAlgorithmsWrapper)

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController,
      ErrorResult& aError) override;

 private:
  virtual ~WritableStreamRTCFrameSink();
  RefPtr<RTCRtpScriptTransformer> mTransformer;
};

class RTCEncodedFrameBase;

// Here's the basic flow. All of this happens on the worker thread.
// 0. We register with a FrameTransformerProxy.
// 1. That FrameTransformerProxy dispatches webrtc::TransformableFrameInterface
//    to us (from either the encoder/depacketizer thread), via our
//    TransformFrame method.
// 2. We wrap these frames in RTCEncodedAudio/VideoFrame, and feed them to
//    mReadableSource, which queues them.
// 3. mReadableSource.PullCallbackImpl consumes that queue, and feeds the
//    frames to mReadable.
// 4. JS worker code consumes from mReadable, does whatever transformation it
//    wants, then writes the frames to mWritable.
// 5. mWritableSink.WriteCallback passes those frames to us.
// 6. We unwrap the webrtc::TransformableFrameInterface from these frames.
// 7. We pass these unwrapped frames back to the FrameTransformerProxy.
//    (FrameTransformerProxy handles any dispatching/synchronization necessary)
// 8. Eventually, that FrameTransformerProxy calls NotifyReleased (possibly at
//    our prompting).
class RTCRtpScriptTransformer final : public nsISupports,
                                      public nsWrapperCache {
 public:
  explicit RTCRtpScriptTransformer(nsIGlobalObject* aGlobal);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult Init(JSContext* aCx,
                                            JS::Handle<JS::Value> aOptions,
                                            WorkerPrivate* aWorkerPrivate,
                                            FrameTransformerProxy* aProxy);

  void NotifyReleased();

  void TransformFrame(
      std::unique_ptr<webrtc::TransformableFrameInterface> aFrame);
  already_AddRefed<Promise> OnTransformedFrame(RTCEncodedFrameBase* aFrame,
                                               ErrorResult& aError);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(RTCRtpScriptTransformer)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  // WebIDL Interface
  already_AddRefed<mozilla::dom::ReadableStream> Readable() const {
    return do_AddRef(mReadable);
  }
  already_AddRefed<mozilla::dom::WritableStream> Writable() const {
    return do_AddRef(mWritable);
  }

  void GetOptions(JSContext* aCx, JS::MutableHandle<JS::Value> aVal,
                  ErrorResult& aError);

  already_AddRefed<Promise> GenerateKeyFrame(const Optional<nsAString>& aRid);
  void GenerateKeyFrameError(const Maybe<std::string>& aRid,
                             const CopyableErrorResult& aResult);
  already_AddRefed<Promise> SendKeyFrameRequest();
  void KeyFrameRequestDone(bool aSuccess);

  // Public to ease implementation of cycle collection functions
  using GenerateKeyFramePromises =
      nsTHashMap<nsCStringHashKey, nsTArray<RefPtr<Promise>>>;

 private:
  virtual ~RTCRtpScriptTransformer();
  void RejectPendingPromises();
  // empty string means no rid
  void ResolveGenerateKeyFramePromises(const std::string& aRid,
                                       uint64_t aTimestamp);

  nsCOMPtr<nsIGlobalObject> mGlobal;

  RefPtr<FrameTransformerProxy> mProxy;
  RefPtr<nsISupportsStreamSource> mReadableSource;
  RefPtr<ReadableStream> mReadable;
  RefPtr<WritableStream> mWritable;
  RefPtr<WritableStreamRTCFrameSink> mWritableSink;

  JS::Heap<JS::Value> mOptions;
  uint64_t mLastEnqueuedFrameCounter = 0;
  uint64_t mLastReceivedFrameCounter = 0;
  nsTArray<RefPtr<Promise>> mKeyFrameRequestPromises;
  // Contains the promise returned for each call to GenerateKeyFrame(rid), in
  // the order in which it was called, keyed by the rid (empty string if not
  // passed). If there is already a promise in here for a given rid, we do not
  // ask the FrameTransformerProxy again, and just bulk resolve/reject.
  GenerateKeyFramePromises mGenerateKeyFramePromises;
  Maybe<bool> mVideo;
  RefPtr<StrongWorkerRef> mWorkerRef;
};

}  // namespace dom
}  // namespace mozilla

#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORMER_H_
