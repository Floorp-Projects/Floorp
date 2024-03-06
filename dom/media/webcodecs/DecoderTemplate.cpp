/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderTemplate.h"

#include <atomic>
#include <utility>

#include "DecoderTypes.h"
#include "MediaInfo.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/VideoDecoderBinding.h"
#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/WorkerCommon.h"
#include "nsGkAtoms.h"
#include "nsString.h"
#include "nsThreadUtils.h"

mozilla::LazyLogModule gWebCodecsLog("WebCodecs");

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

template <typename DecoderType>
DecoderTemplate<DecoderType>::ControlMessage::ControlMessage(
    const nsACString& aTitle)
    : mTitle(aTitle) {}

template <typename DecoderType>
DecoderTemplate<DecoderType>::ConfigureMessage::ConfigureMessage(
    Id aId, UniquePtr<ConfigTypeInternal>&& aConfig)
    : ControlMessage(
          nsPrintfCString("configure #%d (%s)", aId,
                          NS_ConvertUTF16toUTF8(aConfig->mCodec).get())),
      mId(aId),
      mConfig(std::move(aConfig)) {}

/* static */
template <typename DecoderType>
typename DecoderTemplate<DecoderType>::ConfigureMessage*
DecoderTemplate<DecoderType>::ConfigureMessage::Create(
    UniquePtr<ConfigTypeInternal>&& aConfig) {
  // This needs to be atomic since this can run on the main thread or worker
  // thread.
  static std::atomic<Id> sNextId = NoId;
  return new ConfigureMessage(++sNextId, std::move(aConfig));
}

template <typename DecoderType>
DecoderTemplate<DecoderType>::DecodeMessage::DecodeMessage(
    Id aId, ConfigId aConfigId, UniquePtr<InputTypeInternal>&& aData)
    : ControlMessage(
          nsPrintfCString("decode #%zu (config #%d)", aId, aConfigId)),
      mId(aId),
      mData(std::move(aData)) {}

template <typename DecoderType>
DecoderTemplate<DecoderType>::FlushMessage::FlushMessage(Id aId,
                                                         ConfigId aConfigId,
                                                         Promise* aPromise)
    : ControlMessage(
          nsPrintfCString("flush #%zu (config #%d)", aId, aConfigId)),
      mId(aId),
      mPromise(aPromise) {}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::FlushMessage::RejectPromiseIfAny(
    const nsresult& aReason) {
  if (mPromise) {
    mPromise->MaybeReject(aReason);
  }
}

/*
 * Below are DecoderTemplate implementation
 */

