/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncoderAgent.h"

#include "PDMFactory.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Logging.h"
#include "nsThreadUtils.h"

extern mozilla::LazyLogModule gWebCodecsLog;

namespace mozilla {

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
#endif  // LOGE
#define LOGW(msg, ...) LOG_INTERNAL(Warning, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

#ifdef LOGV
#  undef LOGV
#endif  // LOGV
#define LOGV(msg, ...) LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

EncoderAgent::EncoderAgent(WebCodecsId aId)
    : mId(aId),
      mOwnerThread(GetCurrentSerialEventTarget()),
      mPEMFactory(MakeRefPtr<PEMFactory>()),
      mEncoder(nullptr),
      mState(State::Unconfigured) {
  MOZ_ASSERT(mOwnerThread);
  MOZ_ASSERT(mPEMFactory);
  LOG("EncoderAgent #%zu (%p) ctor", mId, this);
}

EncoderAgent::~EncoderAgent() {
  LOG("EncoderAgent #%zu (%p) dtor", mId, this);
  MOZ_ASSERT(mState == State::Unconfigured, "encoder released in wrong state");
  MOZ_ASSERT(!mEncoder, "encoder must be shutdown");
}

RefPtr<EncoderAgent::ConfigurePromise> EncoderAgent::Configure(
    const EncoderConfig& aConfig) {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());
  MOZ_ASSERT(mState == State::Unconfigured || mState == State::Error);
  MOZ_ASSERT(mConfigurePromise.IsEmpty());
  MOZ_ASSERT(!mCreateRequest.Exists());
  MOZ_ASSERT(!mInitRequest.Exists());

  if (mState == State::Error) {
    LOGE("EncoderAgent #%zu (%p) tried to configure in error state", mId, this);
    return ConfigurePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    "Cannot configure in error state"),
        __func__);
  }

  MOZ_ASSERT(mState == State::Unconfigured);
  MOZ_ASSERT(!mEncoder);
  SetState(State::Configuring);

  LOG("EncoderAgent #%zu (%p) is creating an encoder (%s)", mId, this,
      GetCodecTypeString(aConfig.mCodec));

  RefPtr<ConfigurePromise> p = mConfigurePromise.Ensure(__func__);

  mPEMFactory->CreateEncoderAsync(aConfig, dom::GetWebCodecsEncoderTaskQueue())
      ->Then(
          mOwnerThread, __func__,
          [self = RefPtr{this}](RefPtr<MediaDataEncoder>&& aEncoder) {
            self->mCreateRequest.Complete();

            // If EncoderAgent has been shut down, shut the created encoder down
            // and return.
            if (!self->mShutdownWhileCreationPromise.IsEmpty()) {
              MOZ_ASSERT(self->mState == State::ShuttingDown);
              MOZ_ASSERT(self->mConfigurePromise.IsEmpty(),
                         "configuration should have been rejected");

              LOGW(
                  "EncoderAgent #%zu (%p) has been shut down. We need to shut "
                  "the newly created encoder down",
                  self->mId, self.get());
              aEncoder->Shutdown()->Then(
                  self->mOwnerThread, __func__,
                  [self](const ShutdownPromise::ResolveOrRejectValue& aValue) {
                    MOZ_ASSERT(self->mState == State::ShuttingDown);

                    LOGW(
                        "EncoderAgent #%zu (%p), newly created encoder "
                        "shutdown "
                        "has been %s",
                        self->mId, self.get(),
                        aValue.IsResolve() ? "resolved" : "rejected");

                    self->SetState(State::Unconfigured);

                    self->mShutdownWhileCreationPromise.ResolveOrReject(
                        aValue, __func__);
                  });
              return;
            }

            self->mEncoder = aEncoder.forget();
            LOG("EncoderAgent #%zu (%p) has created a encoder, now initialize "
                "it",
                self->mId, self.get());
            self->mEncoder->Init()
                ->Then(
                    self->mOwnerThread, __func__,
                    [self]() {
                      self->mInitRequest.Complete();
                      LOG("EncoderAgent #%zu (%p) has initialized the encoder",
                          self->mId, self.get());
                      self->SetState(State::Configured);
                      self->mConfigurePromise.Resolve(true, __func__);
                    },
                    [self](const MediaResult& aError) {
                      self->mInitRequest.Complete();
                      LOGE(
                          "EncoderAgent #%zu (%p) failed to initialize the "
                          "encoder",
                          self->mId, self.get());
                      self->SetState(State::Error);
                      self->mConfigurePromise.Reject(aError, __func__);
                    })
                ->Track(self->mInitRequest);
          },
          [self = RefPtr{this}](const MediaResult& aError) {
            self->mCreateRequest.Complete();
            LOGE("EncoderAgent #%zu (%p) failed to create a encoder", self->mId,
                 self.get());

            // If EncoderAgent has been shut down, we need to resolve the
            // shutdown promise.
            if (!self->mShutdownWhileCreationPromise.IsEmpty()) {
              MOZ_ASSERT(self->mState == State::ShuttingDown);
              MOZ_ASSERT(self->mConfigurePromise.IsEmpty(),
                         "configuration should have been rejected");

              LOGW(
                  "EncoderAgent #%zu (%p) has been shut down. Resolve the "
                  "shutdown promise right away since encoder creation failed",
                  self->mId, self.get());

              self->SetState(State::Unconfigured);
              self->mShutdownWhileCreationPromise.Resolve(true, __func__);
              return;
            }

            self->SetState(State::Error);
            self->mConfigurePromise.Reject(aError, __func__);
          })
      ->Track(mCreateRequest);

  return p;
}

