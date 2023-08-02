/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCRtpScriptTransformer.h"

#include <stdint.h>

#include <utility>
#include <memory>
#include <string>

#include "api/frame_transformer_interface.h"

#include "nsString.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "ErrorList.h"
#include "nsDebug.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"
#include "nsIGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "nsLiteralString.h"
#include "nsContentUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Result.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Maybe.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/RTCRtpScriptTransformerBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PrototypeList.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/RTCEncodedAudioFrame.h"
#include "mozilla/dom/RTCEncodedVideoFrame.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/UnderlyingSinkCallbackHelpers.h"
#include "mozilla/dom/WritableStreamDefaultController.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "js/RootingAPI.h"
#include "js/Value.h"
#include "js/CallArgs.h"
#include "libwebrtcglue/FrameTransformerProxy.h"
#include "sdp/SdpAttribute.h"  // CheckRidValidity

namespace mozilla::dom {

LazyLogModule gScriptTransformerLog("RTCRtpScriptTransformer");

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsISupportsStreamSource,
                                   UnderlyingSourceAlgorithmsWrapper, mStream,
                                   mThingQueuedPromise, mQueue)
NS_IMPL_ADDREF_INHERITED(nsISupportsStreamSource,
                         UnderlyingSourceAlgorithmsWrapper)
NS_IMPL_RELEASE_INHERITED(nsISupportsStreamSource,
                          UnderlyingSourceAlgorithmsWrapper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsISupportsStreamSource)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsWrapper)

nsISupportsStreamSource::nsISupportsStreamSource() = default;

nsISupportsStreamSource::~nsISupportsStreamSource() = default;

void nsISupportsStreamSource::Init(ReadableStream* aStream) {
  mStream = aStream;
}

void nsISupportsStreamSource::Enqueue(nsISupports* aThing) {
  if (!mThingQueuedPromise) {
    mQueue.AppendElement(aThing);
    return;
  }

  // Maybe put a limit here? Or at least some sort of logging if this gets
  // unreasonably long?
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mStream->GetParentObject()))) {
    return;
  }

  EnqueueToStream(jsapi.cx(), aThing);
  mThingQueuedPromise->MaybeResolveWithUndefined();
  mThingQueuedPromise = nullptr;
}

already_AddRefed<Promise> nsISupportsStreamSource::PullCallbackImpl(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  if (!mQueue.IsEmpty()) {
    EnqueueOneThingFromQueue(aCx);
    return nullptr;
  }

  RefPtr<nsISupportsStreamSource> self(this);
  mThingQueuedPromise = Promise::CreateInfallible(mStream->GetParentObject());
  return do_AddRef(mThingQueuedPromise);
}

void nsISupportsStreamSource::EnqueueToStream(JSContext* aCx,
                                              nsISupports* aThing) {
  JS::Rooted<JS::Value> jsThing(aCx);
  if (NS_WARN_IF(MOZ_UNLIKELY(!ToJSValue(aCx, *aThing, &jsThing)))) {
    // Do we want to add error handling for this?
    return;
  }
  IgnoredErrorResult rv;
  // EnqueueNative is CAN_RUN_SCRIPT. Need a strong-ref temporary.
  auto stream = mStream;
  stream->EnqueueNative(aCx, jsThing, rv);
}