template <typename DecoderType>
DecoderTemplate<DecoderType>::DecoderTemplate(
    nsIGlobalObject* aGlobalObject,
    RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
    RefPtr<OutputCallbackType>&& aOutputCallback)
    : DOMEventTargetHelper(aGlobalObject),
      mErrorCallback(std::move(aErrorCallback)),
      mOutputCallback(std::move(aOutputCallback)),
      mState(CodecState::Unconfigured),
      mKeyChunkRequired(true),
      mMessageQueueBlocked(false),
      mDecodeQueueSize(0),
      mDequeueEventScheduled(false),
      mLatestConfigureId(ConfigureMessage::NoId),
      mDecodeCounter(0),
      mFlushCounter(0) {}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::Configure(const ConfigType& aConfig,
                                             ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Configure: codec %s", DecoderType::Name.get(), this,
      NS_ConvertUTF16toUTF8(aConfig.mCodec).get());

  nsCString errorMessage;
  if (!DecoderType::Validate(aConfig, errorMessage)) {
    aRv.ThrowTypeError(
        nsPrintfCString("config is invalid: %s", errorMessage.get()));
    return;
  }

  if (mState == CodecState::Closed) {
    LOG("Configure: CodecState::Closed, rejecting with InvalidState");
    aRv.ThrowInvalidStateError("The codec is no longer usable");
    return;
  }

  // Clone a ConfigType as the active decoder config.
  UniquePtr<ConfigTypeInternal> config =
      DecoderType::CreateConfigInternal(aConfig);
  if (!config) {
    aRv.Throw(NS_ERROR_UNEXPECTED);  // Invalid description data.
    return;
  }

  mState = CodecState::Configured;
  mKeyChunkRequired = true;
  mDecodeCounter = 0;
  mFlushCounter = 0;

  mControlMessageQueue.emplace(
      UniquePtr<ControlMessage>(ConfigureMessage::Create(std::move(config))));
  mLatestConfigureId = mControlMessageQueue.back()->AsConfigureMessage()->mId;
  LOG("%s %p enqueues %s", DecoderType::Name.get(), this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::Decode(InputType& aInput, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Decode", DecoderType::Name.get(), this);

  if (mState != CodecState::Configured) {
    aRv.ThrowInvalidStateError("Decoder must be configured first");
    return;
  }

  if (mKeyChunkRequired) {
    // TODO: Verify input's data is truly a key chunk
    if (!DecoderType::IsKeyChunk(aInput)) {
      aRv.ThrowDataError(
          nsPrintfCString("%s needs a key chunk", DecoderType::Name.get()));
      return;
    }
    mKeyChunkRequired = false;
  }

  mDecodeQueueSize += 1;
  mControlMessageQueue.emplace(UniquePtr<ControlMessage>(
      new DecodeMessage(++mDecodeCounter, mLatestConfigureId,
                        DecoderType::CreateInputInternal(aInput))));
  LOGV("%s %p enqueues %s", DecoderType::Name.get(), this,
       mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
}

template <typename DecoderType>
already_AddRefed<Promise> DecoderTemplate<DecoderType>::Flush(
    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Flush", DecoderType::Name.get(), this);

  if (mState != CodecState::Configured) {
    LOG("%s %p, wrong state!", DecoderType::Name.get(), this);
    aRv.ThrowInvalidStateError("Decoder must be configured first");
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(GetParentObject(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return p.forget();
  }

  mKeyChunkRequired = true;

  mControlMessageQueue.emplace(UniquePtr<ControlMessage>(
      new FlushMessage(++mFlushCounter, mLatestConfigureId, p)));
  LOG("%s %p enqueues %s", DecoderType::Name.get(), this,
      mControlMessageQueue.back()->ToString().get());
  ProcessControlMessageQueue();
  return p.forget();
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::Reset(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Reset", DecoderType::Name.get(), this);

  if (auto r = ResetInternal(NS_ERROR_DOM_ABORT_ERR); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::Close(ErrorResult& aRv) {
  AssertIsOnOwningThread();

  LOG("%s %p, Close", DecoderType::Name.get(), this);

  if (auto r = CloseInternalWithAbort(); r.isErr()) {
    aRv.Throw(r.unwrapErr());
  }
}

template <typename DecoderType>
Result<Ok, nsresult> DecoderTemplate<DecoderType>::ResetInternal(
    const nsresult& aResult) {
  AssertIsOnOwningThread();

  if (mState == CodecState::Closed) {
    return Err(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  mState = CodecState::Unconfigured;
  mDecodeCounter = 0;
  mFlushCounter = 0;

  CancelPendingControlMessages(aResult);
  DestroyDecoderAgentIfAny();

  if (mDecodeQueueSize > 0) {
    mDecodeQueueSize = 0;
    ScheduleDequeueEventIfNeeded();
  }

  LOG("%s %p now has its message queue unblocked", DecoderType::Name.get(),
      this);
  mMessageQueueBlocked = false;

  return Ok();
}
template <typename DecoderType>
Result<Ok, nsresult> DecoderTemplate<DecoderType>::CloseInternalWithAbort() {
  AssertIsOnOwningThread();

  MOZ_TRY(ResetInternal(NS_ERROR_DOM_ABORT_ERR));
  mState = CodecState::Closed;
  return Ok();
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::CloseInternal(const nsresult& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aResult != NS_ERROR_DOM_ABORT_ERR, "Use CloseInternalWithAbort");

  auto r = ResetInternal(aResult);
  if (r.isErr()) {
    nsCString name;
    GetErrorName(r.unwrapErr(), name);
    LOGE("Error in ResetInternal: %s", name.get());
    MOZ_CRASH();
  }
  mState = CodecState::Closed;
  nsCString error;
  GetErrorName(aResult, error);
  LOGE("%s %p Close on error: %s", DecoderType::Name.get(), this, error.get());
  ReportError(aResult);
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::ReportError(const nsresult& aResult) {
  AssertIsOnOwningThread();

  RefPtr<DOMException> e = DOMException::Create(aResult);
  RefPtr<WebCodecsErrorCallback> cb(mErrorCallback);
  cb->Call(*e);
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::OutputDecodedData(
    const nsTArray<RefPtr<MediaData>>&& aData) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(mActiveConfig);

  nsTArray<RefPtr<VideoFrame>> frames = DecodedDataToOutputType(
      GetParentObject(), std::move(aData), *mActiveConfig);
  RefPtr<VideoFrameOutputCallback> cb(mOutputCallback);
  for (RefPtr<VideoFrame>& frame : frames) {
    LOG("Outputing decoded data: ts: %" PRId64, frame->Timestamp());
    RefPtr<VideoFrame> f = frame;
    cb->Call((VideoFrame&)(*f));
  }
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::ScheduleDequeueEventIfNeeded() {
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

template <typename DecoderType>
nsresult DecoderTemplate<DecoderType>::FireEvent(nsAtom* aTypeWithOn,
                                                 const nsAString& aEventType) {
  if (aTypeWithOn && !HasListenersFor(aTypeWithOn)) {
    LOGV("%s %p has no %s event listener", DecoderType::Name.get(), this,
         NS_ConvertUTF16toUTF8(aEventType).get());
    return NS_ERROR_ABORT;
  }

  LOGV("Dispatch %s event to %s %p", NS_ConvertUTF16toUTF8(aEventType).get(),
       DecoderType::Name.get(), this);
  RefPtr<Event> event = new Event(this, nullptr, nullptr);
  event->InitEvent(aEventType, true, true);
  event->SetTrusted(true);
  this->DispatchEvent(*event);
  return NS_OK;
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::ProcessControlMessageQueue() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);

  while (!mMessageQueueBlocked && !mControlMessageQueue.empty()) {
    UniquePtr<ControlMessage>& msg = mControlMessageQueue.front();
    if (msg->AsConfigureMessage()) {
      if (ProcessConfigureMessage(msg) ==
          MessageProcessedResult::NotProcessed) {
        break;
      }
    } else if (msg->AsDecodeMessage()) {
      if (ProcessDecodeMessage(msg) == MessageProcessedResult::NotProcessed) {
        break;
      }
    } else {
      MOZ_ASSERT(msg->AsFlushMessage());
      if (ProcessFlushMessage(msg) == MessageProcessedResult::NotProcessed) {
        break;
      }
    }
  }
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::CancelPendingControlMessages(
    const nsresult& aResult) {
  AssertIsOnOwningThread();

  // Cancel the message that is being processed.
  if (mProcessingMessage) {
    LOG("%s %p cancels current %s", DecoderType::Name.get(), this,
        mProcessingMessage->ToString().get());
    mProcessingMessage->Cancel();

    if (FlushMessage* flush = mProcessingMessage->AsFlushMessage()) {
      flush->RejectPromiseIfAny(aResult);
    }

    mProcessingMessage.reset();
  }

  // Clear the message queue.
  while (!mControlMessageQueue.empty()) {
    LOG("%s %p cancels pending %s", DecoderType::Name.get(), this,
        mControlMessageQueue.front()->ToString().get());

    MOZ_ASSERT(!mControlMessageQueue.front()->IsProcessing());
    if (FlushMessage* flush = mControlMessageQueue.front()->AsFlushMessage()) {
      flush->RejectPromiseIfAny(aResult);
    }

    mControlMessageQueue.pop();
  }
}

template <typename DecoderType>
template <typename Func>
void DecoderTemplate<DecoderType>::QueueATask(const char* aName,
                                              Func&& aSteps) {
  AssertIsOnOwningThread();
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(
      NS_NewRunnableFunction(aName, std::forward<Func>(aSteps))));
}

template <typename DecoderType>
MessageProcessedResult DecoderTemplate<DecoderType>::ProcessConfigureMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsConfigureMessage());

  if (mProcessingMessage) {
    LOG("%s %p is processing %s. Defer %s", DecoderType::Name.get(), this,
        mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  ConfigureMessage* msg = mProcessingMessage->AsConfigureMessage();
  LOG("%s %p starts processing %s", DecoderType::Name.get(), this,
      msg->ToString().get());

  DestroyDecoderAgentIfAny();

  mMessageQueueBlocked = true;

  nsAutoCString errorMessage;
  auto i = DecoderType::CreateTrackInfo(msg->Config());
  if (i.isErr()) {
    nsCString res;
    GetErrorName(i.unwrapErr(), res);
    errorMessage.AppendPrintf("CreateTrackInfo failed: %s", res.get());
  } else if (!DecoderType::IsSupported(msg->Config())) {
    errorMessage.Append("Not supported.");
  } else if (!CreateDecoderAgent(msg->mId, msg->TakeConfig(), i.unwrap())) {
    errorMessage.Append("DecoderAgent creation failed.");
  }
  if (!errorMessage.IsEmpty()) {
    LOGE("%s %p ProcessConfigureMessage error (sync): %s",
         DecoderType::Name.get(), this, errorMessage.get());

    mProcessingMessage.reset();
    QueueATask("Error while configuring decoder",
               [self = RefPtr{this}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                 MOZ_ASSERT(self->mState != CodecState::Closed);
                 self->CloseInternal(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
               });
    return MessageProcessedResult::Processed;
  }

  MOZ_ASSERT(mAgent);
  MOZ_ASSERT(mActiveConfig);

  LOG("%s %p now blocks message-queue-processing", DecoderType::Name.get(),
      this);

  bool preferSW = mActiveConfig->mHardwareAcceleration ==
                  HardwareAcceleration::Prefer_software;
  bool lowLatency = mActiveConfig->mOptimizeForLatency.isSome() &&
                    mActiveConfig->mOptimizeForLatency.value();
  mAgent->Configure(preferSW, lowLatency)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr{this}, id = mAgent->mId](
                 const DecoderAgent::ConfigurePromise::ResolveOrRejectValue&
                     aResult) {
               MOZ_ASSERT(self->mProcessingMessage);
               MOZ_ASSERT(self->mProcessingMessage->AsConfigureMessage());
               MOZ_ASSERT(self->mState == CodecState::Configured);
               MOZ_ASSERT(self->mAgent);
               MOZ_ASSERT(id == self->mAgent->mId);
               MOZ_ASSERT(self->mActiveConfig);

               ConfigureMessage* msg =
                   self->mProcessingMessage->AsConfigureMessage();
               LOG("%s %p, DecoderAgent #%d %s has been %s. now unblocks "
                   "message-queue-processing",
                   DecoderType::Name.get(), self.get(), id,
                   msg->ToString().get(),
                   aResult.IsResolve() ? "resolved" : "rejected");

               msg->Complete();
               self->mProcessingMessage.reset();

               if (aResult.IsReject()) {
                 // The spec asks to close the decoder with an
                 // NotSupportedError so we log the exact error here.
                 const MediaResult& error = aResult.RejectValue();
                 LOGE("%s %p, DecoderAgent #%d failed to configure: %s",
                      DecoderType::Name.get(), self.get(), id,
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

               self->mMessageQueueBlocked = false;
               self->ProcessControlMessageQueue();
             })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

template <typename DecoderType>
MessageProcessedResult DecoderTemplate<DecoderType>::ProcessDecodeMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsDecodeMessage());

  if (mProcessingMessage) {
    LOGV("%s %p is processing %s. Defer %s", DecoderType::Name.get(), this,
         mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  DecodeMessage* msg = mProcessingMessage->AsDecodeMessage();
  LOGV("%s %p starts processing %s", DecoderType::Name.get(), this,
       msg->ToString().get());

  mDecodeQueueSize -= 1;
  ScheduleDequeueEventIfNeeded();

  // Treat it like decode error if no DecoderAgent is available or the encoded
  // data is invalid.
  auto closeOnError = [&]() {
    mProcessingMessage.reset();
    QueueATask("Error during decode",
               [self = RefPtr{this}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                 MOZ_ASSERT(self->mState != CodecState::Closed);
                 self->CloseInternal(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
               });
    return MessageProcessedResult::Processed;
  };

  if (!mAgent) {
    LOGE("%s %p is not configured", DecoderType::Name.get(), this);
    return closeOnError();
  }

  MOZ_ASSERT(mActiveConfig);
  RefPtr<MediaRawData> data = InputDataToMediaRawData(
      std::move(msg->mData), *(mAgent->mInfo), *mActiveConfig);
  if (!data) {
    LOGE("%s %p, data for %s is empty or invalid", DecoderType::Name.get(),
         this, msg->ToString().get());
    return closeOnError();
  }

  mAgent->Decode(data.get())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, id = mAgent->mId](
              DecoderAgent::DecodePromise::ResolveOrRejectValue&& aResult) {
            MOZ_ASSERT(self->mProcessingMessage);
            MOZ_ASSERT(self->mProcessingMessage->AsDecodeMessage());
            MOZ_ASSERT(self->mState == CodecState::Configured);
            MOZ_ASSERT(self->mAgent);
            MOZ_ASSERT(id == self->mAgent->mId);
            MOZ_ASSERT(self->mActiveConfig);

            DecodeMessage* msg = self->mProcessingMessage->AsDecodeMessage();
            LOGV("%s %p, DecoderAgent #%d %s has been %s",
                 DecoderType::Name.get(), self.get(), id, msg->ToString().get(),
                 aResult.IsResolve() ? "resolved" : "rejected");

            nsCString msgStr = msg->ToString();

            msg->Complete();
            self->mProcessingMessage.reset();

            if (aResult.IsReject()) {
              // The spec asks to queue a task to run close the decoder
              // with an EncodingError so we log the exact error here.
              const MediaResult& error = aResult.RejectValue();
              LOGE("%s %p, DecoderAgent #%d %s failed: %s",
                   DecoderType::Name.get(), self.get(), id, msgStr.get(),
                   error.Description().get());
              self->QueueATask(
                  "Error during decode runnable",
                  [self = RefPtr{self}]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                    MOZ_ASSERT(self->mState != CodecState::Closed);
                    self->CloseInternal(
                        NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                  });
              return;
            }

            MOZ_ASSERT(aResult.IsResolve());
            nsTArray<RefPtr<MediaData>> data =
                std::move(aResult.ResolveValue());
            if (data.IsEmpty()) {
              LOGV("%s %p got no data for %s", DecoderType::Name.get(),
                   self.get(), msgStr.get());
            } else {
              LOGV("%s %p, schedule %zu decoded data output for %s",
                   DecoderType::Name.get(), self.get(), data.Length(),
                   msgStr.get());
              self->QueueATask("Output Decoded Data",
                               [self = RefPtr{self}, data = std::move(data)]()
                                   MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                                     self->OutputDecodedData(std::move(data));
                                   });
            }
            self->ProcessControlMessageQueue();
          })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

template <typename DecoderType>
MessageProcessedResult DecoderTemplate<DecoderType>::ProcessFlushMessage(
    UniquePtr<ControlMessage>& aMessage) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(aMessage->AsFlushMessage());

  if (mProcessingMessage) {
    LOG("%s %p is processing %s. Defer %s", DecoderType::Name.get(), this,
        mProcessingMessage->ToString().get(), aMessage->ToString().get());
    return MessageProcessedResult::NotProcessed;
  }

  mProcessingMessage.reset(aMessage.release());
  mControlMessageQueue.pop();

  FlushMessage* msg = mProcessingMessage->AsFlushMessage();
  LOG("%s %p starts processing %s", DecoderType::Name.get(), this,
      msg->ToString().get());

  // No agent, no thing to do. The promise has been rejected with the
  // appropriate error in ResetInternal already.
  if (!mAgent) {
    LOGE("%s %p no agent, nothing to do", DecoderType::Name.get(), this);
    mProcessingMessage.reset();
    return MessageProcessedResult::Processed;
  }

  mAgent->DrainAndFlush()
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, id = mAgent->mId,
           this](DecoderAgent::DecodePromise::ResolveOrRejectValue&& aResult) {
            MOZ_ASSERT(self->mProcessingMessage);
            MOZ_ASSERT(self->mProcessingMessage->AsFlushMessage());
            MOZ_ASSERT(self->mState == CodecState::Configured);
            MOZ_ASSERT(self->mAgent);
            MOZ_ASSERT(id == self->mAgent->mId);
            MOZ_ASSERT(self->mActiveConfig);

            FlushMessage* msg = self->mProcessingMessage->AsFlushMessage();
            LOG("%s %p, DecoderAgent #%d %s has been %s",
                DecoderType::Name.get(), self.get(), id, msg->ToString().get(),
                aResult.IsResolve() ? "resolved" : "rejected");

            nsCString msgStr = msg->ToString();

            msg->Complete();

            // If flush failed, it means decoder fails to decode the data
            // sent before, so we treat it like decode error. We reject
            // the promise first and then queue a task to close
            // VideoDecoder with an EncodingError.
            if (aResult.IsReject()) {
              const MediaResult& error = aResult.RejectValue();
              LOGE("%s %p, DecoderAgent #%d failed to flush: %s",
                   DecoderType::Name.get(), self.get(), id,
                   error.Description().get());
              RefPtr<Promise> promise = msg->TakePromise();
              // Reject with an EncodingError instead of the error we got
              // above.
              self->QueueATask(
                  "Error during flush runnable",
                  [self = RefPtr{this}, promise]() MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                    promise->MaybeReject(
                        NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                    self->mProcessingMessage.reset();
                    MOZ_ASSERT(self->mState != CodecState::Closed);
                    self->CloseInternal(
                        NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR);
                  });
              return;
            }

            nsTArray<RefPtr<MediaData>> data =
                std::move(aResult.ResolveValue());

            if (data.IsEmpty()) {
              LOG("%s %p gets no data for %s", DecoderType::Name.get(),
                  self.get(), msgStr.get());
            } else {
              LOG("%s %p, schedule %zu decoded data output for %s",
                  DecoderType::Name.get(), self.get(), data.Length(),
                  msgStr.get());
            }

            RefPtr<Promise> promise = msg->TakePromise();
            self->QueueATask(
                "Flush: output decoding data task",
                [self = RefPtr{self}, promise, data = std::move(data)]()
                    MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                      self->OutputDecodedData(std::move(data));
                      promise->MaybeResolveWithUndefined();
                    });
            self->mProcessingMessage.reset();
            self->ProcessControlMessageQueue();
          })
      ->Track(msg->Request());

  return MessageProcessedResult::Processed;
}

// CreateDecoderAgent will create an DecoderAgent paired with a xpcom-shutdown
// blocker and a worker-reference. Besides the needs mentioned in the header
// file, the blocker and the worker-reference also provides an entry point for
// us to clean up the resources. Other than the decoder dtor, Reset(), or
// Close(), the resources should be cleaned up in the following situations:
// 1. Decoder on window, closing document
// 2. Decoder on worker, closing document
// 3. Decoder on worker, terminating worker
//
// In case 1, the entry point to clean up is in the mShutdownBlocker's
// ShutdownpPomise-resolver. In case 2, the entry point is in mWorkerRef's
// shutting down callback. In case 3, the entry point is in mWorkerRef's
// shutting down callback.

template <typename DecoderType>
bool DecoderTemplate<DecoderType>::CreateDecoderAgent(
    DecoderAgent::Id aId, UniquePtr<ConfigTypeInternal>&& aConfig,
    UniquePtr<TrackInfo>&& aInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == CodecState::Configured);
  MOZ_ASSERT(!mAgent);
  MOZ_ASSERT(!mActiveConfig);
  MOZ_ASSERT(!mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), !mWorkerRef);

  auto resetOnFailure = MakeScopeExit([&]() {
    mAgent = nullptr;
    mActiveConfig = nullptr;
    mShutdownBlocker = nullptr;
    mWorkerRef = nullptr;
  });

  // If the decoder is on worker, get a worker reference.
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (NS_WARN_IF(!workerPrivate)) {
      return false;
    }

    // Clean up all the resources when worker is going away.
    RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
        workerPrivate, "DecoderTemplate::CreateDecoderAgent",
        [self = RefPtr{this}]() {
          LOG("%s %p, worker is going away", DecoderType::Name.get(),
              self.get());
          Unused << self->ResetInternal(NS_ERROR_DOM_ABORT_ERR);
        });
    if (NS_WARN_IF(!workerRef)) {
      return false;
    }

    mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  }

  mAgent = MakeRefPtr<DecoderAgent>(aId, std::move(aInfo));
  mActiveConfig = std::move(aConfig);

  // ShutdownBlockingTicket requires an unique name to register its own
  // nsIAsyncShutdownBlocker since each blocker needs a distinct name.
  // To do that, we use DecoderAgent's unique id to create a unique name.
  nsAutoString uniqueName;
  uniqueName.AppendPrintf(
      "Blocker for DecoderAgent #%d (codec: %s) @ %p", mAgent->mId,
      NS_ConvertUTF16toUTF8(mActiveConfig->mCodec).get(), mAgent.get());

  mShutdownBlocker = media::ShutdownBlockingTicket::Create(
      uniqueName, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__);
  if (!mShutdownBlocker) {
    LOGE("%s %p failed to create %s", DecoderType::Name.get(), this,
         NS_ConvertUTF16toUTF8(uniqueName).get());
    return false;
  }

  // Clean up all the resources when xpcom-will-shutdown arrives since the page
  // is going to be closed.
  mShutdownBlocker->ShutdownPromise()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("%s %p gets xpcom-will-shutdown notification for DecoderAgent #%d",
            DecoderType::Name.get(), self.get(), id);
        Unused << self->ResetInternal(NS_ERROR_DOM_ABORT_ERR);
      },
      [self = RefPtr{this}, id = mAgent->mId,
       ref = mWorkerRef](bool /* aUnUsed*/) {
        LOG("%s %p removes shutdown-blocker #%d before getting any "
            "notification. DecoderAgent #%d should have been dropped",
            DecoderType::Name.get(), self.get(), id, id);
        MOZ_ASSERT(!self->mAgent || self->mAgent->mId != id);
      });

  LOG("%s %p creates DecoderAgent #%d @ %p and its shutdown-blocker",
      DecoderType::Name.get(), this, mAgent->mId, mAgent.get());

  resetOnFailure.release();
  return true;
}

template <typename DecoderType>
void DecoderTemplate<DecoderType>::DestroyDecoderAgentIfAny() {
  AssertIsOnOwningThread();

  if (!mAgent) {
    LOG("%s %p has no DecoderAgent to destroy", DecoderType::Name.get(), this);
    return;
  }

  MOZ_ASSERT(mActiveConfig);
  MOZ_ASSERT(mShutdownBlocker);
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerRef);

  LOG("%s %p destroys DecoderAgent #%d @ %p", DecoderType::Name.get(), this,
      mAgent->mId, mAgent.get());
  mActiveConfig = nullptr;
  RefPtr<DecoderAgent> agent = std::move(mAgent);
  // mShutdownBlocker should be kept alive until the shutdown is done.
  // mWorkerRef is used to ensure this task won't be discarded in worker.
  agent->Shutdown()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, id = agent->mId, ref = std::move(mWorkerRef),
       blocker = std::move(mShutdownBlocker)](
          const ShutdownPromise::ResolveOrRejectValue& aResult) {
        LOG("%s %p, DecoderAgent #%d's shutdown has been %s. Drop its "
            "shutdown-blocker now",
            DecoderType::Name.get(), self.get(), id,
            aResult.IsResolve() ? "resolved" : "rejected");
      });
}

template class DecoderTemplate<VideoDecoderTraits>;

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla::dom
