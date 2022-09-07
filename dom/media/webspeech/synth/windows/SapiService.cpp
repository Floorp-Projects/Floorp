/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "SapiService.h"
#include "nsServiceManagerUtils.h"
#include "nsEscape.h"
#include "nsXULAppAPI.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/dom/nsSpeechTask.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla::dom {

constexpr static WCHAR kSpCategoryOneCoreVoices[] =
    L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices";

StaticRefPtr<SapiService> SapiService::sSingleton;

class SapiCallback final : public nsISpeechTaskCallback {
 public:
  SapiCallback(nsISpeechTask* aTask, ISpVoice* aSapiClient,
               uint32_t aTextOffset, uint32_t aSpeakTextLen)
      : mTask(aTask),
        mSapiClient(aSapiClient),
        mTextOffset(aTextOffset),
        mSpeakTextLen(aSpeakTextLen),
        mCurrentIndex(0),
        mStreamNum(0) {
    mStartingTime = TimeStamp::Now();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(SapiCallback, nsISpeechTaskCallback)

  NS_DECL_NSISPEECHTASKCALLBACK

  ULONG GetStreamNum() const { return mStreamNum; }
  void SetStreamNum(ULONG aValue) { mStreamNum = aValue; }

  void OnSpeechEvent(const SPEVENT& speechEvent);

 private:
  ~SapiCallback() {}

  float GetTimeDurationFromStart() const {
    TimeDuration duration = TimeStamp::Now() - mStartingTime;
    return duration.ToSeconds();
  }

  // This pointer is used to dispatch events
  nsCOMPtr<nsISpeechTask> mTask;
  RefPtr<ISpVoice> mSapiClient;

  uint32_t mTextOffset;
  uint32_t mSpeakTextLen;

  // Used for calculating the time taken to speak the utterance
  TimeStamp mStartingTime;
  uint32_t mCurrentIndex;

  ULONG mStreamNum;
};

NS_IMPL_CYCLE_COLLECTION(SapiCallback, mTask);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SapiCallback)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTaskCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTaskCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SapiCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SapiCallback)

NS_IMETHODIMP
SapiCallback::OnPause() {
  if (FAILED(mSapiClient->Pause())) {
    return NS_ERROR_FAILURE;
  }
  if (!mTask) {
    // When calling pause() on child porcess, it may not receive end event
    // from chrome process yet.
    return NS_ERROR_FAILURE;
  }
  mTask->DispatchPause(GetTimeDurationFromStart(), mCurrentIndex);
  return NS_OK;
}

NS_IMETHODIMP
SapiCallback::OnResume() {
  if (FAILED(mSapiClient->Resume())) {
    return NS_ERROR_FAILURE;
  }
  if (!mTask) {
    // When calling resume() on child porcess, it may not receive end event
    // from chrome process yet.
    return NS_ERROR_FAILURE;
  }
  mTask->DispatchResume(GetTimeDurationFromStart(), mCurrentIndex);
  return NS_OK;
}

