/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EncoderTemplate_h
#define mozilla_dom_EncoderTemplate_h

#include <queue>

#include "EncoderAgent.h"
#include "MediaData.h"
#include "WebCodecsUtils.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/VideoEncoderBinding.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/media/MediaUtils.h"
#include "nsStringFwd.h"

namespace mozilla::dom {

class WebCodecsErrorCallback;
class Promise;
enum class CodecState : uint8_t;

using Id = size_t;

template <typename EncoderType>
class EncoderTemplate : public DOMEventTargetHelper {
  using Self = EncoderTemplate<EncoderType>;
  using ConfigType = typename EncoderType::ConfigType;
  using ConfigTypeInternal = typename EncoderType::ConfigTypeInternal;
  using OutputConfigType = typename EncoderType::OutputConfigType;
  using InputType = typename EncoderType::InputType;
  using InputTypeInternal = typename EncoderType::InputTypeInternal;
  using OutputType = typename EncoderType::OutputType;
  using OutputCallbackType = typename EncoderType::OutputCallbackType;

  /* ControlMessage classes */
 protected:
  class ConfigureMessage;
  class EncodeMessage;
  class FlushMessage;

  class ControlMessage {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ControlMessage)
    explicit ControlMessage(Id aConfigureId);
    virtual void Cancel() = 0;
    virtual bool IsProcessing() = 0;

    virtual nsCString ToString() const = 0;
    virtual RefPtr<ConfigureMessage> AsConfigureMessage() { return nullptr; }
    virtual RefPtr<EncodeMessage> AsEncodeMessage() { return nullptr; }
    virtual RefPtr<FlushMessage> AsFlushMessage() { return nullptr; }

    // For logging purposes
    const WebCodecsId mConfigureId;
    const WebCodecsId mMessageId;

   protected:
    virtual ~ControlMessage() = default;
  };

  class ConfigureMessage final
      : public ControlMessage,
        public MessageRequestHolder<EncoderAgent::ConfigurePromise> {
   public:
    ConfigureMessage(Id aConfigureId,
                     const RefPtr<ConfigTypeInternal>& aConfig);
    virtual void Cancel() override { Disconnect(); }
    virtual bool IsProcessing() override { return Exists(); };
    virtual RefPtr<ConfigureMessage> AsConfigureMessage() override {
      return this;
    }
    RefPtr<ConfigTypeInternal> Config() { return mConfig; }
    nsCString ToString() const override {
      nsCString rv;
      rv.AppendPrintf(
          "ConfigureMessage(#%zu): %s", this->mMessageId,
          mConfig ? NS_ConvertUTF16toUTF8(mConfig->ToString().get()).get()
                  : "null cfg");
      return rv;
    }

   private:
    const RefPtr<ConfigTypeInternal> mConfig;
  };

  class EncodeMessage final
      : public ControlMessage,
        public MessageRequestHolder<EncoderAgent::EncodePromise> {
   public:
    EncodeMessage(WebCodecsId aConfigureId, RefPtr<InputTypeInternal>&& aData,
                  Maybe<VideoEncoderEncodeOptions>&& aOptions = Nothing());
    nsCString ToString() const override {
      nsCString rv;
      bool isKeyFrame = mOptions.isSome() && mOptions.ref().mKeyFrame;
      rv.AppendPrintf("EncodeMessage(#%zu,#%zu): %s (%s)", this->mConfigureId,
                      this->mMessageId, mData->ToString().get(),
                      isKeyFrame ? "kf" : "");
      return rv;
    }
    virtual void Cancel() override { Disconnect(); }
    virtual bool IsProcessing() override { return Exists(); };
    virtual RefPtr<EncodeMessage> AsEncodeMessage() override { return this; }
    RefPtr<InputTypeInternal> mData;
    Maybe<VideoEncoderEncodeOptions> mOptions;
  };

  class FlushMessage final
      : public ControlMessage,
        public MessageRequestHolder<EncoderAgent::EncodePromise> {
   public:
    FlushMessage(WebCodecsId aConfigureId, Promise* aPromise);
    virtual void Cancel() override { Disconnect(); }
    virtual bool IsProcessing() override { return Exists(); };
    virtual RefPtr<FlushMessage> AsFlushMessage() override { return this; }
    already_AddRefed<Promise> TakePromise() { return mPromise.forget(); }
    void RejectPromiseIfAny(const nsresult& aReason);

    nsCString ToString() const override {
      nsCString rv;
      rv.AppendPrintf("FlushMessage(#%zu,#%zu)", this->mConfigureId,
                      this->mMessageId);
      return rv;
    }

   private:
    RefPtr<Promise> mPromise;
  };

 protected:
  EncoderTemplate(nsIGlobalObject* aGlobalObject,
                  RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
                  RefPtr<OutputCallbackType>&& aOutputCallback);

  virtual ~EncoderTemplate() = default;

  /* WebCodecs interfaces */
 public:
  IMPL_EVENT_HANDLER(dequeue)

  void StartBlockingMessageQueue();
  void StopBlockingMessageQueue();

  CodecState State() const { return mState; };

  uint32_t EncodeQueueSize() const { return mEncodeQueueSize; };

  void Configure(const ConfigType& aConfig, ErrorResult& aRv);

  void EncodeAudioData(InputType& aInput, ErrorResult& aRv);
  void EncodeVideoFrame(InputType& aInput,
                        const VideoEncoderEncodeOptions& aOptions,
                        ErrorResult& aRv);

  already_AddRefed<Promise> Flush(ErrorResult& aRv);

