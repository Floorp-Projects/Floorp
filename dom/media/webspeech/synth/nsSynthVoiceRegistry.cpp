/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsILocaleService.h"
#include "nsISpeechService.h"
#include "nsServiceManagerUtils.h"
#include "nsCategoryManagerUtils.h"

#include "MediaPrefs.h"
#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"
#include "nsSynthVoiceRegistry.h"
#include "nsSpeechTask.h"
#include "AudioChannelService.h"

#include "nsString.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/unused.h"

#include "SpeechSynthesisChild.h"
#include "SpeechSynthesisParent.h"

#undef LOG
extern mozilla::LogModule* GetSpeechSynthLog();
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

namespace {

void
GetAllSpeechSynthActors(InfallibleTArray<mozilla::dom::SpeechSynthesisParent*>& aActors)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aActors.IsEmpty());

  AutoTArray<mozilla::dom::ContentParent*, 20> contentActors;
  mozilla::dom::ContentParent::GetAll(contentActors);

  for (uint32_t contentIndex = 0;
       contentIndex < contentActors.Length();
       ++contentIndex) {
    MOZ_ASSERT(contentActors[contentIndex]);

    AutoTArray<mozilla::dom::PSpeechSynthesisParent*, 5> speechsynthActors;
    contentActors[contentIndex]->ManagedPSpeechSynthesisParent(speechsynthActors);

    for (uint32_t speechsynthIndex = 0;
         speechsynthIndex < speechsynthActors.Length();
         ++speechsynthIndex) {
      MOZ_ASSERT(speechsynthActors[speechsynthIndex]);

      mozilla::dom::SpeechSynthesisParent* actor =
        static_cast<mozilla::dom::SpeechSynthesisParent*>(speechsynthActors[speechsynthIndex]);
      aActors.AppendElement(actor);
    }
  }
}

} // namespace

namespace mozilla {
namespace dom {

// VoiceData

class VoiceData final
{
private:
  // Private destructor, to discourage deletion outside of Release():
  ~VoiceData() {}

public:
  VoiceData(nsISpeechService* aService, const nsAString& aUri,
            const nsAString& aName, const nsAString& aLang,
            bool aIsLocal, bool aQueuesUtterances)
    : mService(aService)
    , mUri(aUri)
    , mName(aName)
    , mLang(aLang)
    , mIsLocal(aIsLocal)
    , mIsQueued(aQueuesUtterances) {}

  NS_INLINE_DECL_REFCOUNTING(VoiceData)

  nsCOMPtr<nsISpeechService> mService;

  nsString mUri;

  nsString mName;

  nsString mLang;

  bool mIsLocal;

  bool mIsQueued;
};

// GlobalQueueItem

class GlobalQueueItem final
{
private:
  // Private destructor, to discourage deletion outside of Release():
  ~GlobalQueueItem() {}

public:
  GlobalQueueItem(VoiceData* aVoice, nsSpeechTask* aTask, const nsAString& aText,
                  const float& aVolume, const float& aRate, const float& aPitch)
    : mVoice(aVoice)
    , mTask(aTask)
    , mText(aText)
    , mVolume(aVolume)
    , mRate(aRate)
    , mPitch(aPitch) {}

  NS_INLINE_DECL_REFCOUNTING(GlobalQueueItem)

  RefPtr<VoiceData> mVoice;

  RefPtr<nsSpeechTask> mTask;

  nsString mText;

  float mVolume;

  float mRate;

  float mPitch;