void nsISupportsStreamSource::EnqueueOneThingFromQueue(JSContext* aCx) {
  if (!mQueue.IsEmpty()) {
    RefPtr<nsISupports> thing = mQueue[0];
    mQueue.RemoveElementAt(0);
    EnqueueToStream(aCx, thing);
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(WritableStreamRTCFrameSink,
                                   UnderlyingSinkAlgorithmsWrapper,
                                   mTransformer)
NS_IMPL_ADDREF_INHERITED(WritableStreamRTCFrameSink,
                         UnderlyingSinkAlgorithmsWrapper)
NS_IMPL_RELEASE_INHERITED(WritableStreamRTCFrameSink,
                          UnderlyingSinkAlgorithmsWrapper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WritableStreamRTCFrameSink)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSinkAlgorithmsWrapper)

WritableStreamRTCFrameSink::WritableStreamRTCFrameSink(
    RTCRtpScriptTransformer* aTransformer)
    : mTransformer(aTransformer) {}

WritableStreamRTCFrameSink::~WritableStreamRTCFrameSink() = default;

already_AddRefed<Promise> WritableStreamRTCFrameSink::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aError) {
  // Spec does not say to do this right now. Might be a spec bug, needs
  // clarification.
  // https://github.com/w3c/webrtc-encoded-transform/issues/191
  if (NS_WARN_IF(!aChunk.isObject())) {
    aError.ThrowTypeError(
        "Wrong type for RTCRtpScriptTransformer.[[writeable]]: Not an object");
    return nullptr;
  }

  // Lame. But, without a webidl base class, this is the only way.
  RefPtr<RTCEncodedVideoFrame> video;
  UNWRAP_OBJECT(RTCEncodedVideoFrame, &aChunk.toObject(), video);
  RefPtr<RTCEncodedAudioFrame> audio;
  UNWRAP_OBJECT(RTCEncodedAudioFrame, &aChunk.toObject(), audio);

  RefPtr<RTCEncodedFrameBase> frame;
  if (video) {
    frame = video;
  } else if (audio) {
    frame = audio;
  }

  if (NS_WARN_IF(!frame)) {
    aError.ThrowTypeError(
        "Wrong type for RTCRtpScriptTransformer.[[writeable]]: Not an "
        "RTCEncodedAudioFrame or RTCEncodedVideoFrame");
    return nullptr;
  }

  return mTransformer->OnTransformedFrame(frame, aError);
}

