/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncoderTemplate.h"

#include "EncoderTypes.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/WorkerCommon.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsThreadUtils.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla::dom {

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gWebCodecsLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGW
#  undef LOGW
#endif  // LOGW
#define LOGW(msg, ...) LOG_INTERNAL(Warning, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#ifdef LOGV
#  undef LOGV
#endif  // LOGV
#define LOGV(msg, ...) LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

/*
 * Below are ControlMessage classes implementations
 */

template <typename EncoderType>
EncoderTemplate<EncoderType>::ControlMessage::ControlMessage(
    WebCodecsId aConfigureId)
    : mConfigureId(aConfigureId), mMessageId(sNextId++) {}

template <typename EncoderType>
EncoderTemplate<EncoderType>::ConfigureMessage::ConfigureMessage(
    WebCodecsId aConfigureId, const RefPtr<ConfigTypeInternal>& aConfig)
    : ControlMessage(aConfigureId), mConfig(aConfig) {}

template <typename EncoderType>
EncoderTemplate<EncoderType>::EncodeMessage::EncodeMessage(
    WebCodecsId aConfigureId, RefPtr<InputTypeInternal>&& aData,
    Maybe<VideoEncoderEncodeOptions>&& aOptions)
    : ControlMessage(aConfigureId), mData(aData) {}

template <typename EncoderType>
EncoderTemplate<EncoderType>::FlushMessage::FlushMessage(
    WebCodecsId aConfigureId)
    : ControlMessage(aConfigureId) {}

/*
 * Below are EncoderTemplate implementation
 */

