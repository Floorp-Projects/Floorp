/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesisService.h"

#include <android/log.h>

#include "nsXULAppAPI.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/jni/Utils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_media.h"

#define ALOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "GeckoSpeechSynthesis", ##args)

namespace mozilla {
namespace dom {

StaticRefPtr<SpeechSynthesisService> SpeechSynthesisService::sSingleton;

class AndroidSpeechCallback final : public nsISpeechTaskCallback {
 public:
  AndroidSpeechCallback() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD OnResume() override { return NS_OK; }

  NS_IMETHOD OnPause() override { return NS_OK; }

  NS_IMETHOD OnCancel() override {
    java::SpeechSynthesisService::Stop();
    return NS_OK;
  }

  NS_IMETHOD OnVolumeChanged(float aVolume) override { return NS_OK; }

 private:
  ~AndroidSpeechCallback() {}
};

NS_IMPL_ISUPPORTS(AndroidSpeechCallback, nsISpeechTaskCallback)

NS_IMPL_ISUPPORTS(SpeechSynthesisService, nsISpeechService)

void SpeechSynthesisService::Setup() {
  ALOG("SpeechSynthesisService::Setup");

  if (!StaticPrefs::media_webspeech_synth_enabled() ||
      Preferences::GetBool("media.webspeech.synth.test")) {
    return;
  }

  if (!jni::IsAvailable()) {
    NS_WARNING("Failed to initialize speech synthesis");
    return;
  }

  Init();
  java::SpeechSynthesisService::InitSynth();
}

// nsISpeechService

NS_IMETHODIMP
SpeechSynthesisService::Speak(const nsAString& aText, const nsAString& aUri,
                              float aVolume, float aRate, float aPitch,
                              nsISpeechTask* aTask) {
  if (mTask) {
    NS_WARNING("Service only supports one speech task at a time.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<AndroidSpeechCallback> callback = new AndroidSpeechCallback();
  nsresult rv = aTask->Setup(callback);

  if (NS_FAILED(rv)) {
    return rv;
  }

  jni::String::LocalRef utteranceId =
      java::SpeechSynthesisService::Speak(aUri, aText, aRate, aPitch, aVolume);
  if (!utteranceId) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mTaskUtteranceId = utteranceId->ToCString();
  mTask = aTask;
  mTaskTextLength = aText.Length();
  mTaskTextOffset = 0;

  return NS_OK;
}

SpeechSynthesisService* SpeechSynthesisService::GetInstance(bool aCreate) {
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    MOZ_ASSERT(
        false,
        "SpeechSynthesisService can only be started on main gecko process");
    return nullptr;
  }

  if (!sSingleton && aCreate) {
    sSingleton = new SpeechSynthesisService();
    sSingleton->Setup();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

already_AddRefed<SpeechSynthesisService>
SpeechSynthesisService::GetInstanceForService() {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<SpeechSynthesisService> sapiService = GetInstance();
  return sapiService.forget();
}

// JNI

void SpeechSynthesisService::RegisterVoice(jni::String::Param aUri,
                                           jni::String::Param aName,
                                           jni::String::Param aLocale,
                                           bool aIsNetwork, bool aIsDefault) {
  nsSynthVoiceRegistry* registry = nsSynthVoiceRegistry::GetInstance();
  SpeechSynthesisService* service = SpeechSynthesisService::GetInstance(false);
  // This service can only speak one utterance at a time, so we set
  // aQueuesUtterances to true in order to track global state and schedule
  // access to this service.
  DebugOnly<nsresult> rv =
      registry->AddVoice(service, aUri->ToString(), aName->ToString(),
                         aLocale->ToString(), !aIsNetwork, true);

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to add voice");

  if (aIsDefault) {
    DebugOnly<nsresult> rv = registry->SetDefaultVoice(aUri->ToString(), true);

    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to set voice as default");
  }
}

void SpeechSynthesisService::DoneRegisteringVoices() {
  nsSynthVoiceRegistry* registry = nsSynthVoiceRegistry::GetInstance();
  registry->NotifyVoicesChanged();
}

void SpeechSynthesisService::DispatchStart(jni::String::Param aUtteranceId) {
  if (sSingleton) {
    MOZ_ASSERT(sSingleton->mTaskUtteranceId.Equals(aUtteranceId->ToCString()));
    nsCOMPtr<nsISpeechTask> task = sSingleton->mTask;
    if (task) {
      sSingleton->mTaskStartTime = TimeStamp::Now();
      DebugOnly<nsresult> rv = task->DispatchStart();
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to dispatch start");
    }
  }
}

void SpeechSynthesisService::DispatchEnd(jni::String::Param aUtteranceId) {
  if (sSingleton) {
    // In API older than 23, we will sometimes call this function
    // without providing an utterance ID.
    MOZ_ASSERT(!aUtteranceId ||
               sSingleton->mTaskUtteranceId.Equals(aUtteranceId->ToCString()));
    nsCOMPtr<nsISpeechTask> task = sSingleton->mTask;
    sSingleton->mTask = nullptr;
    if (task) {
      TimeStamp startTime = sSingleton->mTaskStartTime;
      DebugOnly<nsresult> rv =
          task->DispatchEnd((TimeStamp::Now() - startTime).ToSeconds(),
                            sSingleton->mTaskTextLength);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to dispatch start");
    }
  }
}

void SpeechSynthesisService::DispatchError(jni::String::Param aUtteranceId) {
  if (sSingleton) {
    MOZ_ASSERT(sSingleton->mTaskUtteranceId.Equals(aUtteranceId->ToCString()));
    nsCOMPtr<nsISpeechTask> task = sSingleton->mTask;
    sSingleton->mTask = nullptr;
    if (task) {
      TimeStamp startTime = sSingleton->mTaskStartTime;
      DebugOnly<nsresult> rv =
          task->DispatchError((TimeStamp::Now() - startTime).ToSeconds(),
                              sSingleton->mTaskTextOffset);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to dispatch start");
    }
  }
}

void SpeechSynthesisService::DispatchBoundary(jni::String::Param aUtteranceId,
                                              int32_t aStart, int32_t aEnd) {
  if (sSingleton) {
    MOZ_ASSERT(sSingleton->mTaskUtteranceId.Equals(aUtteranceId->ToCString()));
    nsCOMPtr<nsISpeechTask> task = sSingleton->mTask;
    if (task) {
      TimeStamp startTime = sSingleton->mTaskStartTime;
      sSingleton->mTaskTextOffset = aStart;
      DebugOnly<nsresult> rv = task->DispatchBoundary(
          u"word"_ns, (TimeStamp::Now() - startTime).ToSeconds(), aStart,
          aEnd - aStart, 1);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to dispatch boundary");
    }
  }
}

}  // namespace dom
}  // namespace mozilla
