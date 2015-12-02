/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "SapiService.h"
#include "nsServiceManagerUtils.h"
#include "nsWin32Locale.h"

#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/dom/nsSpeechTask.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

StaticRefPtr<SapiService> SapiService::sSingleton;

class SapiCallback final : public nsISpeechTaskCallback
{
public:
  SapiCallback(nsISpeechTask* aTask, ISpVoice* aSapiClient,
               uint32_t aTextOffset, uint32_t aSpeakTextLen)
    : mTask(aTask)
    , mSapiClient(aSapiClient)
    , mTextOffset(aTextOffset)
    , mSpeakTextLen(aSpeakTextLen)
    , mCurrentIndex(0)
    , mStreamNum(0)
  {
    mStartingTime = GetTickCount();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(SapiCallback, nsISpeechTaskCallback)

  NS_DECL_NSISPEECHTASKCALLBACK

  ULONG GetStreamNum() const { return mStreamNum; }
  void SetStreamNum(ULONG aValue) { mStreamNum = aValue; }

  void OnSpeechEvent(const SPEVENT& speechEvent);

private:
  ~SapiCallback() { }

  // This pointer is used to dispatch events
  nsCOMPtr<nsISpeechTask> mTask;
  RefPtr<ISpVoice> mSapiClient;

  uint32_t mTextOffset;
  uint32_t mSpeakTextLen;

  // Used for calculating the time taken to speak the utterance
  double mStartingTime;
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
SapiCallback::OnPause()
{
  if (FAILED(mSapiClient->Pause())) {
    return NS_ERROR_FAILURE;
  }
  mTask->DispatchPause(GetTickCount() - mStartingTime, mCurrentIndex);
  return NS_OK;
}

NS_IMETHODIMP
SapiCallback::OnResume()
{
  if (FAILED(mSapiClient->Resume())) {
    return NS_ERROR_FAILURE;
  }
  mTask->DispatchResume(GetTickCount() - mStartingTime, mCurrentIndex);
  return NS_OK;
}

NS_IMETHODIMP
SapiCallback::OnCancel()
{
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
SapiCallback::OnVolumeChanged(float aVolume)
{
  mSapiClient->SetVolume(static_cast<USHORT>(aVolume * 100));
  return NS_OK;
}

void
SapiCallback::OnSpeechEvent(const SPEVENT& speechEvent)
{
  switch (speechEvent.eEventId) {
  case SPEI_START_INPUT_STREAM:
    mTask->DispatchStart();
    break;
  case SPEI_END_INPUT_STREAM:
    if (mSpeakTextLen) {
      mCurrentIndex = mSpeakTextLen;
    }
    mTask->DispatchEnd(GetTickCount() - mStartingTime, mCurrentIndex);
    mTask = nullptr;
    break;
  case SPEI_TTS_BOOKMARK:
    mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
    mTask->DispatchBoundary(NS_LITERAL_STRING("mark"),
                            GetTickCount() - mStartingTime, mCurrentIndex);
    break;
  case SPEI_WORD_BOUNDARY:
    mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
    mTask->DispatchBoundary(NS_LITERAL_STRING("word"),
                            GetTickCount() - mStartingTime, mCurrentIndex);
    break;
  case SPEI_SENTENCE_BOUNDARY:
    mCurrentIndex = static_cast<ULONG>(speechEvent.lParam) - mTextOffset;
    mTask->DispatchBoundary(NS_LITERAL_STRING("sentence"),
                            GetTickCount() - mStartingTime, mCurrentIndex);
    break;
  default:
    break;
  }
}

// static
void __stdcall
SapiService::SpeechEventCallback(WPARAM aWParam, LPARAM aLParam)
{
  RefPtr<SapiService> service = (SapiService*) aWParam;

  SPEVENT speechEvent;
  while (service->mSapiClient->GetEvents(1, &speechEvent, nullptr) == S_OK) {
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

SapiService::SapiService()
  : mInitialized(false)
{
}

SapiService::~SapiService()
{
}

bool
SapiService::Init()
{
  MOZ_ASSERT(!mInitialized);

  if (Preferences::GetBool("media.webspeech.synth.test") ||
      !Preferences::GetBool("media.webspeech.synth.enabled")) {
    // When enabled, we shouldn't add OS backend (Bug 1160844)
    return false;
  }

  if (FAILED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice,
                              getter_AddRefs(mSapiClient)))) {
    return false;
  }

  // Set interest for all the events we are interested in
  ULONGLONG eventMask =
    SPFEI(SPEI_START_INPUT_STREAM) |
    SPFEI(SPEI_TTS_BOOKMARK) |
    SPFEI(SPEI_WORD_BOUNDARY) |
    SPFEI(SPEI_SENTENCE_BOUNDARY) |
    SPFEI(SPEI_END_INPUT_STREAM);

  if (FAILED(mSapiClient->SetInterest(eventMask, eventMask))) {
    return false;
  }

  // Get all the voices from sapi and register in the SynthVoiceRegistry
  if (!RegisterVoices()) {
    return false;
  }

  // Set the callback function for receiving the events
  mSapiClient->SetNotifyCallbackFunction(
    (SPNOTIFYCALLBACK*) SapiService::SpeechEventCallback, (WPARAM) this, 0);

  mInitialized = true;
  return true;
}

bool
SapiService::RegisterVoices()
{
  nsresult rv;

  nsCOMPtr<nsISynthVoiceRegistry> registry =
    do_GetService(NS_SYNTHVOICEREGISTRY_CONTRACTID);
  if (!registry) {
    return false;
  }

  RefPtr<ISpObjectTokenCategory> category;
  if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
                              IID_ISpObjectTokenCategory,
                              getter_AddRefs(category)))) {
    return false;
  }
  if (FAILED(category->SetId(SPCAT_VOICES, FALSE))) {
    return false;
  }