  bool mIsLocal;
};

// nsSynthVoiceRegistry

static StaticRefPtr<nsSynthVoiceRegistry> gSynthVoiceRegistry;

NS_IMPL_ISUPPORTS(nsSynthVoiceRegistry, nsISynthVoiceRegistry)

nsSynthVoiceRegistry::nsSynthVoiceRegistry()
  : mSpeechSynthChild(nullptr)
  , mUseGlobalQueue(false)
  , mIsSpeaking(false)
{
  if (XRE_IsContentProcess()) {

    mSpeechSynthChild = new SpeechSynthesisChild();
    ContentChild::GetSingleton()->SendPSpeechSynthesisConstructor(mSpeechSynthChild);

    InfallibleTArray<RemoteVoice> voices;
    InfallibleTArray<nsString> defaults;
    bool isSpeaking;

    mSpeechSynthChild->SendReadVoicesAndState(&voices, &defaults, &isSpeaking);

    for (uint32_t i = 0; i < voices.Length(); ++i) {
      RemoteVoice voice = voices[i];
      AddVoiceImpl(nullptr, voice.voiceURI(),
                   voice.name(), voice.lang(),
                   voice.localService(), voice.queued());
    }

    for (uint32_t i = 0; i < defaults.Length(); ++i) {
      SetDefaultVoice(defaults[i], true);
    }

    mIsSpeaking = isSpeaking;
  }
}

nsSynthVoiceRegistry::~nsSynthVoiceRegistry()
{
  LOG(LogLevel::Debug, ("~nsSynthVoiceRegistry"));

  // mSpeechSynthChild's lifecycle is managed by the Content protocol.
  mSpeechSynthChild = nullptr;

  mUriVoiceMap.Clear();
}

nsSynthVoiceRegistry*
nsSynthVoiceRegistry::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSynthVoiceRegistry) {
    gSynthVoiceRegistry = new nsSynthVoiceRegistry();
    if (XRE_IsParentProcess()) {
      // Start up all speech synth services.
      NS_CreateServicesFromCategory(NS_SPEECH_SYNTH_STARTED, nullptr,
        NS_SPEECH_SYNTH_STARTED);
    }
  }

  return gSynthVoiceRegistry;
}

already_AddRefed<nsSynthVoiceRegistry>
nsSynthVoiceRegistry::GetInstanceForService()
{
  RefPtr<nsSynthVoiceRegistry> registry = GetInstance();

  return registry.forget();
}

void
nsSynthVoiceRegistry::Shutdown()
{
  LOG(LogLevel::Debug, ("[%s] nsSynthVoiceRegistry::Shutdown()",
                        (XRE_IsContentProcess()) ? "Content" : "Default"));
  gSynthVoiceRegistry = nullptr;
}

void
nsSynthVoiceRegistry::SendVoicesAndState(InfallibleTArray<RemoteVoice>* aVoices,
                                         InfallibleTArray<nsString>* aDefaults,
                                         bool* aIsSpeaking)
{
  for (uint32_t i=0; i < mVoices.Length(); ++i) {
    RefPtr<VoiceData> voice = mVoices[i];

    aVoices->AppendElement(RemoteVoice(voice->mUri, voice->mName, voice->mLang,
                                       voice->mIsLocal, voice->mIsQueued));
  }

  for (uint32_t i=0; i < mDefaultVoices.Length(); ++i) {
    aDefaults->AppendElement(mDefaultVoices[i]->mUri);
  }

  *aIsSpeaking = IsSpeaking();
}

void
nsSynthVoiceRegistry::RecvRemoveVoice(const nsAString& aUri)
{
  // If we dont have a local instance of the registry yet, we will recieve current
  // voices at contruction time.
  if(!gSynthVoiceRegistry) {
    return;
  }

  gSynthVoiceRegistry->RemoveVoice(nullptr, aUri);
}

void
nsSynthVoiceRegistry::RecvAddVoice(const RemoteVoice& aVoice)
{
  // If we dont have a local instance of the registry yet, we will recieve current
  // voices at contruction time.
  if(!gSynthVoiceRegistry) {
    return;
  }

  gSynthVoiceRegistry->AddVoiceImpl(nullptr, aVoice.voiceURI(),
                                    aVoice.name(), aVoice.lang(),
                                    aVoice.localService(), aVoice.queued());
}

void
nsSynthVoiceRegistry::RecvSetDefaultVoice(const nsAString& aUri, bool aIsDefault)
{
  // If we dont have a local instance of the registry yet, we will recieve current
  // voices at contruction time.
  if(!gSynthVoiceRegistry) {
    return;
  }

  gSynthVoiceRegistry->SetDefaultVoice(aUri, aIsDefault);
}