NS_IMETHODIMP
SapiCallback::OnCancel() {
  // After cancel, mCurrentIndex may be updated.
  // At cancel case, use mCurrentIndex for DispatchEnd.
  mSpeakTextLen = 0;
  // Purge all the previous utterances and speak an empty string
  if (FAILED(mSapiClient->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
SapiCallback::OnVolumeChanged(float aVolume) {
  mSapiClient->SetVolume(static_cast<USHORT>(aVolume * 100));
  return NS_OK;
}

void SapiCallback::OnSpeechEvent(const SPEVENT& speechEvent) {
  switch (speechEvent.eEventId) {
    case SPEI_START_INPUT_STREAM:
      mTask->DispatchStart();
      break;
    case SPEI_END_INPUT_STREAM:
      if (mSpeakTextLen) {
        mCurrentIndex = mSpeakTextLen;
      }
      mTask->DispatchEnd(GetTimeDurationFromStart(), mCurrentIndex);
      mTask = nullptr;
      break;
    case SPEI_TTS_BOOKMARK:
      mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
      mTask->DispatchBoundary(u"mark"_ns, GetTimeDurationFromStart(),
                              mCurrentIndex, 0, 0);
      break;
    case SPEI_WORD_BOUNDARY:
      mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
      mTask->DispatchBoundary(u"word"_ns, GetTimeDurationFromStart(),
                              mCurrentIndex,
                              static_cast<ULONG>(speechEvent.wParam), 1);
      break;
    case SPEI_SENTENCE_BOUNDARY:
      mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
      mTask->DispatchBoundary(u"sentence"_ns, GetTimeDurationFromStart(),
                              mCurrentIndex,
                              static_cast<ULONG>(speechEvent.wParam), 1);
      break;
    default:
      break;
  }
}

// static
void __stdcall SapiService::SpeechEventCallback(WPARAM aWParam,
                                                LPARAM aLParam) {
  RefPtr<ISpVoice> spVoice = (ISpVoice*)aWParam;
  RefPtr<SapiService> service = (SapiService*)aLParam;

  SPEVENT speechEvent;
  while (spVoice->GetEvents(1, &speechEvent, nullptr) == S_OK) {
    for (size_t i = 0; i < service->mCallbacks.Length(); i++) {
      RefPtr<SapiCallback> callback = service->mCallbacks[i];
      if (callback->GetStreamNum() == speechEvent.ulStreamNum) {
        callback->OnSpeechEvent(speechEvent);
        if (speechEvent.eEventId == SPEI_END_INPUT_STREAM) {
          service->mCallbacks.RemoveElementAt(i);
        }
        break;
      }
    }
  }
}

NS_INTERFACE_MAP_BEGIN(SapiService)
  NS_INTERFACE_MAP_ENTRY(nsISpeechService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechService)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SapiService)
NS_IMPL_RELEASE(SapiService)

SapiService::SapiService() : mInitialized(false) {}

SapiService::~SapiService() {}

bool SapiService::Init() {
  AUTO_PROFILER_LABEL("SapiService::Init", OTHER);

  MOZ_ASSERT(!mInitialized);

  if (Preferences::GetBool("media.webspeech.synth.test") ||
      !StaticPrefs::media_webspeech_synth_enabled()) {
    // When enabled, we shouldn't add OS backend (Bug 1160844)
    return false;
  }

  // Get all the voices from sapi and register in the SynthVoiceRegistry
  if (!RegisterVoices()) {
    return false;
  }

  mInitialized = true;
  return true;
}

already_AddRefed<ISpVoice> SapiService::InitSapiInstance() {
  RefPtr<ISpVoice> spVoice;
  if (FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice,
                              getter_AddRefs(spVoice)))) {
    return nullptr;
  }

  // Set interest for all the events we are interested in
  ULONGLONG eventMask = SPFEI(SPEI_START_INPUT_STREAM) |
                        SPFEI(SPEI_TTS_BOOKMARK) | SPFEI(SPEI_WORD_BOUNDARY) |
                        SPFEI(SPEI_SENTENCE_BOUNDARY) |
                        SPFEI(SPEI_END_INPUT_STREAM);

  if (FAILED(spVoice->SetInterest(eventMask, eventMask))) {
    return nullptr;
  }

  // Set the callback function for receiving the events
  spVoice->SetNotifyCallbackFunction(
      (SPNOTIFYCALLBACK*)SapiService::SpeechEventCallback,
      (WPARAM)spVoice.get(), (LPARAM)this);

  return spVoice.forget();
}

bool SapiService::RegisterVoices() {
  nsCOMPtr<nsISynthVoiceRegistry> registry =
      do_GetService(NS_SYNTHVOICEREGISTRY_CONTRACTID);
  if (!registry) {
    return false;
  }
  bool result = RegisterVoices(registry, kSpCategoryOneCoreVoices);
  result |= RegisterVoices(registry, SPCAT_VOICES);
  if (result) {
    registry->NotifyVoicesChanged();
  }
  return result;
}

bool SapiService::RegisterVoices(nsCOMPtr<nsISynthVoiceRegistry>& registry,
                                 const WCHAR* categoryId) {
  nsresult rv;

  RefPtr<ISpObjectTokenCategory> category;
  if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                              IID_ISpObjectTokenCategory,
                              getter_AddRefs(category)))) {
    return false;
  }
  if (FAILED(category->SetId(categoryId, FALSE))) {
    return false;
  }

  RefPtr<IEnumSpObjectTokens> voiceTokens;
  if (FAILED(category->EnumTokens(nullptr, nullptr,
                                  getter_AddRefs(voiceTokens)))) {
    return false;
  }

  WCHAR locale[LOCALE_NAME_MAX_LENGTH];
  while (true) {
    RefPtr<ISpObjectToken> voiceToken;
    if (voiceTokens->Next(1, getter_AddRefs(voiceToken), nullptr) != S_OK) {
      break;
    }

    RefPtr<ISpDataKey> attributes;
    if (FAILED(
            voiceToken->OpenKey(L"Attributes", getter_AddRefs(attributes)))) {
      continue;
    }

    WCHAR* language = nullptr;
    if (FAILED(attributes->GetStringValue(L"Language", &language))) {
      continue;
    }

    // Language attribute is LCID by hex.  So we need convert to locale
    // name.
    nsAutoString hexLcid;
    LCID lcid = wcstol(language, nullptr, 16);
    CoTaskMemFree(language);
    if (NS_WARN_IF(
            !LCIDToLocaleName(lcid, locale, LOCALE_NAME_MAX_LENGTH, 0))) {
      continue;
    }

    WCHAR* description = nullptr;
    if (FAILED(voiceToken->GetStringValue(nullptr, &description))) {
      continue;
    }

    nsAutoString uri;
    uri.AssignLiteral("urn:moz-tts:sapi:");
    uri.Append(description);
    uri.AppendLiteral("?");
    uri.Append(locale);

    // This service can only speak one utterance at a time, se we set
    // aQueuesUtterances to true in order to track global state and schedule
    // access to this service.
    rv = registry->AddVoice(this, uri, nsDependentString(description),
                            nsDependentString(locale), true, true);
    CoTaskMemFree(description);
    if (NS_FAILED(rv)) {
      continue;
    }

    mVoices.InsertOrUpdate(uri, std::move(voiceToken));
  }

  return true;
}