  RefPtr<IEnumSpObjectTokens> voiceTokens;
  if (FAILED(category->EnumTokens(nullptr, nullptr,
                                  getter_AddRefs(voiceTokens)))) {
    return false;
  }

  while (true) {
    RefPtr<ISpObjectToken> voiceToken;
    if (voiceTokens->Next(1, getter_AddRefs(voiceToken), nullptr) != S_OK) {
      break;
    }

    RefPtr<ISpDataKey> attributes;
    if (FAILED(voiceToken->OpenKey(L"Attributes",
                                   getter_AddRefs(attributes)))) {
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
    nsAutoString locale;
    nsWin32Locale::GetXPLocale(lcid, locale);

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
    rv = registry->AddVoice(this, uri, nsDependentString(description), locale,
                            true, true);
    CoTaskMemFree(description);
    if (NS_FAILED(rv)) {
      continue;
    }

    mVoices.Put(uri, voiceToken);
  }

  return true;
}

NS_IMETHODIMP
SapiService::Speak(const nsAString& aText, const nsAString& aUri,
                   float aVolume, float aRate, float aPitch,
                   nsISpeechTask* aTask)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_AVAILABLE);

  RefPtr<ISpObjectToken> voiceToken;
  if (!mVoices.Get(aUri, getter_AddRefs(voiceToken))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (FAILED(mSapiClient->SetVoice(voiceToken))) {
    return NS_ERROR_FAILURE;
  }
  if (FAILED(mSapiClient->SetVolume(static_cast<USHORT>(aVolume * 100)))) {
    return NS_ERROR_FAILURE;
  }
  if (FAILED(mSapiClient->SetRate(static_cast<long>(10 * log10(aRate))))) {
    return NS_ERROR_FAILURE;
  }

  // Set the pitch using xml
  nsAutoString xml;
  xml.AssignLiteral("<pitch absmiddle=\"");
  xml.AppendFloat(aPitch * 10.0f - 10.0f);
  xml.AppendLiteral("\">");
  uint32_t textOffset = xml.Length();

  xml.Append(aText);
  xml.AppendLiteral("</pitch>");

  RefPtr<SapiCallback> callback =
    new SapiCallback(aTask, mSapiClient, textOffset, aText.Length());

  // The last three parameters doesn't matter for an indirect service
  nsresult rv = aTask->Setup(callback, 0, 0, 0);
  if (NS_FAILED(rv)) {
    return rv;
  }

  ULONG streamNum;
  if (FAILED(mSapiClient->Speak(xml.get(), SPF_ASYNC, &streamNum))) {
    aTask->Setup(nullptr, 0, 0, 0);
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
SapiService::GetServiceType(SpeechServiceType* aServiceType)
{
  *aServiceType = nsISpeechService::SERVICETYPE_INDIRECT_AUDIO;
  return NS_OK;
}

NS_IMETHODIMP
SapiService::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  return NS_OK;
}

SapiService*
SapiService::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    MOZ_ASSERT(false,
               "SapiService can only be started on main gecko process");
    return nullptr;
  }

  if (!sSingleton) {
    RefPtr<SapiService> service = new SapiService();
    if (service->Init()) {
      sSingleton = service;
    }
  }
  return sSingleton;
}

already_AddRefed<SapiService>
SapiService::GetInstanceForService()
{
  RefPtr<SapiService> sapiService = GetInstance();
  return sapiService.forget();
}

void
SapiService::Shutdown()
{
  if (!sSingleton) {
    return;
  }
  sSingleton = nullptr;
}

} // namespace dom
} // namespace mozilla