void
nsSynthVoiceRegistry::RecvIsSpeakingChanged(bool aIsSpeaking)
{
  // If we dont have a local instance of the registry yet, we will get the
  // speaking state on construction.
  if(!gSynthVoiceRegistry) {
    return;
  }

  gSynthVoiceRegistry->mIsSpeaking = aIsSpeaking;
}

void
nsSynthVoiceRegistry::RecvNotifyVoicesChanged()
{
  // If we dont have a local instance of the registry yet, we don't care.
  if(!gSynthVoiceRegistry) {
    return;
  }

  gSynthVoiceRegistry->NotifyVoicesChanged();
}

NS_IMETHODIMP
nsSynthVoiceRegistry::AddVoice(nsISpeechService* aService,
                               const nsAString& aUri,
                               const nsAString& aName,
                               const nsAString& aLang,
                               bool aLocalService,
                               bool aQueuesUtterances)
{
  LOG(LogLevel::Debug,
      ("nsSynthVoiceRegistry::AddVoice uri='%s' name='%s' lang='%s' local=%s queued=%s",
       NS_ConvertUTF16toUTF8(aUri).get(), NS_ConvertUTF16toUTF8(aName).get(),
       NS_ConvertUTF16toUTF8(aLang).get(),
       aLocalService ? "true" : "false",
       aQueuesUtterances ? "true" : "false"));

  if(NS_WARN_IF(XRE_IsContentProcess())) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return AddVoiceImpl(aService, aUri, aName, aLang, aLocalService, aQueuesUtterances);
}