NS_IMETHODIMP
SapiService::Speak(const nsAString& aText, const nsAString& aUri, float aVolume,
                   float aRate, float aPitch, nsISpeechTask* aTask) {
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_AVAILABLE);

  RefPtr<ISpObjectToken> voiceToken;
  if (!mVoices.Get(aUri, getter_AddRefs(voiceToken))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<ISpVoice> spVoice = InitSapiInstance();
  if (!spVoice) {
    return NS_ERROR_FAILURE;
  }

  if (FAILED(spVoice->SetVoice(voiceToken))) {
    return NS_ERROR_FAILURE;
  }

  if (FAILED(spVoice->SetVolume(static_cast<USHORT>(aVolume * 100)))) {
    return NS_ERROR_FAILURE;
  }

  // The max supported rate in SAPI engines is 3x, and the min is 1/3x. It is
  // expressed by an integer. 0 being normal rate, -10 is 1/3 and 10 is 3x.
  // Values below and above that are allowed, but the engine may clip the rate
  // to its maximum capable value.
  // "Each increment between -10 and +10 is logarithmically distributed such
  //  that incrementing or decrementing by 1 is multiplying or dividing the
  //  rate by the 10th root of 3"
  // https://msdn.microsoft.com/en-us/library/ee431826(v=vs.85).aspx
  long rate = aRate != 0 ? static_cast<long>(10 * log10(aRate) / log10(3)) : 0;
  if (FAILED(spVoice->SetRate(rate))) {
    return NS_ERROR_FAILURE;
  }

  // Set the pitch using xml
  nsAutoString xml;
  xml.AssignLiteral("<pitch absmiddle=\"");
  // absmiddle doesn't allow float type
  xml.AppendInt(static_cast<int32_t>(aPitch * 10.0f - 10.0f));
  xml.AppendLiteral("\">");
  uint32_t textOffset = xml.Length();

  for (size_t i = 0; i < aText.Length(); i++) {
    switch (aText[i]) {
      case '&':
        xml.AppendLiteral("&amp;");
        break;
      case '<':
        xml.AppendLiteral("&lt;");
        break;
      case '>':
        xml.AppendLiteral("&gt;");
        break;
      default:
        xml.Append(aText[i]);
        break;
    }
  }

  xml.AppendLiteral("</pitch>");

  RefPtr<SapiCallback> callback =
      new SapiCallback(aTask, spVoice, textOffset, aText.Length());

  // The last three parameters doesn't matter for an indirect service
  nsresult rv = aTask->Setup(callback);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ULONG streamNum;
  if (FAILED(spVoice->Speak(xml.get(), SPF_ASYNC, &streamNum))) {
    aTask->Setup(nullptr);
    return NS_ERROR_FAILURE;
  }

  callback->SetStreamNum(streamNum);
  // streamNum reassigns same value when last stream is finished even if
  // callback for stream end isn't called
  // So we cannot use data hashtable and has to add it to vector at last.
  mCallbacks.AppendElement(callback);

  return NS_OK;
}

NS_IMETHODIMP
SapiService::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  return NS_OK;
}

SapiService* SapiService::GetInstance() {
  MOZ_ASSERT(NS_IsMainThread());
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    MOZ_ASSERT(false, "SapiService can only be started on main gecko process");
    return nullptr;
  }

  if (!sSingleton) {
    RefPtr<SapiService> service = new SapiService();
    if (service->Init()) {
      sSingleton = service;
      ClearOnShutdown(&sSingleton);
    }
  }
  return sSingleton;
}

already_AddRefed<SapiService> SapiService::GetInstanceForService() {
  RefPtr<SapiService> sapiService = GetInstance();
  return sapiService.forget();
}

}  // namespace mozilla::dom