// There is not presently an implementation of these for nsTHashMap :(
inline void ImplCycleCollectionUnlink(
    RTCRtpScriptTransformer::GenerateKeyFramePromises& aMap) {
  for (auto& tableEntry : aMap) {
    ImplCycleCollectionUnlink(*tableEntry.GetModifiableData());
  }
  aMap.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    RTCRtpScriptTransformer::GenerateKeyFramePromises& aMap, const char* aName,
    uint32_t aFlags = 0) {
  for (auto& tableEntry : aMap) {
    ImplCycleCollectionTraverse(aCallback, *tableEntry.GetModifiableData(),
                                aName, aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(
    RTCRtpScriptTransformer,
    (mGlobal, mReadableSource, mReadable, mWritable, mWritableSink,
     mKeyFrameRequestPromises, mGenerateKeyFramePromises),
    (mOptions))

NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpScriptTransformer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpScriptTransformer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpScriptTransformer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

RTCRtpScriptTransformer::RTCRtpScriptTransformer(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal),
      mReadableSource(new nsISupportsStreamSource),
      mWritableSink(new WritableStreamRTCFrameSink(this)),
      mOptions(JS::UndefinedHandleValue) {
  mozilla::HoldJSObjects(this);
}

RTCRtpScriptTransformer::~RTCRtpScriptTransformer() {
  mozilla::DropJSObjects(this);
}

nsresult RTCRtpScriptTransformer::Init(JSContext* aCx,
                                       JS::Handle<JS::Value> aOptions,
                                       WorkerPrivate* aWorkerPrivate,
                                       FrameTransformerProxy* aProxy) {
  ErrorResult rv;
  RefPtr<nsIGlobalObject> global(mGlobal);
  auto source = mReadableSource;
  auto sink = mWritableSink;

  // NOTE: We do not transfer these streams from mainthread, as the spec says,
  // because there's no JS observable reason to. The spec is likely to change
  // here, because it is overspecifying implementation details.
  mReadable = ReadableStream::CreateNative(aCx, global, *source, Some(1.0),
                                           nullptr, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }
  mReadableSource->Init(mReadable);

  // WritableStream::CreateNative takes a nsIGlobalObject&, but
  // ReadableStream::CreateNative takes a nsIGlobalObject*?
  mWritable =
      WritableStream::CreateNative(aCx, *global, *sink, Nothing(), nullptr, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  mOptions = aOptions;
  mProxy = aProxy;
  // This will return null if the worker is already shutting down.
  // A call to ReleaseScriptTransformer will eventually result in a call to
  // NotifyReleased.
  mWorkerRef = StrongWorkerRef::Create(
      aWorkerPrivate, "RTCRtpScriptTransformer",
      [this, self = RefPtr(this)]() { mProxy->ReleaseScriptTransformer(); });
  if (mWorkerRef) {
    mProxy->SetScriptTransformer(*this);
  }
  return NS_OK;
}

void RTCRtpScriptTransformer::NotifyReleased() {
  RejectPendingPromises();
  mWorkerRef = nullptr;
  mProxy = nullptr;
}

void RTCRtpScriptTransformer::RejectPendingPromises() {
  for (const auto& promise : mKeyFrameRequestPromises) {
    ErrorResult rv;
    rv.ThrowInvalidStateError(
        "RTCRtpScriptTransformer is not associated with a receiver");
    promise->MaybeReject(std::move(rv));
  }
  mKeyFrameRequestPromises.Clear();

  // GenerateKeyFrame promises are indexed by rid
  for (auto& ridAndPromises : mGenerateKeyFramePromises) {
    for (const auto& promise : ridAndPromises.GetData()) {
      ErrorResult rv;
      rv.ThrowInvalidStateError(
          "RTCRtpScriptTransformer is not associated with a sender");
      promise->MaybeReject(std::move(rv));
    }
  }
  mGenerateKeyFramePromises.Clear();
}

void RTCRtpScriptTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame) {
  if (!mVideo.isSome()) {
    // First frame. mProxy will know whether it's video or not by now.
    mVideo = mProxy->IsVideo();
    MOZ_ASSERT(mVideo.isSome());
  }

  RefPtr<RTCEncodedFrameBase> domFrame;
  if (*mVideo) {
    // If this is a send video keyframe, resolve any pending GenerateKeyFrame
    // promises for its rid.
    if (aFrame->GetDirection() ==
        webrtc::TransformableFrameInterface::Direction::kSender) {
      auto* videoFrame =
          static_cast<webrtc::TransformableVideoFrameInterface*>(aFrame.get());
      if (videoFrame->IsKeyFrame()) {
        ResolveGenerateKeyFramePromises(videoFrame->GetRid(),
                                        videoFrame->GetTimestamp());
        if (!videoFrame->GetRid().empty() &&
            videoFrame->Metadata().GetSimulcastIdx() == 0) {
          ResolveGenerateKeyFramePromises("", videoFrame->GetTimestamp());
        }
      }
    }
    domFrame = new RTCEncodedVideoFrame(mGlobal, std::move(aFrame),
                                        ++mLastEnqueuedFrameCounter, this);
  } else {
    domFrame = new RTCEncodedAudioFrame(mGlobal, std::move(aFrame),
                                        ++mLastEnqueuedFrameCounter, this);
  }
  mReadableSource->Enqueue(domFrame);
}

void RTCRtpScriptTransformer::GetOptions(JSContext* aCx,
                                         JS::MutableHandle<JS::Value> aVal,
                                         ErrorResult& aError) {
  if (!ToJSValue(aCx, mOptions, aVal)) {
    aError.NoteJSContextException(aCx);
  }
}

already_AddRefed<Promise> RTCRtpScriptTransformer::GenerateKeyFrame(
    const Optional<nsAString>& aRid) {
  Maybe<std::string> utf8Rid;
  if (aRid.WasPassed()) {
    utf8Rid = Some(NS_ConvertUTF16toUTF8(aRid.Value()).get());
    std::string error;
    if (!SdpRidAttributeList::CheckRidValidity(*utf8Rid, &error)) {
      ErrorResult rv;
      nsCString nsError(error.c_str());
      rv.ThrowNotAllowedError(nsError);
      return Promise::CreateRejectedWithErrorResult(GetParentObject(), rv);
    }
  }

  nsCString key;
  if (utf8Rid.isSome()) {
    key.Assign(utf8Rid->data(), utf8Rid->size());
  }

  nsTArray<RefPtr<Promise>>& promises =
      mGenerateKeyFramePromises.LookupOrInsert(key);
  if (!promises.Length()) {
    // No pending keyframe generation request for this rid. Make one.
    if (!mProxy || !mProxy->GenerateKeyFrame(utf8Rid)) {
      ErrorResult rv;
      rv.ThrowInvalidStateError(
          "RTCRtpScriptTransformer is not associated with a video sender");
      return Promise::CreateRejectedWithErrorResult(GetParentObject(), rv);
    }
  }
  RefPtr<Promise> promise = Promise::CreateInfallible(GetParentObject());
  promises.AppendElement(promise);
  return promise.forget();
}

void RTCRtpScriptTransformer::ResolveGenerateKeyFramePromises(
    const std::string& aRid, uint64_t aTimestamp) {
  nsCString key(aRid.data(), aRid.size());
  nsTArray<RefPtr<Promise>> promises;
  mGenerateKeyFramePromises.Remove(key, &promises);
  for (auto& promise : promises) {
    promise->MaybeResolve(aTimestamp);
  }
}

void RTCRtpScriptTransformer::GenerateKeyFrameError(
    const Maybe<std::string>& aRid, const CopyableErrorResult& aResult) {
  nsCString key;
  if (aRid.isSome()) {
    key.Assign(aRid->data(), aRid->size());
  }
  nsTArray<RefPtr<Promise>> promises;
  mGenerateKeyFramePromises.Remove(key, &promises);
  for (auto& promise : promises) {
    CopyableErrorResult rv(aResult);
    promise->MaybeReject(std::move(rv));
  }
}

already_AddRefed<Promise> RTCRtpScriptTransformer::SendKeyFrameRequest() {
  if (!mKeyFrameRequestPromises.Length()) {
    if (!mProxy || !mProxy->RequestKeyFrame()) {
      ErrorResult rv;
      rv.ThrowInvalidStateError(
          "RTCRtpScriptTransformer is not associated with a video receiver");
      return Promise::CreateRejectedWithErrorResult(GetParentObject(), rv);
    }
  }
  RefPtr<Promise> promise = Promise::CreateInfallible(GetParentObject());
  mKeyFrameRequestPromises.AppendElement(promise);
  return promise.forget();
}

void RTCRtpScriptTransformer::KeyFrameRequestDone(bool aSuccess) {
  auto promises = std::move(mKeyFrameRequestPromises);
  if (aSuccess) {
    for (const auto& promise : promises) {
      promise->MaybeResolveWithUndefined();
    }
  } else {
    for (const auto& promise : promises) {
      ErrorResult rv;
      rv.ThrowInvalidStateError(
          "Depacketizer is not defined, or not processing");
      promise->MaybeReject(std::move(rv));
    }
  }
}

JSObject* RTCRtpScriptTransformer::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return RTCRtpScriptTransformer_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> RTCRtpScriptTransformer::OnTransformedFrame(
    RTCEncodedFrameBase* aFrame, ErrorResult& aError) {
  // Spec says to skip frames that are out of order or have wrong owner
  if (aFrame->GetCounter() > mLastReceivedFrameCounter &&
      aFrame->CheckOwner(this) && mProxy) {
    mLastReceivedFrameCounter = aFrame->GetCounter();
    mProxy->OnTransformedFrame(aFrame->TakeFrame());
  }

  return Promise::CreateResolvedWithUndefined(GetParentObject(), aError);
}

}  // namespace mozilla::dom

#undef LOGTAG