NS_IMETHODIMP
nsSynthVoiceRegistry::RemoveVoice(nsISpeechService* aService,
                                  const nsAString& aUri)
{
  LOG(LogLevel::Debug,
      ("nsSynthVoiceRegistry::RemoveVoice uri='%s' (%s)",
       NS_ConvertUTF16toUTF8(aUri).get(),
       (XRE_IsContentProcess()) ? "child" : "parent"));

  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);

  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(!(aService == retval->mService))) {
    return NS_ERROR_INVALID_ARG;
  }

  mVoices.RemoveElement(retval);
  mDefaultVoices.RemoveElement(retval);
  mUriVoiceMap.Remove(aUri);

  if (retval->mIsQueued && !MediaPrefs::WebSpeechForceGlobal()) {
    // Check if this is the last queued voice, and disable the global queue if
    // it is.
    bool queued = false;
    for (uint32_t i = 0; i < mVoices.Length(); i++) {
      VoiceData* voice = mVoices[i];
      if (voice->mIsQueued) {
        queued = true;
        break;
      }
    }
    if (!queued) {
      mUseGlobalQueue = false;
    }
  }

  nsTArray<SpeechSynthesisParent*> ssplist;
  GetAllSpeechSynthActors(ssplist);

  for (uint32_t i = 0; i < ssplist.Length(); ++i)
    Unused << ssplist[i]->SendVoiceRemoved(nsString(aUri));

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::NotifyVoicesChanged()
{
  if (XRE_IsParentProcess()) {
    nsTArray<SpeechSynthesisParent*> ssplist;
    GetAllSpeechSynthActors(ssplist);

    for (uint32_t i = 0; i < ssplist.Length(); ++i)
      Unused << ssplist[i]->SendNotifyVoicesChanged();
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if(NS_WARN_IF(!(obs))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  obs->NotifyObservers(nullptr, "synth-voices-changed", nullptr);

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::SetDefaultVoice(const nsAString& aUri,
                                      bool aIsDefault)
{
  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mDefaultVoices.RemoveElement(retval);

  LOG(LogLevel::Debug, ("nsSynthVoiceRegistry::SetDefaultVoice %s %s",
                     NS_ConvertUTF16toUTF8(aUri).get(),
                     aIsDefault ? "true" : "false"));

  if (aIsDefault) {
    mDefaultVoices.AppendElement(retval);
  }

  if (XRE_IsParentProcess()) {
    nsTArray<SpeechSynthesisParent*> ssplist;
    GetAllSpeechSynthActors(ssplist);

    for (uint32_t i = 0; i < ssplist.Length(); ++i) {
      Unused << ssplist[i]->SendSetDefaultVoice(nsString(aUri), aIsDefault);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceCount(uint32_t* aRetval)
{
  *aRetval = mVoices.Length();

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoice(uint32_t aIndex, nsAString& aRetval)
{
  if(NS_WARN_IF(!(aIndex < mVoices.Length()))) {
    return NS_ERROR_INVALID_ARG;
  }

  aRetval = mVoices[aIndex]->mUri;

  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::IsDefaultVoice(const nsAString& aUri, bool* aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (int32_t i = mDefaultVoices.Length(); i > 0; ) {
    VoiceData* defaultVoice = mDefaultVoices[--i];

    if (voice->mLang.Equals(defaultVoice->mLang)) {
      *aRetval = voice == defaultVoice;
      return NS_OK;
    }
  }

  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::IsLocalVoice(const nsAString& aUri, bool* aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aRetval = voice->mIsLocal;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceLang(const nsAString& aUri, nsAString& aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aRetval = voice->mLang;
  return NS_OK;
}

NS_IMETHODIMP
nsSynthVoiceRegistry::GetVoiceName(const nsAString& aUri, nsAString& aRetval)
{
  bool found;
  VoiceData* voice = mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  aRetval = voice->mName;
  return NS_OK;
}

nsresult
nsSynthVoiceRegistry::AddVoiceImpl(nsISpeechService* aService,
                                   const nsAString& aUri,
                                   const nsAString& aName,
                                   const nsAString& aLang,
                                   bool aLocalService,
                                   bool aQueuesUtterances)
{
  bool found = false;
  mUriVoiceMap.GetWeak(aUri, &found);
  if(NS_WARN_IF(found)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<VoiceData> voice = new VoiceData(aService, aUri, aName, aLang,
                                            aLocalService, aQueuesUtterances);

  mVoices.AppendElement(voice);
  mUriVoiceMap.Put(aUri, voice);
  mUseGlobalQueue |= aQueuesUtterances;

  nsTArray<SpeechSynthesisParent*> ssplist;
  GetAllSpeechSynthActors(ssplist);

  if (!ssplist.IsEmpty()) {
    mozilla::dom::RemoteVoice ssvoice(nsString(aUri),
                                      nsString(aName),
                                      nsString(aLang),
                                      aLocalService,
                                      aQueuesUtterances);

    for (uint32_t i = 0; i < ssplist.Length(); ++i) {
      Unused << ssplist[i]->SendVoiceAdded(ssvoice);
    }
  }

  return NS_OK;
}

bool
nsSynthVoiceRegistry::FindVoiceByLang(const nsAString& aLang,
                                      VoiceData** aRetval)
{
  nsAString::const_iterator dashPos, start, end;
  aLang.BeginReading(start);
  aLang.EndReading(end);

  while (true) {
    nsAutoString langPrefix(Substring(start, end));

    for (int32_t i = mDefaultVoices.Length(); i > 0; ) {
      VoiceData* voice = mDefaultVoices[--i];

      if (StringBeginsWith(voice->mLang, langPrefix)) {
        *aRetval = voice;
        return true;
      }
    }

    for (int32_t i = mVoices.Length(); i > 0; ) {
      VoiceData* voice = mVoices[--i];

      if (StringBeginsWith(voice->mLang, langPrefix)) {
        *aRetval = voice;
        return true;
      }
    }

    dashPos = end;
    end = start;

    if (!RFindInReadable(NS_LITERAL_STRING("-"), end, dashPos)) {
      break;
    }
  }

  return false;
}

VoiceData*
nsSynthVoiceRegistry::FindBestMatch(const nsAString& aUri,
                                    const nsAString& aLang)
{
  if (mVoices.IsEmpty()) {
    return nullptr;
  }

  bool found = false;
  VoiceData* retval = mUriVoiceMap.GetWeak(aUri, &found);

  if (found) {
    LOG(LogLevel::Debug, ("nsSynthVoiceRegistry::FindBestMatch - Matched URI"));
    return retval;
  }

  // Try finding a match for given voice.
  if (!aLang.IsVoid() && !aLang.IsEmpty()) {
    if (FindVoiceByLang(aLang, &retval)) {
      LOG(LogLevel::Debug,
          ("nsSynthVoiceRegistry::FindBestMatch - Matched language (%s ~= %s)",
           NS_ConvertUTF16toUTF8(aLang).get(),
           NS_ConvertUTF16toUTF8(retval->mLang).get()));

      return retval;
    }
  }

  // Try UI language.
  nsresult rv;
  nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  nsAutoString uiLang;
  rv = localeService->GetLocaleComponentForUserAgent(uiLang);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  if (FindVoiceByLang(uiLang, &retval)) {
    LOG(LogLevel::Debug,
        ("nsSynthVoiceRegistry::FindBestMatch - Matched UI language (%s ~= %s)",
         NS_ConvertUTF16toUTF8(uiLang).get(),
         NS_ConvertUTF16toUTF8(retval->mLang).get()));

    return retval;
  }

  // Try en-US, the language of locale "C"
  if (FindVoiceByLang(NS_LITERAL_STRING("en-US"), &retval)) {
    LOG(LogLevel::Debug,
        ("nsSynthVoiceRegistry::FindBestMatch - Matched C locale language (en-US ~= %s)",
         NS_ConvertUTF16toUTF8(retval->mLang).get()));

    return retval;
  }

  // The top default voice is better than nothing...
  if (!mDefaultVoices.IsEmpty()) {
    return mDefaultVoices.LastElement();
  }

  return nullptr;
}

already_AddRefed<nsSpeechTask>
nsSynthVoiceRegistry::SpeakUtterance(SpeechSynthesisUtterance& aUtterance,
                                     const nsAString& aDocLang)
{
  nsString lang = nsString(aUtterance.mLang.IsEmpty() ? aDocLang : aUtterance.mLang);
  nsAutoString uri;

  if (aUtterance.mVoice) {
    aUtterance.mVoice->GetVoiceURI(uri);
  }

  // Get current audio volume to apply speech call
  float volume = aUtterance.Volume();
  RefPtr<AudioChannelService> service = AudioChannelService::GetOrCreate();
  if (service) {
    if (nsCOMPtr<nsPIDOMWindowInner> topWindow = aUtterance.GetOwner()) {
      // TODO : use audio channel agent, open new bug to fix it.
      uint32_t channel = static_cast<uint32_t>(AudioChannelService::GetDefaultAudioChannel());
      AudioPlaybackConfig config = service->GetMediaConfig(topWindow->GetOuterWindow(),
                                                           channel);
      volume = config.mMuted ? 0.0f : config.mVolume * volume;
    }
  }

  RefPtr<nsSpeechTask> task;
  if (XRE_IsContentProcess()) {
    task = new SpeechTaskChild(&aUtterance);
    SpeechSynthesisRequestChild* actor =
      new SpeechSynthesisRequestChild(static_cast<SpeechTaskChild*>(task.get()));
    mSpeechSynthChild->SendPSpeechSynthesisRequestConstructor(actor,
                                                              aUtterance.mText,
                                                              lang,
                                                              uri,
                                                              volume,
                                                              aUtterance.Rate(),
                                                              aUtterance.Pitch());
  } else {
    task = new nsSpeechTask(&aUtterance);
    Speak(aUtterance.mText, lang, uri,
          volume, aUtterance.Rate(), aUtterance.Pitch(), task);
  }

  return task.forget();
}

void
nsSynthVoiceRegistry::Speak(const nsAString& aText,
                            const nsAString& aLang,
                            const nsAString& aUri,
                            const float& aVolume,
                            const float& aRate,
                            const float& aPitch,
                            nsSpeechTask* aTask)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  VoiceData* voice = FindBestMatch(aUri, aLang);

  if (!voice) {
    NS_WARNING("No voices found.");
    aTask->DispatchError(0, 0);
    return;
  }

  aTask->SetChosenVoiceURI(voice->mUri);

  if (mUseGlobalQueue || MediaPrefs::WebSpeechForceGlobal()) {
    LOG(LogLevel::Debug,
        ("nsSynthVoiceRegistry::Speak queueing text='%s' lang='%s' uri='%s' rate=%f pitch=%f",
         NS_ConvertUTF16toUTF8(aText).get(), NS_ConvertUTF16toUTF8(aLang).get(),
         NS_ConvertUTF16toUTF8(aUri).get(), aRate, aPitch));
    RefPtr<GlobalQueueItem> item = new GlobalQueueItem(voice, aTask, aText,
                                                         aVolume, aRate, aPitch);
    mGlobalQueue.AppendElement(item);

    if (mGlobalQueue.Length() == 1) {
      SpeakImpl(item->mVoice, item->mTask, item->mText, item->mVolume, item->mRate,
                item->mPitch);
    }
  } else {
    SpeakImpl(voice, aTask, aText, aVolume, aRate, aPitch);
  }
}

void
nsSynthVoiceRegistry::SpeakNext()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug,
      ("nsSynthVoiceRegistry::SpeakNext %d", mGlobalQueue.IsEmpty()));

  SetIsSpeaking(false);

  if (mGlobalQueue.IsEmpty()) {
    return;
  }

  mGlobalQueue.RemoveElementAt(0);

  while (!mGlobalQueue.IsEmpty()) {
    RefPtr<GlobalQueueItem> item = mGlobalQueue.ElementAt(0);
    if (item->mTask->IsPreCanceled()) {
      mGlobalQueue.RemoveElementAt(0);
      continue;
    }
    if (!item->mTask->IsPrePaused()) {
      SpeakImpl(item->mVoice, item->mTask, item->mText, item->mVolume,
                item->mRate, item->mPitch);
    }
    break;
  }
}

void
nsSynthVoiceRegistry::ResumeQueue()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  LOG(LogLevel::Debug,
      ("nsSynthVoiceRegistry::ResumeQueue %d", mGlobalQueue.IsEmpty()));

  if (mGlobalQueue.IsEmpty()) {
    return;
  }

  RefPtr<GlobalQueueItem> item = mGlobalQueue.ElementAt(0);
  if (!item->mTask->IsPrePaused()) {
    SpeakImpl(item->mVoice, item->mTask, item->mText, item->mVolume,
              item->mRate, item->mPitch);
  }
}

bool
nsSynthVoiceRegistry::IsSpeaking()
{
  return mIsSpeaking;
}

void
nsSynthVoiceRegistry::SetIsSpeaking(bool aIsSpeaking)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  // Only set to 'true' if global queue is enabled.
  mIsSpeaking =
    aIsSpeaking && (mUseGlobalQueue || MediaPrefs::WebSpeechForceGlobal());

  nsTArray<SpeechSynthesisParent*> ssplist;
  GetAllSpeechSynthActors(ssplist);
  for (uint32_t i = 0; i < ssplist.Length(); ++i) {
    Unused << ssplist[i]->SendIsSpeakingChanged(aIsSpeaking);
  }
}

void
nsSynthVoiceRegistry::SpeakImpl(VoiceData* aVoice,
                                nsSpeechTask* aTask,
                                const nsAString& aText,
                                const float& aVolume,
                                const float& aRate,
                                const float& aPitch)
{
  LOG(LogLevel::Debug,
      ("nsSynthVoiceRegistry::SpeakImpl queueing text='%s' uri='%s' rate=%f pitch=%f",
       NS_ConvertUTF16toUTF8(aText).get(), NS_ConvertUTF16toUTF8(aVoice->mUri).get(),
       aRate, aPitch));

  SpeechServiceType serviceType;

  DebugOnly<nsresult> rv = aVoice->mService->GetServiceType(&serviceType);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to get speech service type");

  if (serviceType == nsISpeechService::SERVICETYPE_INDIRECT_AUDIO) {
    aTask->InitIndirectAudio();
  } else {
    aTask->InitDirectAudio();
  }

  if (NS_FAILED(aVoice->mService->Speak(aText, aVoice->mUri, aVolume, aRate,
                                        aPitch, aTask))) {
    if (serviceType == nsISpeechService::SERVICETYPE_INDIRECT_AUDIO) {
      aTask->DispatchError(0, 0);
    }
    // XXX When using direct audio, no way to dispatch error
  }
}

} // namespace dom
} // namespace mozilla
