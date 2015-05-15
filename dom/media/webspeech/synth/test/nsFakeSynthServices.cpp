/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsFakeSynthServices.h"
#include "nsPrintfCString.h"
#include "nsIWeakReferenceUtils.h"
#include "SharedBuffer.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/dom/nsSpeechTask.h"

#include "nsThreadUtils.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"

#define CHANNELS 1
#define SAMPLERATE 1600

namespace mozilla {
namespace dom {

StaticRefPtr<nsFakeSynthServices> nsFakeSynthServices::sSingleton;

enum VoiceFlags
{
  eSuppressEvents = 1,
  eSuppressEnd = 2
};

struct VoiceDetails
{
  const char* uri;
  const char* name;
  const char* lang;
  bool defaultVoice;
  uint32_t flags;
};

static const VoiceDetails sDirectVoices[] = {
  {"urn:moz-tts:fake-direct:bob", "Bob Marley", "en-JM", true, 0},
  {"urn:moz-tts:fake-direct:amy", "Amy Winehouse", "en-GB", false, 0},
  {"urn:moz-tts:fake-direct:lenny", "Leonard Cohen", "en-CA", false, 0},
  {"urn:moz-tts:fake-direct:celine", "Celine Dion", "fr-CA", false, 0},
  {"urn:moz-tts:fake-direct:julie", "Julieta Venegas", "es-MX", false, },
};

static const VoiceDetails sIndirectVoices[] = {
  {"urn:moz-tts:fake-indirect:zanetta", "Zanetta Farussi", "it-IT", false, 0},
  {"urn:moz-tts:fake-indirect:margherita", "Margherita Durastanti", "it-IT-noevents-noend", false, eSuppressEvents | eSuppressEnd},
  {"urn:moz-tts:fake-indirect:teresa", "Teresa Cornelys", "it-IT-noend", false, eSuppressEnd},
};

// FakeSynthCallback
class FakeSynthCallback : public nsISpeechTaskCallback
{
public:
  explicit FakeSynthCallback(nsISpeechTask* aTask) : mTask(aTask) { }
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(FakeSynthCallback, nsISpeechTaskCallback)

  NS_IMETHOD OnPause()
  {
    if (mTask) {
      mTask->DispatchPause(1.5, 1);
    }

    return NS_OK;
  }

  NS_IMETHOD OnResume()
  {
    if (mTask) {
      mTask->DispatchResume(1.5, 1);
    }

    return NS_OK;
  }

  NS_IMETHOD OnCancel()
  {
    if (mTask) {
      mTask->DispatchEnd(1.5, 1);
    }

    return NS_OK;
  }

private:
  virtual ~FakeSynthCallback() { }

  nsCOMPtr<nsISpeechTask> mTask;
};

NS_IMPL_CYCLE_COLLECTION(FakeSynthCallback, mTask);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FakeSynthCallback)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTaskCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTaskCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FakeSynthCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FakeSynthCallback)

// FakeDirectAudioSynth