RefPtr<EncoderAgent::ReconfigurationPromise> EncoderAgent::Reconfigure(
    const RefPtr<const EncoderConfigurationChangeList>& aConfigChanges) {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());
  MOZ_ASSERT(mState == State::Configured || mState == State::Error);
  MOZ_ASSERT(mReconfigurationPromise.IsEmpty());

  if (mState == State::Error) {
    LOGE("EncoderAgent #%zu (%p) tried to reconfigure in error state", mId,
         this);
    return ReconfigurationPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    "Cannot reconfigure in error state"),
        __func__);
  }

  MOZ_ASSERT(mEncoder);
  SetState(State::Configuring);

  LOG("EncoderAgent #%zu (%p) is reconfiguring its encoder (%s)", mId, this,
      NS_ConvertUTF16toUTF8(aConfigChanges->ToString().get()).get());

  RefPtr<ReconfigurationPromise> p = mReconfigurationPromise.Ensure(__func__);

  mEncoder->Reconfigure(aConfigChanges)
      ->Then(
          mOwnerThread, __func__,
          [self = RefPtr{this}](bool) {
            self->mReconfigurationRequest.Complete();
            LOGE("EncoderAgent #%zu (%p) reconfigure success", self->mId,
                 self.get());
            self->mReconfigurationPromise.Resolve(true, __func__);
          },
          [self = RefPtr{this}](const MediaResult& aError) {
            self->mReconfigurationRequest.Complete();
            LOGE("EncoderAgent #%zu (%p) reconfigure failure", self->mId,
                 self.get());
            // Not a a fatal error per se, the owner will deal with it.
            self->mReconfigurationPromise.Reject(aError, __func__);
          })
      ->Track(mReconfigurationRequest);

  return p;
}

RefPtr<ShutdownPromise> EncoderAgent::Shutdown() {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());

  auto r =
      MediaResult(NS_ERROR_DOM_MEDIA_CANCELED, "Canceled by encoder shutdown");

  // If the encoder creation has not been completed yet, wait until the encoder
  // being created has been shut down.
  if (mCreateRequest.Exists()) {
    MOZ_ASSERT(!mInitRequest.Exists());
    MOZ_ASSERT(!mConfigurePromise.IsEmpty());
    MOZ_ASSERT(!mEncoder);
    MOZ_ASSERT(mState == State::Configuring);
    MOZ_ASSERT(mShutdownWhileCreationPromise.IsEmpty());

    LOGW(
        "EncoderAgent #%zu (%p) shutdown while the encoder creation for "
        "configuration is in flight. Reject the configuration now and defer "
        "the shutdown until the created encoder has been shut down",
        mId, this);

    // Reject the configuration in flight.
    mConfigurePromise.Reject(r, __func__);

    // Get the promise that will be resolved when the encoder being created has
    // been destroyed.
    SetState(State::ShuttingDown);
    return mShutdownWhileCreationPromise.Ensure(__func__);
  }

  // If encoder creation has been completed, we must have the encoder now.
  MOZ_ASSERT(mEncoder);

  // Cancel pending initialization for configuration in flight if any.
  mInitRequest.DisconnectIfExists();
  mConfigurePromise.RejectIfExists(r, __func__);

  mReconfigurationRequest.DisconnectIfExists();
  mReconfigurationPromise.RejectIfExists(r, __func__);

  // Cancel encoder in flight if any.
  mEncodeRequest.DisconnectIfExists();
  mEncodePromise.RejectIfExists(r, __func__);

  // Cancel flush-out in flight if any.
  mDrainRequest.DisconnectIfExists();
  mEncodeRequest.DisconnectIfExists();

  mDrainRequest.DisconnectIfExists();
  mDrainPromise.RejectIfExists(r, __func__);

  SetState(State::Unconfigured);

  RefPtr<MediaDataEncoder> encoder = std::move(mEncoder);
  return encoder->Shutdown();
}

