/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoDecoder_h
#define mozilla_dom_VideoDecoder_h

#include <queue>

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/DecoderAgent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStringFwd.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/VideoDecoderBinding.h"

class nsIGlobalObject;

namespace mozilla {

class MediaData;
class TrackInfo;
class VideoInfo;

namespace dom {

class EncodedVideoChunk;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class ThreadSafeWorkerRef;
class VideoFrameOutputCallback;
class WebCodecsErrorCallback;
enum class CodecState : uint8_t;
struct VideoDecoderConfig;
struct VideoDecoderInit;

}  // namespace dom

namespace media {
class ShutdownBlockingTicket;
}

}  // namespace mozilla

namespace mozilla::dom {

struct VideoColorSpaceInternal {
  explicit VideoColorSpaceInternal(const VideoColorSpaceInit& aColorSpaceInit);
  VideoColorSpaceInternal() = default;
  VideoColorSpaceInit ToColorSpaceInit() const;
  Maybe<bool> mFullRange;
  Maybe<VideoMatrixCoefficients> mMatrix;
  Maybe<VideoColorPrimaries> mPrimaries;
  Maybe<VideoTransferCharacteristics> mTransfer;
};

class VideoDecoderConfigInternal {
 public:
  static UniquePtr<VideoDecoderConfigInternal> Create(
      const VideoDecoderConfig& aConfig);
  ~VideoDecoderConfigInternal() = default;

  nsString mCodec;
  Maybe<uint32_t> mCodedHeight;
  Maybe<uint32_t> mCodedWidth;
  Maybe<VideoColorSpaceInternal> mColorSpace;
  Maybe<RefPtr<MediaByteBuffer>> mDescription;
  Maybe<uint32_t> mDisplayAspectHeight;
  Maybe<uint32_t> mDisplayAspectWidth;
  HardwareAcceleration mHardwareAcceleration;
  Maybe<bool> mOptimizeForLatency;

 private:
  VideoDecoderConfigInternal(const nsAString& aCodec,
                             Maybe<uint32_t>&& aCodedHeight,
                             Maybe<uint32_t>&& aCodedWidth,
                             Maybe<VideoColorSpaceInternal>&& aColorSpace,
                             Maybe<RefPtr<MediaByteBuffer>>&& aDescription,
                             Maybe<uint32_t>&& aDisplayAspectHeight,
                             Maybe<uint32_t>&& aDisplayAspectWidth,
                             const HardwareAcceleration& aHardwareAcceleration,
                             Maybe<bool>&& aOptimizeForLatency);
};

class ConfigureMessage;
class DecodeMessage;
class FlushMessage;

class ControlMessage {
 public:
  explicit ControlMessage(const nsACString& aTitle);
  virtual ~ControlMessage() = default;
  virtual void Cancel() = 0;
  virtual bool IsProcessing() = 0;

  virtual const nsCString& ToString() const { return mTitle; }
  virtual ConfigureMessage* AsConfigureMessage() { return nullptr; }
  virtual DecodeMessage* AsDecodeMessage() { return nullptr; }
  virtual FlushMessage* AsFlushMessage() { return nullptr; }

  const nsCString mTitle;  // Used to identify the message in the logs.
};

class VideoDecoder final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VideoDecoder, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(dequeue)

 public:
  VideoDecoder(nsIGlobalObject* aParent,
               RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
               RefPtr<VideoFrameOutputCallback>&& aOutputCallback);

 protected:
  ~VideoDecoder();

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoDecoder> Constructor(
      const GlobalObject& aGlobal, const VideoDecoderInit& aInit,
      ErrorResult& aRv);

  CodecState State() const;

  uint32_t DecodeQueueSize() const;

  void Configure(const VideoDecoderConfig& aConfig, ErrorResult& aRv);

  void Decode(EncodedVideoChunk& aChunk, ErrorResult& aRv);

  already_AddRefed<Promise> Flush(ErrorResult& aRv);

  void Reset(ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig,
      ErrorResult& aRv);

 private:
  // VideoDecoder can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(VideoDecoder); }

  Result<Ok, nsresult> Reset(const nsresult& aResult);
  Result<Ok, nsresult> Close(const nsresult& aResult);