class FakeDirectAudioSynth : public nsISpeechService
{

public:
  FakeDirectAudioSynth() { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE

private:
  virtual ~FakeDirectAudioSynth() { }
};

NS_IMPL_ISUPPORTS(FakeDirectAudioSynth, nsISpeechService)

NS_IMETHODIMP
FakeDirectAudioSynth::Speak(const nsAString& aText, const nsAString& aUri,
                            float aVolume, float aRate, float aPitch,
                            nsISpeechTask* aTask)
{
  class Runnable final : public nsRunnable
  {
  public:
    Runnable(nsISpeechTask* aTask, const nsAString& aText) :
      mTask(aTask), mText(aText)
    {
    }

    NS_IMETHOD Run() override
    {
      nsRefPtr<FakeSynthCallback> cb = new FakeSynthCallback(nullptr);
      mTask->Setup(cb, CHANNELS, SAMPLERATE, 2);

      // Just an arbitrary multiplier. Pretend that each character is
      // synthesized to 40 frames.
      uint32_t frames_length = 40 * mText.Length();
      nsAutoArrayPtr<int16_t> frames(new int16_t[frames_length]());
      mTask->SendAudioNative(frames, frames_length);

      mTask->SendAudioNative(nullptr, 0);

      return NS_OK;
    }

  private:
    nsCOMPtr<nsISpeechTask> mTask;
    nsString mText;
  };

  nsCOMPtr<nsIRunnable> runnable = new Runnable(aTask, aText);
  NS_DispatchToMainThread(runnable);
  return NS_OK;
}

NS_IMETHODIMP
FakeDirectAudioSynth::GetServiceType(SpeechServiceType* aServiceType)
{
  *aServiceType = nsISpeechService::SERVICETYPE_DIRECT_AUDIO;
  return NS_OK;
}

// FakeDirectAudioSynth

class FakeIndirectAudioSynth : public nsISpeechService
{

public:
  FakeIndirectAudioSynth() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE

private:
  virtual ~FakeIndirectAudioSynth() { }
};

NS_IMPL_ISUPPORTS(FakeIndirectAudioSynth, nsISpeechService)

NS_IMETHODIMP
FakeIndirectAudioSynth::Speak(const nsAString& aText, const nsAString& aUri,
                              float aVolume, float aRate, float aPitch,
                              nsISpeechTask* aTask)
{
  class DispatchStart final : public nsRunnable
  {
  public:
    explicit DispatchStart(nsISpeechTask* aTask) :
      mTask(aTask)
    {
    }

    NS_IMETHOD Run() override
    {
      mTask->DispatchStart();

      return NS_OK;
    }

  private:
    nsCOMPtr<nsISpeechTask> mTask;
  };

  class DispatchEnd final : public nsRunnable
  {
  public:
    DispatchEnd(nsISpeechTask* aTask, const nsAString& aText) :
      mTask(aTask), mText(aText)
    {
    }

    NS_IMETHOD Run() override
    {
      mTask->DispatchEnd(mText.Length()/2, mText.Length());

      return NS_OK;
    }

  private:
    nsCOMPtr<nsISpeechTask> mTask;
    nsString mText;
  };

  uint32_t flags = 0;
  for (uint32_t i = 0; i < ArrayLength(sIndirectVoices); i++) {
    if (aUri.EqualsASCII(sIndirectVoices[i].uri)) {
      flags = sIndirectVoices[i].flags;
    }
  }

  nsRefPtr<FakeSynthCallback> cb = new FakeSynthCallback(
    (flags & eSuppressEvents) ? nullptr : aTask);

  aTask->Setup(cb, 0, 0, 0);

  nsCOMPtr<nsIRunnable> runnable = new DispatchStart(aTask);
  NS_DispatchToMainThread(runnable);

  if ((flags & eSuppressEnd) == 0) {
    runnable = new DispatchEnd(aTask, aText);
    NS_DispatchToMainThread(runnable);
  }

  return NS_OK;
}

NS_IMETHODIMP
FakeIndirectAudioSynth::GetServiceType(SpeechServiceType* aServiceType)
{
  *aServiceType = nsISpeechService::SERVICETYPE_INDIRECT_AUDIO;
  return NS_OK;
}

// nsFakeSynthService

NS_INTERFACE_MAP_BEGIN(nsFakeSynthServices)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsFakeSynthServices)
NS_IMPL_RELEASE(nsFakeSynthServices)

nsFakeSynthServices::nsFakeSynthServices()
{
}

nsFakeSynthServices::~nsFakeSynthServices()
{
}

static void
AddVoices(nsISpeechService* aService, const VoiceDetails* aVoices, uint32_t aLength)
{
  nsSynthVoiceRegistry* registry = nsSynthVoiceRegistry::GetInstance();
  for (uint32_t i = 0; i < aLength; i++) {
    NS_ConvertUTF8toUTF16 name(aVoices[i].name);
    NS_ConvertUTF8toUTF16 uri(aVoices[i].uri);
    NS_ConvertUTF8toUTF16 lang(aVoices[i].lang);
    registry->AddVoice(aService, uri, name, lang, true);
    if (aVoices[i].defaultVoice) {
      registry->SetDefaultVoice(uri, true);
    }
  }
}

void
nsFakeSynthServices::Init()
{
  mDirectService = new FakeDirectAudioSynth();
  AddVoices(mDirectService, sDirectVoices, ArrayLength(sDirectVoices));

  mIndirectService = new FakeIndirectAudioSynth();
  AddVoices(mIndirectService, sIndirectVoices, ArrayLength(sIndirectVoices));
}

// nsIObserver

NS_IMETHODIMP
nsFakeSynthServices::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE(!strcmp(aTopic, "profile-after-change"), NS_ERROR_UNEXPECTED);

  if (Preferences::GetBool("media.webspeech.synth.test")) {
    Init();
  }

  return NS_OK;
}

// static methods

nsFakeSynthServices*
nsFakeSynthServices::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    MOZ_ASSERT(false, "nsFakeSynthServices can only be started on main gecko process");
    return nullptr;
  }

  if (!sSingleton) {
    sSingleton = new nsFakeSynthServices();
  }

  return sSingleton;
}

already_AddRefed<nsFakeSynthServices>
nsFakeSynthServices::GetInstanceForService()
{
  nsRefPtr<nsFakeSynthServices> picoService = GetInstance();
  return picoService.forget();
}

void
nsFakeSynthServices::Shutdown()
{
  if (!sSingleton) {
    return;
  }

  sSingleton = nullptr;
}

} // namespace dom
} // namespace mozilla