RefPtr<EncoderAgent::EncodePromise> EncoderAgent::Encode(MediaData* aInput) {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());
  MOZ_ASSERT(aInput);
  MOZ_ASSERT(mState == State::Configured || mState == State::Error);
  MOZ_ASSERT(mEncodePromise.IsEmpty());
  MOZ_ASSERT(!mEncodeRequest.Exists());

  if (mState == State::Error) {
    LOGE("EncoderAgent #%zu (%p) tried to encoder in error state", mId, this);
    return EncodePromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    "Cannot encoder in error state"),
        __func__);
  }

  MOZ_ASSERT(mState == State::Configured);
  MOZ_ASSERT(mEncoder);
  SetState(State::Encoding);

  RefPtr<EncodePromise> p = mEncodePromise.Ensure(__func__);

  mEncoder->Encode(aInput)
      ->Then(
          mOwnerThread, __func__,
          [self = RefPtr{this}](MediaDataEncoder::EncodedData&& aData) {
            self->mEncodeRequest.Complete();
            LOGV("EncoderAgent #%zu (%p) encode successful", self->mId,
                 self.get());
            self->SetState(State::Configured);
            self->mEncodePromise.Resolve(std::move(aData), __func__);
          },
          [self = RefPtr{this}](const MediaResult& aError) {
            self->mEncodeRequest.Complete();
            LOGV("EncoderAgent #%zu (%p) failed to encode", self->mId,
                 self.get());
            self->SetState(State::Error);
            self->mEncodePromise.Reject(aError, __func__);
          })
      ->Track(mEncodeRequest);

  return p;
}

RefPtr<EncoderAgent::EncodePromise> EncoderAgent::Drain() {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());
  // This can be called when reconfiguring the encoder.
  MOZ_ASSERT(mState == State::Configured || mState == State::Configuring);
  MOZ_ASSERT(mDrainPromise.IsEmpty());
  MOZ_ASSERT(mEncoder);

  SetState(State::Flushing);

  RefPtr<EncodePromise> p = mDrainPromise.Ensure(__func__);
  DryUntilDrain();
  return p;
}

void EncoderAgent::DryUntilDrain() {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());
  MOZ_ASSERT(mState == State::Flushing);
  MOZ_ASSERT(!mDrainRequest.Exists());
  MOZ_ASSERT(mEncoder);

  LOG("EncoderAgent #%zu (%p) is draining the encoder", mId, this);
  mEncoder->Drain()
      ->Then(
          mOwnerThread, __func__,
          [self = RefPtr{this}](MediaDataEncoder::EncodedData&& aData) {
            self->mDrainRequest.Complete();

            if (aData.IsEmpty()) {
              LOG("EncoderAgent #%zu (%p) is dry now", self->mId, self.get());
              self->SetState(State::Configured);
              self->mDrainPromise.Resolve(std::move(self->mDrainData),
                                          __func__);
              return;
            }

            LOG("EncoderAgent #%zu (%p) drained %zu encoder data. Keep "
                "draining "
                "until dry",
                self->mId, self.get(), aData.Length());
            self->mDrainData.AppendElements(std::move(aData));
            self->DryUntilDrain();
          },
          [self = RefPtr{this}](const MediaResult& aError) {
            self->mDrainRequest.Complete();

            LOGE("EncoderAgent %p failed to drain encoder", self.get());
            self->mDrainData.Clear();
            self->mDrainPromise.Reject(aError, __func__);
          })
      ->Track(mDrainRequest);
}

void EncoderAgent::SetState(State aState) {
  MOZ_ASSERT(mOwnerThread->IsOnCurrentThread());

  auto validateStateTransition = [](State aOldState, State aNewState) {
    switch (aOldState) {
      case State::Unconfigured:
        return aNewState == State::Configuring;
      case State::Configuring:
        return aNewState == State::Configured || aNewState == State::Error ||
               aNewState == State::Flushing ||
               aNewState == State::Unconfigured ||
               aNewState == State::ShuttingDown;
      case State::Configured:
        return aNewState == State::Unconfigured ||
               aNewState == State::Configuring ||
               aNewState == State::Encoding || aNewState == State::Flushing;
      case State::Encoding:
      case State::Flushing:
        return aNewState == State::Configured || aNewState == State::Error ||
               aNewState == State::Unconfigured;
      case State::ShuttingDown:
        return aNewState == State::Unconfigured;
      case State::Error:
        return aNewState == State::Unconfigured;
      default:
        break;
    }
    MOZ_ASSERT_UNREACHABLE("Unhandled state transition");
    return false;
  };

  auto stateToString = [](State aState) -> const char* {
    switch (aState) {
      case State::Unconfigured:
        return "Unconfigured";
      case State::Configuring:
        return "Configuring";
      case State::Configured:
        return "Configured";
      case State::Encoding:
        return "Encoding";
      case State::Flushing:
        return "Flushing";
      case State::ShuttingDown:
        return "ShuttingDown";
      case State::Error:
        return "Error";
      default:
        break;
    }
    MOZ_ASSERT_UNREACHABLE("Unhandled state type");
    return "Unknown";
  };

  DebugOnly<bool> isValid = validateStateTransition(mState, aState);
  LOGV("EncoderAgent #%zu (%p) state change: %s -> %s", mId, this,
       stateToString(mState), stateToString(aState));
  MOZ_ASSERT(isValid);
  mState = aState;
}

#undef LOG
#undef LOGW
#undef LOGE
#undef LOGV
#undef LOG_INTERNAL

}  // namespace mozilla