  MOZ_CAN_RUN_SCRIPT void ReportError(const nsresult& aResult);
  MOZ_CAN_RUN_SCRIPT void OutputVideoFrames(
      nsTArray<RefPtr<MediaData>>&& aData);

  class ErrorRunnable;
  void ScheduleReportError(const nsresult& aResult);

  class OutputRunnable;
  void ScheduleOutputVideoFrames(nsTArray<RefPtr<MediaData>>&& aData,
                                 const nsACString& aLabel);

  void ScheduleClose(const nsresult& aResult);

  void ScheduleDequeueEvent();

  void SchedulePromiseResolveOrReject(already_AddRefed<Promise> aPromise,
                                      const nsresult& aResult);

  void ProcessControlMessageQueue();
  void CancelPendingControlMessages(const nsresult& aResult);

  enum class MessageProcessedResult { NotProcessed, Processed };

  MessageProcessedResult ProcessConfigureMessage(
      UniquePtr<ControlMessage>& aMessage);

  MessageProcessedResult ProcessDecodeMessage(
      UniquePtr<ControlMessage>& aMessage);

  MessageProcessedResult ProcessFlushMessage(
      UniquePtr<ControlMessage>& aMessage);

  // Returns true when mAgent can be created.
  bool CreateDecoderAgent(DecoderAgent::Id aId,
                          UniquePtr<VideoDecoderConfigInternal>&& aConfig,
                          UniquePtr<TrackInfo>&& aInfo);
  void DestroyDecoderAgentIfAny();

  // Constant in practice, only set in ::Constructor.
  RefPtr<WebCodecsErrorCallback> mErrorCallback;
  RefPtr<VideoFrameOutputCallback> mOutputCallback;

  CodecState mState;
  bool mKeyChunkRequired;

  bool mMessageQueueBlocked;
  std::queue<UniquePtr<ControlMessage>> mControlMessageQueue;
  UniquePtr<ControlMessage> mProcessingMessage;

  // DecoderAgent will be created every time "configure" is being processed, and
  // will be destroyed when "reset" or another "configure" is called (spec
  // allows calling two "configure" without a "reset" in between).
  RefPtr<DecoderAgent> mAgent;
  UniquePtr<VideoDecoderConfigInternal> mActiveConfig;
  uint32_t mDecodeQueueSize;
  bool mDequeueEventScheduled;

  // A unique id tracking the ConfigureMessage and will be used as the
  // DecoderAgent's Id.
  uint32_t mLatestConfigureId;
  // Tracking how many decode data has been enqueued and this number will be
  // used as the DecodeMessage's Id.
  size_t mDecodeCounter;
  // Tracking how many flush request has been enqueued and this number will be
  // used as the FlushMessage's Id.
  size_t mFlushCounter;

  // Used to add a nsIAsyncShutdownBlocker on main thread to block
  // xpcom-shutdown before the underlying MediaDataDecoder is created. The
  // blocker will be held until the underlying MediaDataDecoder has been shut
  // down. This blocker guarantees RemoteDecoderManagerChild's thread, where the
  // underlying RemoteMediaDataDecoder is on, outlives the
  // RemoteMediaDataDecoder, since the thread releasing, which happens on main
  // thread when getting a xpcom-shutdown signal, is blocked by the added
  // blocker. As a result, RemoteMediaDataDecoder can safely work on worker
  // thread with a holding blocker (otherwise, if RemoteDecoderManagerChild
  // releases its thread on main thread before RemoteMediaDataDecoder's
  // Shutdown() task run on worker thread, RemoteMediaDataDecoder has no thread
  // to run).
  UniquePtr<media::ShutdownBlockingTicket> mShutdownBlocker;

  // Held to make sure the dispatched tasks can be done before worker is going
  // away. As long as this worker-ref is held somewhere, the tasks dispatched to
  // the worker can be executed (otherwise the tasks would be canceled). This
  // ref should be activated as long as the underlying MediaDataDecoder is
  // alive, and should keep alive until mShutdownBlocker is dropped, so all
  // MediaDataDecoder's tasks and mShutdownBlocker-releasing task can be
  // executed.
  // TODO: Use StrongWorkerRef instead if this is always used in the same
  // thread?
  RefPtr<dom::ThreadSafeWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoDecoder_h