template <typename EncoderType>
EncoderTemplate<EncoderType>::EncoderTemplate(
    nsIGlobalObject* aGlobalObject,
    RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
    RefPtr<OutputCallbackType>&& aOutputCallback)
    : DOMEventTargetHelper(aGlobalObject),
      mErrorCallback(std::move(aErrorCallback)),
      mOutputCallback(std::move(aOutputCallback)),
      mState(CodecState::Unconfigured),
      mMessageQueueBlocked(false),
      mEncodeQueueSize(0),
      mDequeueEventScheduled(false),
      mLatestConfigureId(0),
      mEncodeCounter(0),
      mFlushCounter(0) {}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::Configure(const ConfigType& aConfig,
                                             ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s::Configure %p codec %s", EncoderType::Name.get(), this,
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsCString errorMessage;
  if (!EncoderType::Validate(aConfig, errorMessage)) {
    LOG("Configure: Validate error: %s", errorMessage.get());
    aRv.ThrowTypeError(errorMessage);
    return;
  }

  if (mState == CodecState::Closed) {
    LOG("Configure: CodecState::Closed, rejecting with InvalidState");
    aRv.ThrowInvalidStateError("The codec is no longer usable");
    return;
  }

  // Clone a ConfigType as the active Encode config.
  RefPtr<ConfigTypeInternal> config =
      EncoderType::CreateConfigInternal(aConfig);
  if (!config) {
    CloseInternal(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  mState = CodecState::Configured;
  mEncodeCounter = 0;
  mFlushCounter = 0;

  mControlMessageQueue.push(MakeRefPtr<ConfigureMessage>(sNextId++, config));
  mLatestConfigureId = mControlMessageQueue.back()->mMessageId;
  LOG("%s %p enqueues %s", EncoderType::Name.get(), this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::EncodeAudioData(InputType& aInput,
                                                   ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, EncodeAudioData", EncoderType::Name.get(), this);

  if (mState != CodecState::Configured) {
    aRv.ThrowInvalidStateError("Encoder must be configured first");
    return;
  }

  if (aInput.IsClosed()) {
    aRv.ThrowTypeError("input AudioData has been closed");
    return;
  }

  mEncodeQueueSize += 1;
  // Dummy options here as a shortcut
  mControlMessageQueue.push(MakeRefPtr<EncodeMessage>(
      mLatestConfigureId,
      EncoderType::CreateInputInternal(aInput, VideoEncoderEncodeOptions())));
  LOGV("%s %p enqueues %s", EncoderType::Name.get(), this,
       mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::EncodeVideoFrame(
    InputType& aInput, const VideoEncoderEncodeOptions& aOptions,
    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s::Encode %p %s", EncoderType::Name.get(), this,
      aInput.ToString().get());

  if (mState != CodecState::Configured) {
    aRv.ThrowInvalidStateError("Encoder must be configured first");
    return;
  }

  if (aInput.IsClosed()) {
    aRv.ThrowTypeError("input VideoFrame has been closed");
    return;
  }

  mEncodeQueueSize += 1;
  mControlMessageQueue.push(MakeRefPtr<EncodeMessage>(
      mLatestConfigureId, EncoderType::CreateInputInternal(aInput, aOptions),
      Some(aOptions)));
  LOGV("%s %p enqueues %s", EncoderType::Name.get(), this,
       mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

template <typename EncoderType>
already_AddRefed<Promise> EncoderTemplate<EncoderType>::Flush(
    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s::Flush %p", EncoderType::Name.get(), this);

  if (mState != CodecState::Configured) {
    LOG("%s %p, wrong state!", EncoderType::Name.get(), this);
    aRv.ThrowInvalidStateError("Encoder must be configured first");
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return p.forget();
  }

  auto msg = MakeRefPtr<FlushMessage>(mLatestConfigureId);
  const auto flushPromiseId = static_cast<int64_t>(msg->mMessageId);
  MOZ_ASSERT(!mPendingFlushPromises.Contains(flushPromiseId));
  mPendingFlushPromises.Insert(flushPromiseId, p);

  mControlMessageQueue.emplace(std::move(msg));

  LOG("%s %p enqueues %s", EncoderType::Name.get(), this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
  return p.forget();
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::Reset(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Reset", EncoderType::Name.get(), this);

  if (auto r = ResetInternal(NS_ERROR_DOM_ABORT_ERR); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::Close(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Close", EncoderType::Name.get(), this);

  if (auto r = CloseInternalWithAbort(); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

template <typename EncoderType>
Result<Ok, nsresult> EncoderTemplate<EncoderType>::ResetInternal(
    const nsresult& aResult) {
  AssertIsOnOwningThread();

  LOG("%s::Reset %p", EncoderType::Name.get(), this);

  if (mState == CodecState::Closed) {
    return Err(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  mState = CodecState::Unconfigured;
  mEncodeCounter = 0;
  mFlushCounter = 0;

  CancelPendingControlMessagesAndFlushPromises(aResult);
  DestroyEncoderAgentIfAny();

  if (mEncodeQueueSize > 0) {
    mEncodeQueueSize = 0;
    ScheduleDequeueEvent();
  }

  StopBlockingMessageQueue();

  return Ok();
}

template <typename EncoderType>
Result<Ok, nsresult> EncoderTemplate<EncoderType>::CloseInternalWithAbort() {
  AssertIsOnOwningThread();

  MOZ_TRY(ResetInternal(NS_ERROR_DOM_ABORT_ERR));
  mState = CodecState::Closed;
  return Ok();
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::CloseInternal(const nsresult& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult != NS_ERROR_DOM_ABORT_ERR, "Use CloseInternalWithAbort");

  auto r = ResetInternal(aResult);
  if (r.isErr()) {
    nsCString name;
    GetErrorName(r.unwrapErr(), name);
    LOGE("Error during ResetInternal during CloseInternal: %s", name.get());
  }
  mState = CodecState::Closed;
  nsCString error;
  GetErrorName(aResult, error);
  LOGE("%s %p Close on error: %s", EncoderType::Name.get(), this, error.get());
  ReportError(aResult);
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::ReportError(const nsresult& aResult) {
  AssertIsOnOwningThread();

  RefPtr<DOMException> e = DOMException::Create(aResult);
  RefPtr<WebCodecsErrorCallback> cb(mErrorCallback);
  cb->Call(*e);
}

template <>
void EncoderTemplate<VideoEncoderTraits>::OutputEncodedVideoData(
    const nsTArray<RefPtr<MediaRawData>>&& aData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(mActiveConfig);

  // Get JSContext for RootedDictionary.
  // The EncoderType::MetadataType, VideoDecoderConfig, and VideoColorSpaceInit
  // below are rooted to work around the JS hazard issues.
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    LOGE("%s %p AutoJSAPI init failed", VideoEncoderTraits::Name.get(), this);
    return;
  }
  JSContext* cx = jsapi.cx();

  RefPtr<EncodedVideoChunkOutputCallback> cb(mOutputCallback);
  for (auto& data : aData) {
    // It's possible to have reset() called in between this task having been
    // dispatched, and running -- no output callback should happen when that's
    // the case.
    // This is imprecise in the spec, but discussed in
    // https://github.com/w3c/webcodecs/issues/755 and agreed upon.
    if (!mActiveConfig) {
      return;
    }
    RefPtr<EncodedVideoChunk> encodedData =
        EncodedDataToOutputType(GetParentObject(), data);

    RootedDictionary<EncodedVideoChunkMetadata> metadata(cx);
    if (mOutputNewDecoderConfig) {
      RootedDictionary<VideoDecoderConfig> decoderConfig(cx);
      EncoderConfigToDecoderConfig(cx, data, *mActiveConfig, decoderConfig);
      metadata.mDecoderConfig.Construct(std::move(decoderConfig));
      mOutputNewDecoderConfig = false;
      LOG("New config passed to output callback");
    }

    nsAutoCString metadataInfo;

    if (data->mTemporalLayerId) {
      RootedDictionary<SvcOutputMetadata> svc(cx);
      svc.mTemporalLayerId.Construct(data->mTemporalLayerId.value());
      metadata.mSvc.Construct(std::move(svc));
      metadataInfo.Append(
          nsPrintfCString(", temporal layer id %d",
                          metadata.mSvc.Value().mTemporalLayerId.Value()));
    }

    if (metadata.mDecoderConfig.WasPassed()) {
      metadataInfo.Append(", new decoder config");
    }

    LOG("EncoderTemplate:: output callback (ts: % " PRId64 ")%s",
        encodedData->Timestamp(), metadataInfo.get());
    cb->Call((EncodedVideoChunk&)(*encodedData), metadata);
  }
}

template <>
void EncoderTemplate<AudioEncoderTraits>::OutputEncodedAudioData(
    const nsTArray<RefPtr<MediaRawData>>&& aData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(mActiveConfig);

  // Get JSContext for RootedDictionary.
  // The EncoderType::MetadataType, AudioDecoderConfig
  // below are rooted to work around the JS hazard issues.
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetParentObject())) {
    LOGE("%s %p AutoJSAPI init failed", AudioEncoderTraits::Name.get(), this);
    return;
  }
  JSContext* cx = jsapi.cx();

  RefPtr<EncodedAudioChunkOutputCallback> cb(mOutputCallback);
  for (auto& data : aData) {
    // It's possible to have reset() called in between this task having been
    // dispatched, and running -- no output callback should happen when that's
    // the case.
    // This is imprecise in the spec, but discussed in
    // https://github.com/w3c/webcodecs/issues/755 and agreed upon.
    if (!mActiveConfig) {
      return;
    }
    RefPtr<EncodedAudioChunk> encodedData =
        EncodedDataToOutputType(GetParentObject(), data);

    RootedDictionary<EncodedAudioChunkMetadata> metadata(cx);
    if (mOutputNewDecoderConfig) {
      RootedDictionary<AudioDecoderConfig> decoderConfig(cx);
      EncoderConfigToDecoderConfig(cx, data, *mActiveConfig, decoderConfig);
      metadata.mDecoderConfig.Construct(std::move(decoderConfig));
      mOutputNewDecoderConfig = false;
      LOG("New config passed to output callback");
    }

    nsAutoCString metadataInfo;

    if (metadata.mDecoderConfig.WasPassed()) {
      metadataInfo.Append(", new decoder config");
    }

    LOG("EncoderTemplate:: output callback (ts: % " PRId64
        ", duration: % " PRId64 ", %zu bytes, %" PRIu64 " so far)",
        encodedData->Timestamp(),
        !encodedData->GetDuration().IsNull()
            ? encodedData->GetDuration().Value()
            : 0,
        data->Size(), mPacketsOutput++);
    cb->Call((EncodedAudioChunk&)(*encodedData), metadata);
  }
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::ScheduleDequeueEvent() {
  AssertIsOnOwningThread();

  if (mDequeueEventScheduled) {
    return;
  }
  mDequeueEventScheduled = true;

  QueueATask("dequeue event task", [self = RefPtr{this}]() {
    self->FireEvent(nsGkAtoms::ondequeue, u"dequeue"_ns);
    self->mDequeueEventScheduled = false;
  });
}

template <typename EncoderType>
nsresult EncoderTemplate<EncoderType>::FireEvent(nsAtom* aTypeWithOn,
                                                 const nsAString& aEventType) {
  if (aTypeWithOn && !HasListenersFor(aTypeWithOn)) {
    return NS_ERROR_ABORT;
  }

  LOGV("Dispatching %s event to %s %p", NS_ConvertUTF16toUTF8(aEventType).get(),
       EncoderType::Name.get(), this);
  RefPtr<Event> event = new Event(this, nullptr, nullptr);
  event->InitEvent(aEventType, true, true);
  event->SetTrusted(true);
  this->DispatchEvent(*event);
  return NS_OK;
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::SchedulePromiseResolveOrReject(
    already_AddRefed<Promise> aPromise, const nsresult& aResult) {
  AssertIsOnOwningThread();

  RefPtr<Promise> p = aPromise;
  auto resolver = [p, result = aResult] {
    if (NS_FAILED(result)) {
      p->MaybeReject(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
      return;
    }
    p->MaybeResolveWithUndefined();
  };
  nsISerialEventTarget* target = GetCurrentSerialEventTarget();

  if (NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewRunnableFunction(
        "SchedulePromiseResolveOrReject Runnable (main)", resolver)));
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(target->Dispatch(NS_NewCancelableRunnableFunction(
      "SchedulePromiseResolveOrReject Runnable (worker)", resolver)));
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::ProcessControlMessageQueue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);

  while (!mMessageQueueBlocked && !mControlMessageQueue.empty()) {
    RefPtr<ControlMessage>& msg = mControlMessageQueue.front();
    if (msg->AsConfigureMessage()) {
      if (ProcessConfigureMessage(msg->AsConfigureMessage()) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    } else if (msg->AsEncodeMessage()) {
      if (ProcessEncodeMessage(msg->AsEncodeMessage()) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    } else {
      MOZ_ASSERT(msg->AsFlushMessage());
      if (ProcessFlushMessage(msg->AsFlushMessage()) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    }
  }
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::CancelPendingControlMessagesAndFlushPromises(
    const nsresult& aResult) {
  AssertIsOnOwningThread();

  // Cancel the message that is being processed.
  if (mProcessingMessage) {
    LOG("%s %p cancels current %s", EncoderType::Name.get(), this,
        mProcessingMessage->ToString().get());
    mProcessingMessage->Cancel();
    mProcessingMessage = nullptr;
  }

  // Clear the message queue.
  while (!mControlMessageQueue.empty()) {
    LOG("%s %p cancels pending %s", EncoderType::Name.get(), this,
        mControlMessageQueue.front()->ToString().get());

    MOZ_ASSERT(!mControlMessageQueue.front()->IsProcessing());
    mControlMessageQueue.pop();
  }

  // If there are pending flush promises, reject them.
  mPendingFlushPromises.ForEach(
      [&](const int64_t& id, const RefPtr<Promise>& p) {
        LOG("%s %p, reject the promise for flush %" PRId64,
            EncoderType::Name.get(), this, id);
        p->MaybeReject(aResult);
      });
  mPendingFlushPromises.Clear();
}

template <typename EncoderType>
template <typename Func>
void EncoderTemplate<EncoderType>::QueueATask(const char* aName,
                                              Func&& aSteps) {
  AssertIsOnOwningThread();
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(
      NS_NewRunnableFunction(aName, std::forward<Func>(aSteps))));
}

template <typename EncoderType>
MessageProcessedResult EncoderTemplate<EncoderType>::ProcessConfigureMessage(
    RefPtr<ConfigureMessage> aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsConfigureMessage());

  if (mProcessingMessage) {
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage = aMessage;
  mControlMessageQueue.pop();
  LOG("%s %p Configuring, message queue processing blocked(%s)",
      EncoderType::Name.get(), this, aMessage->ToString().get());
  StartBlockingMessageQueue();

  bool supported = EncoderType::IsSupported(*aMessage->Config());

  if (!supported) {
    LOGE("%s %p ProcessConfigureMessage error (sync): Not supported",
         EncoderType::Name.get(), this);
    mProcessingMessage = nullptr;
    QueueATask(
        "Error while configuring encoder",
        [self = RefPtr(this)]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
          LOGE("%s %p ProcessConfigureMessage (async close): Not supported",
               EncoderType::Name.get(), self.get());
          self->CloseInternal(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        });
    return MessageProcessedResult::Processed;
  }

  if (mAgent) {
    Reconfigure(aMessage);
  } else {
    Configure(aMessage);
  }

  return MessageProcessedResult::Processed;
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::StartBlockingMessageQueue() {
  LOG("=== Message queue blocked");
  mMessageQueueBlocked = true;
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::StopBlockingMessageQueue() {
  LOG("=== Message queue unblocked");
  mMessageQueueBlocked = false;
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::OutputEncodedData(
    const nsTArray<RefPtr<MediaRawData>>&& aData) {
  if constexpr (std::is_same_v<EncoderType, VideoEncoderTraits>) {
    OutputEncodedVideoData(std::move(aData));
  } else {
    OutputEncodedAudioData(std::move(aData));
  }
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::Reconfigure(
    RefPtr<ConfigureMessage> aMessage) {
  MOZ_ASSERT(mAgent);

  LOG("Reconfiguring encoder: %s", aMessage->Config()->ToString().get());

  RefPtr<ConfigTypeInternal> config = aMessage->Config();
  RefPtr<WebCodecsConfigurationChangeList> configDiff =
      config->Diff(*mActiveConfig);

  // Nothing to do, return now, but per spec the config
  // must be output next time a packet is output.
  if (configDiff->Empty()) {
    mOutputNewDecoderConfig = true;
    LOG("Reconfigure with identical config, returning.");
    mProcessingMessage = nullptr;
    StopBlockingMessageQueue();
    return;
  }

  LOG("Attempting to reconfigure encoder: old: %s new: %s, diff: %s",
      mActiveConfig->ToString().get(), config->ToString().get(),
      configDiff->ToString().get());

  RefPtr<EncoderConfigurationChangeList> changeList =
      configDiff->ToPEMChangeList();

  // Attempt to reconfigure the encoder, if the config is similar enough.
  // Otherwise, or if reconfiguring on the fly didn't work, flush the encoder
  // and recreate a new one.

  mAgent->Reconfigure(changeList)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, id = mAgent->mId,
           message = std::move(aMessage)](
              const EncoderAgent::ReconfigurationPromise::ResolveOrRejectValue&
                  aResult) {
            MOZ_ASSERT(self->mProcessingMessage);
            MOZ_ASSERT(self->mProcessingMessage->AsConfigureMessage());
            MOZ_ASSERT(self->mState == CodecState::Configured);
            MOZ_ASSERT(self->mAgent);
            MOZ_ASSERT(id == self->mAgent->mId);
            MOZ_ASSERT(self->mActiveConfig);

            if (aResult.IsReject()) {
              LOGE(
                  "Reconfiguring on the fly didn't succeed, flushing and "
                  "configuring a new encoder");
              self->mAgent->Drain()->Then(
                  GetCurrentSerialEventTarget(), __func__,
                  [self, id,
                   message](EncoderAgent::EncodePromise::ResolveOrRejectValue&&
                                aResult) {
                    if (aResult.IsReject()) {
                      // The spec asks to close the encoder with an
                      // NotSupportedError so we log the exact error here.
                      const MediaResult& error = aResult.RejectValue();
                      LOGE("%s %p, EncoderAgent #%zu failed to configure: %s",
                           EncoderType::Name.get(), self.get(), id,
                           error.Description().get());

                      self->QueueATask(
                          "Error during drain during reconfigure",
                          [self = RefPtr{self}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                            MOZ_ASSERT(self->mState != CodecState::Closed);
                            self->CloseInternal(
                                NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                          });
                      return;
                    }

                    LOG("%s %p flush during reconfiguration succeeded.",
                        EncoderType::Name.get(), self.get());

                    // If flush succeeded, schedule to output encoded data
                    // first, destroy the current encoder, and proceed to create
                    // a new one.
                    MOZ_ASSERT(aResult.IsResolve());
                    nsTArray<RefPtr<MediaRawData>> data =
                        std::move(aResult.ResolveValue());

                    if (data.IsEmpty()) {
                      LOG("%s %p no data during flush for reconfiguration with "
                          "encoder destruction",
                          EncoderType::Name.get(), self.get());
                    } else {
                      LOG("%s %p Outputing %zu frames during flush "
                          " for reconfiguration with encoder destruction",
                          EncoderType::Name.get(), self.get(), data.Length());
                      self->QueueATask(
                          "Output encoded Data",
                          [self = RefPtr{self}, data = std::move(data)]()
                              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                                self->OutputEncodedData(std::move(data));
                              });
                    }

                    self->QueueATask(
                        "Destroy + recreate encoder after failed reconfigure",
                        [self = RefPtr(self), message]()
                            MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                              // Destroy the agent, and finally create a fresh
                              // encoder with the new configuration.
                              self->DestroyEncoderAgentIfAny();
                              self->Configure(message);
                            });
                  });
              return;
            }

            LOG("%s %p, EncoderAgent #%zu has been reconfigured on the fly to "
                "%s",
                EncoderType::Name.get(), self.get(), id,
                message->ToString().get());

            self->mOutputNewDecoderConfig = true;
            self->mActiveConfig = message->Config();
            self->mProcessingMessage = nullptr;
            self->StopBlockingMessageQueue();
            self->ProcessControlMessageQueue();
          });
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::Configure(
    RefPtr<ConfigureMessage> aMessage) {
  MOZ_ASSERT(!mAgent);

  LOG("Configuring encoder: %s", aMessage->Config()->ToString().get());

  mOutputNewDecoderConfig = true;
  mActiveConfig = aMessage->Config();

  bool encoderAgentCreated =
      CreateEncoderAgent(aMessage->mMessageId, aMessage->Config());
  if (!encoderAgentCreated) {
    LOGE(
        "%s %p ProcessConfigureMessage error (sync): encoder agent "
        "creation "
        "failed",
        EncoderType::Name.get(), this);
    mProcessingMessage = nullptr;
    QueueATask(
        "Error when configuring encoder (encoder agent creation failed)",
        [self = RefPtr(this)]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
          MOZ_ASSERT(self->mState != CodecState::Closed);
          LOGE(
              "%s %p ProcessConfigureMessage (async close): encoder agent "
              "creation failed",
              EncoderType::Name.get(), self.get());
          self->CloseInternal(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        });
    return;
  }

  MOZ_ASSERT(mAgent);
  MOZ_ASSERT(mActiveConfig);

  LOG("Real configuration with fresh config: %s",
      mActiveConfig->ToString().get());

  EncoderConfig config = mActiveConfig->ToEncoderConfig();
  mAgent->Configure(config)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId, aMessage](
                 const EncoderAgent::ConfigurePromise::ResolveOrRejectValue&
                     aResult) MOZ_CAN_RUN_SCRIPT_BOUNDARY {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsConfigureMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               LOG("%s %p, EncoderAgent #%zu %s has been %s. now unblocks "
                   "message-queue-processing",
                   EncoderType::Name.get(), self.get(), id,
                   aMessage->ToString().get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               aMessage->Complete();
               self->mProcessingMessage = nullptr;

               if (aResult.IsReject()) {
                 // The spec asks to close the decoder with an
                 // NotSupportedError so we log the exact error here.
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("%s %p, EncoderAgent #%zu failed to configure: %s",
                      EncoderType::Name.get(), self.get(), id,
                      error.Description().get());

                 self->QueueATask(
                     "Error during configure",
                     [self = RefPtr{self}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                       MOZ_ASSERT(self->mState != CodecState::Closed);
                       self->CloseInternal(
                           NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                     });
                 return;
               }

               self->StopBlockingMessageQueue();
               self->ProcessControlMessageQueue();
             })
      ->Track(aMessage->Request());
}

template <typename EncoderType>
MessageProcessedResult EncoderTemplate<EncoderType>::ProcessEncodeMessage(
    RefPtr<EncodeMessage> aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsEncodeMessage());

  if (mProcessingMessage) {
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage = aMessage;
  mControlMessageQueue.pop();

  LOGV("%s %p processing %s", EncoderType::Name.get(), this,
       aMessage->ToString().get());

  mEncodeQueueSize -= 1;
  ScheduleDequeueEvent();

  // Treat it like decode error if no EncoderAgent is available or the encoded
  // data is invalid.
  auto closeOnError = [&]() {
    mProcessingMessage = nullptr;
    QueueATask("Error during encode",
               [self = RefPtr{this}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                 MOZ_ASSERT(self->mState != CodecState::Closed);
                 self->CloseInternal(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
               });
    return MessageProcessedResult::Processed;
  };

  if (!mAgent) {
    LOGE("%s %p is not configured", EncoderType::Name.get(), this);
    return closeOnError();
  }

  MOZ_ASSERT(mActiveConfig);
  RefPtr<InputTypeInternal> data = aMessage->mData;
  if (!data) {
    LOGE("%s %p, data for %s is empty or invalid", EncoderType::Name.get(),
         this, aMessage->ToString().get());
    return closeOnError();
  }

  mAgent->Encode(data.get())
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId, aMessage](
                 EncoderAgent::EncodePromise::ResolveOrRejectValue&& aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsEncodeMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               nsCString msgStr = aMessage->ToString();

               aMessage->Complete();
               self->mProcessingMessage = nullptr;

               if (aResult.IsReject()) {
                 // The spec asks to queue a task to run close the decoder
                 // with an EncodingError so we log the exact error here.
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("%s %p, EncoderAgent #%zu %s failed: %s",
                      EncoderType::Name.get(), self.get(), id, msgStr.get(),
                      error.Description().get());
                 self->QueueATask(
                     "Error during encode runnable",
                     [self = RefPtr{self}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                       MOZ_ASSERT(self->mState != CodecState::Closed);
                       self->CloseInternal(
                           NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                     });
                 return;
               }

               MOZ_ASSERT(aResult.IsResolve());
               nsTArray<RefPtr<MediaRawData>> data =
                   std::move(aResult.ResolveValue());
               if (data.IsEmpty()) {
                 LOGV("%s %p got no data for %s", EncoderType::Name.get(),
                      self.get(), msgStr.get());
               } else {
                 LOGV("%s %p, schedule %zu encoded data output for %s",
                      EncoderType::Name.get(), self.get(), data.Length(),
                      msgStr.get());
                 self->QueueATask(
                     "Output encoded Data",
                     [self = RefPtr{self}, data2 = std::move(data)]()
                         MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                           self->OutputEncodedData(std::move(data2));
                         });
               }
               self->ProcessControlMessageQueue();
             })
      ->Track(aMessage->Request());

  return MessageProcessedResult::Processed;
}

template <typename EncoderType>
MessageProcessedResult EncoderTemplate<EncoderType>::ProcessFlushMessage(
    RefPtr<FlushMessage> aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsFlushMessage());

  if (mProcessingMessage) {
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage = aMessage;
  mControlMessageQueue.pop();

  LOG("%s %p starts processing %s", EncoderType::Name.get(), this,
      aMessage->ToString().get());

  // No agent, no thing to do. The promise has been rejected with the
  // appropriate error in ResetInternal already.
  if (!mAgent) {
    LOGE("%s %p no agent, nothing to do", EncoderType::Name.get(), this);
    mProcessingMessage = nullptr;
    return MessageProcessedResult::Processed;
  }

  mAgent->Drain()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId, aMessage, this](
                 EncoderAgent::EncodePromise::ResolveOrRejectValue&& aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsFlushMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               LOG("%s %p, EncoderAgent #%zu %s has been %s",
                   EncoderType::Name.get(), self.get(), id,
                   aMessage->ToString().get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               nsCString msgStr = aMessage->ToString();

               aMessage->Complete();

               // If flush failed, it means encoder fails to encode the data
               // sent before, so we treat it like an encode error. We reject
               // the promise first and then queue a task to close VideoEncoder
               // with an EncodingError.
               if (aResult.IsReject()) {
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("%s %p, EncoderAgent #%zu failed to flush: %s",
                      EncoderType::Name.get(), self.get(), id,
                      error.Description().get());
                 // Reject with an EncodingError instead of the error we got
                 // above.
                 self->QueueATask(
                     "Error during flush runnable",
                     [self = RefPtr{this}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                       // If Reset() was invoked before this task executes, the
                       // promise in mPendingFlushPromises is handled there.
                       // Otherwise, the promise is going to be rejected by
                       // CloseInternal() below.
                       self->mProcessingMessage = nullptr;
                       MOZ_ASSERT(self->mState != CodecState::Closed);
                       self->CloseInternal(
                           NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                     });
                 return;
               }

               // If flush succeeded, schedule to output encoded data first
               // and then resolve the promise, then keep processing the
               // control messages.
               MOZ_ASSERT(aResult.IsResolve());
               nsTArray<RefPtr<MediaRawData>> data =
                   std::move(aResult.ResolveValue());

               if (data.IsEmpty()) {
                 LOG("%s %p gets no data for %s", EncoderType::Name.get(),
                     self.get(), msgStr.get());
               } else {
                 LOG("%s %p, schedule %zu encoded data output for %s",
                     EncoderType::Name.get(), self.get(), data.Length(),
                     msgStr.get());
               }

               const auto flushPromiseId =
                   static_cast<int64_t>(aMessage->mMessageId);
               self->QueueATask(
                   "Flush: output encoded data task",
                   [self = RefPtr{self}, data = std::move(data),
                    flushPromiseId]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                     self->OutputEncodedData(std::move(data));
                     // If Reset() was invoked before this task executes, or
                     // during the output callback above in the execution of
                     // this task, the promise in mPendingFlushPromises is
                     // handled there. Otherwise, the promise is resolved here.
                     if (Maybe<RefPtr<Promise>> p =
                             self->mPendingFlushPromises.Take(flushPromiseId)) {
                       LOG("%s %p, resolving the promise for flush %" PRId64,
                           EncoderType::Name.get(), self.get(), flushPromiseId);
                       p.value()->MaybeResolveWithUndefined();
                     }
                   });
               self->mProcessingMessage = nullptr;
               self->ProcessControlMessageQueue();
             })
      ->Track(aMessage->Request());

  return MessageProcessedResult::Processed;
}

// CreateEncoderAgent will create an EncoderAgent paired with a xpcom-shutdown
// blocker and a worker-reference. Besides the needs mentioned in the header
// file, the blocker and the worker-reference also provides an entry point for
// us to clean up the resources. Other than the encoder dtor, Reset(), or
// Close(), the resources should be cleaned up in the following situations:
// 1. Encoder on window, closing document
// 2. Encoder on worker, closing document
// 3. Encoder on worker, terminating worker
//
// In case 1, the entry point to clean up is in the mShutdownBlocker's
// ShutdownpPomise-resolver. In case 2, the entry point is in mWorkerRef's
// shutting down callback. In case 3, the entry point is in mWorkerRef's
// shutting down callback.

template <typename EncoderType>
bool EncoderTemplate<EncoderType>::CreateEncoderAgent(
    WebCodecsId aId, RefPtr<ConfigTypeInternal> aConfig) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(!mAgent);
  MOZ_ASSERT(!mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), !mWorkerRef);

  auto resetOnFailure = MakeScopeExit([&]() {
    mAgent = nullptr;
    mActiveConfig = nullptr;
    mShutdownBlocker = nullptr;
    mWorkerRef = nullptr;
  });

  // If the encoder is on worker, get a worker reference.
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (NS_WARN_IF(!workerPrivate)) {
      return false;
    }

    // Clean up all the resources when worker is going away.
    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        workerPrivate, "EncoderTemplate::CreateEncoderAgent",
        [self = RefPtr{this}]() {
          LOG("%s %p, worker is going away", EncoderType::Name.get(),
              self.get());
          Unused << self->ResetInternal(NS_ERROR_DOM_ABORT_ERR);
        });
    if (NS_WARN_IF(!workerRef)) {
      return false;
    }

    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }

  mAgent = MakeRefPtr<EncoderAgent>(aId);

  // ShutdownBlockingTicket requires an unique name to register its own
  // nsIAsyncShutdownBlocker since each blocker needs a distinct name.
  // To do that, we use EncoderAgent's unique id to create a unique name.
  nsAutoString uniqueName;
  uniqueName.AppendPrintf(
      "Blocker for EncoderAgent #%zu (codec: %s) @ %p", mAgent->mId,
      NS_ConvertUTF16toUTF8(mActiveConfig->mCodec).get(), mAgent.get());

  mShutdownBlocker = media::ShutdownBlockingTicket::Create(
      uniqueName, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  if (!mShutdownBlocker) {
    LOGE("%s %p failed to create %s", EncoderType::Name.get(), this,
         NS_ConvertUTF16toUTF8(uniqueName).get());
    return false;
  }

  // Clean up all the resources when xpcom-will-shutdown arrives since the
  // page is going to be closed.
  mShutdownBlocker->ShutdownPromise()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("%s %p gets xpcom-will-shutdown notification for EncoderAgent "
            "#%zu",
            EncoderType::Name.get(), self.get(), id);
        Unused << self->ResetInternal(NS_ERROR_DOM_ABORT_ERR);
      },
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("%s %p removes shutdown-blocker #%zu before getting any "
            "notification. EncoderAgent should have been dropped",
            EncoderType::Name.get(), self.get(), id);
        MOZ_ASSERT(!self->mAgent || self->mAgent->mId != id);
      });

  LOG("%s %p creates EncoderAgent #%zu @ %p and its shutdown-blocker",
      EncoderType::Name.get(), this, mAgent->mId, mAgent.get());

  resetOnFailure.release();
  return true;
}

template <typename EncoderType>
void EncoderTemplate<EncoderType>::DestroyEncoderAgentIfAny() {
  AssertIsOnOwningThread();

  if (!mAgent) {
    LOG("%s %p has no EncoderAgent to destroy", EncoderType::Name.get(), this);
    return;
  }

  MOZ_ASSERT(mActiveConfig);
  MOZ_ASSERT(mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerRef);

  LOG("%s %p destroys EncoderAgent #%zu @ %p", EncoderType::Name.get(), this,
      mAgent->mId, mAgent.get());
  mActiveConfig = nullptr;
  RefPtr<EncoderAgent> agent = std::move(mAgent);
  // mShutdownBlocker should be kept alive until the shutdown is done.
  // mWorkerRef is used to ensure this task won't be discarded in worker.
  agent->Shutdown()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = agent->mId, ref = std::move(mWorkerRef),
       blocker = std::move(mShutdownBlocker)](
          const ShutdownPromise::ResolveOrRejectValue& aResult) {
        LOG("%s %p, EncoderAgent #%zu's shutdown has been %s. Drop its "
            "shutdown-blocker now",
            EncoderType::Name.get(), self.get(), id,
            aResult.IsResolve() ? "resolved" : "rejected");
      });
}

template class EncoderTemplate<VideoEncoderTraits>;
template class EncoderTemplate<AudioEncoderTraits>;

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