  void Reset(ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void Close(ErrorResult& aRv);

  /* Type conversion functions for the Encoder implementation */
 protected:
  virtual RefPtr<OutputType> EncodedDataToOutputType(
      nsIGlobalObject* aGlobalObject, RefPtr<MediaRawData>& aData) = 0;
  virtual OutputConfigType EncoderConfigToDecoderConfig(
      nsIGlobalObject* aGlobalObject, const RefPtr<MediaRawData>& aData,
      const ConfigTypeInternal& aOutputConfig) const = 0;
  /* Internal member variables and functions */
 protected:
  // EncoderTemplate can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(EncoderTemplate);
  }

  Result<Ok, nsresult> ResetInternal(const nsresult& aResult);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  Result<Ok, nsresult> CloseInternal(const nsresult& aResult);

  MOZ_CAN_RUN_SCRIPT void ReportError(const nsresult& aResult);
  MOZ_CAN_RUN_SCRIPT void OutputEncodedData(
      nsTArray<RefPtr<MediaRawData>>&& aData);

  class ErrorRunnable;
  void ScheduleReportError(const nsresult& aResult);

  class OutputRunnable;
  void ScheduleOutputEncodedData(nsTArray<RefPtr<MediaRawData>>&& aData,
                                 const nsACString& aLabel);

  void ScheduleClose(const nsresult& aResult);

  void ScheduleDequeueEvent();
  nsresult FireEvent(nsAtom* aTypeWithOn, const nsAString& aEventType);

  void SchedulePromiseResolveOrReject(already_AddRefed<Promise> aPromise,
                                      const nsresult& aResult);

  void ProcessControlMessageQueue();
  void CancelPendingControlMessages(const nsresult& aResult);

  MessageProcessedResult ProcessConfigureMessage(
      RefPtr<ConfigureMessage> aMessage);

  MessageProcessedResult ProcessEncodeMessage(RefPtr<EncodeMessage> aMessage);

  MessageProcessedResult ProcessFlushMessage(RefPtr<FlushMessage> aMessage);

  void Configure(RefPtr<ConfigureMessage> aMessage);
  void Reconfigure(RefPtr<ConfigureMessage> aMessage);

  // Returns true when mAgent can be created.
  bool CreateEncoderAgent(WebCodecsId aId, RefPtr<ConfigTypeInternal> aConfig);
  void DestroyEncoderAgentIfAny();

  // Constant in practice, only set in ctor.
  RefPtr<WebCodecsErrorCallback> mErrorCallback;
  RefPtr<OutputCallbackType> mOutputCallback;

  CodecState mState;

  bool mMessageQueueBlocked;
  std::queue<RefPtr<ControlMessage>> mControlMessageQueue;
  RefPtr<ControlMessage> mProcessingMessage;

  uint32_t mEncodeQueueSize;
  bool mDequeueEventScheduled;

  // A unique id tracking the ConfigureMessage and will be used as the
  // EncoderAgent's Id.
  uint32_t mLatestConfigureId;
  // Tracking how many encoded data has been enqueued and this number will be
  // used as the EncodeMessage's Id.
  size_t mEncodeCounter;
  // Tracking how many flush request has been enqueued and this number will be
  // used as the FlushMessage's Id.
  size_t mFlushCounter;

  // EncoderAgent will be created the first time "configure" is being processed,
  // and will be destroyed when "reset" is called. If another "configure" is
  // called, either it's possible to reconfigure the underlying encoder without
  // tearing eveyrthing down (e.g. a bitrate change), or it's not possible, and
  // the current encoder will be destroyed and a new one create.
  // In both cases, the encoder is implicitely flushed before the configuration
  // change.
  // See CanReconfigure on the {Audio,Video}EncoderConfigInternal
  RefPtr<EncoderAgent> mAgent;
  RefPtr<ConfigTypeInternal> mActiveConfig;
  // This is true when a configure call has just been processed, and it's
  // necessary to pass the new decoding configuration when the callback is
  // called. Read and modified on owner thread only.
  bool mOutputNewDecoderConfig = false;

  // Used to add a nsIAsyncShutdownBlocker on main thread to block
  // xpcom-shutdown before the underlying MediaDataEncoder is created. The
  // blocker will be held until the underlying MediaDataEncoder has been shut
  // down. This blocker guarantees RemoteEncoderManagerChild's thread, where
  // the underlying RemoteMediaDataEncoder is on, outlives the
  // RemoteMediaDataEncoder since the thread releasing, which happens on main
  // thread when getting a xpcom-shutdown signal, is blocked by the added
  // blocker. As a result, RemoteMediaDataEncoder can safely work on worker
  // thread with a holding blocker (otherwise, if RemoteEncoderManagerChild
  // releases its thread on main thread before RemoteMediaDataEncoder's
  // Shutdown() task run on worker thread, RemoteMediaDataEncoder has no
  // thread to run).
  UniquePtr<media::ShutdownBlockingTicket> mShutdownBlocker;

  // Held to make sure the dispatched tasks can be done before worker is going
  // away. As long as this worker-ref is held somewhere, the tasks dispatched
  // to the worker can be executed (otherwise the tasks would be canceled).
  // This ref should be activated as long as the underlying MediaDataEncoder
  // is alive, and should keep alive until mShutdownBlocker is dropped, so all
  // MediaDataEncoder's tasks and mShutdownBlocker-releasing task can be
  // executed.
  // TODO: Use StrongWorkerRef instead if this is always used in the same
  // thread?
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EncoderTemplate_h
