/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsPicoService.h"
#include "nsPrintfCString.h"
#include "nsIWeakReferenceUtils.h"
#include "SharedBuffer.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/dom/nsSpeechTask.h"

#include "nsIFile.h"
#include "nsThreadUtils.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "mozilla/DebugOnly.h"
#include <dlfcn.h>

// Pico API constants

// Size of memory allocated for pico engine and voice resources.
// We only have one voice and its resources loaded at once, so this
// should always be enough.
#define PICO_MEM_SIZE 2500000

// Max length of returned strings. Pico will never return longer strings,
// so this amount should be good enough for preallocating.
#define PICO_RETSTRINGSIZE 200

// Max amount we want from a single call of pico_getData
#define PICO_MAX_CHUNK_SIZE 128

// Arbitrary name for loaded voice, it doesn't mean anything outside of Pico
#define PICO_VOICE_NAME "pico"

// Return status from pico_getData meaning there is more data in the pipeline
// to get from more calls to pico_getData
#define PICO_STEP_BUSY 201

// For performing a "soft" reset between utterances. This is used when one
// utterance is interrupted by a new one.
#define PICO_RESET_SOFT 0x10

// Currently, Pico only provides mono output.
#define PICO_CHANNELS_NUM 1

// Pico's sample rate is always 16000
#define PICO_SAMPLE_RATE 16000

// The path to the language files in Gonk
#define GONK_PICO_LANG_PATH "/system/tts/lang_pico"

namespace mozilla {
namespace dom {

StaticRefPtr<nsPicoService> nsPicoService::sSingleton;

class PicoApi
{
public:

  PicoApi() : mInitialized(false) {}

  bool Init()
  {
    if (mInitialized) {
      return true;
    }

    void* handle = dlopen("libttspico.so", RTLD_LAZY);

    if (!handle) {
      NS_WARNING("Failed to open libttspico.so, pico cannot run");
      return false;
    }

    pico_initialize =
      (pico_Status (*)(void*, uint32_t, pico_System*))dlsym(
        handle, "pico_initialize");

    pico_terminate =
      (pico_Status (*)(pico_System*))dlsym(handle, "pico_terminate");

    pico_getSystemStatusMessage =
      (pico_Status (*)(pico_System, pico_Status, pico_Retstring))dlsym(
        handle, "pico_getSystemStatusMessage");;

    pico_loadResource =
      (pico_Status (*)(pico_System, const char*, pico_Resource*))dlsym(
        handle, "pico_loadResource");

    pico_unloadResource =
      (pico_Status (*)(pico_System, pico_Resource*))dlsym(
        handle, "pico_unloadResource");

    pico_getResourceName =
      (pico_Status (*)(pico_System, pico_Resource, pico_Retstring))dlsym(
        handle, "pico_getResourceName");

    pico_createVoiceDefinition =
      (pico_Status (*)(pico_System, const char*))dlsym(
        handle, "pico_createVoiceDefinition");

    pico_addResourceToVoiceDefinition =
      (pico_Status (*)(pico_System, const char*, const char*))dlsym(
        handle, "pico_addResourceToVoiceDefinition");

    pico_releaseVoiceDefinition =
      (pico_Status (*)(pico_System, const char*))dlsym(
        handle, "pico_releaseVoiceDefinition");

    pico_newEngine =
      (pico_Status (*)(pico_System, const char*, pico_Engine*))dlsym(
        handle, "pico_newEngine");

    pico_disposeEngine =
      (pico_Status (*)(pico_System, pico_Engine*))dlsym(
        handle, "pico_disposeEngine");

    pico_resetEngine =
      (pico_Status (*)(pico_Engine, int32_t))dlsym(handle, "pico_resetEngine");

    pico_putTextUtf8 =
      (pico_Status (*)(pico_Engine, const char*, const int16_t, int16_t*))dlsym(
        handle, "pico_putTextUtf8");

    pico_getData =
      (pico_Status (*)(pico_Engine, void*, int16_t, int16_t*, int16_t*))dlsym(
        handle, "pico_getData");

    mInitialized = true;
    return true;
  }

  typedef signed int pico_Status;
  typedef char pico_Retstring[PICO_RETSTRINGSIZE];

  pico_Status (* pico_initialize)(void*, uint32_t, pico_System*);
  pico_Status (* pico_terminate)(pico_System*);
  pico_Status (* pico_getSystemStatusMessage)(
    pico_System, pico_Status, pico_Retstring);

  pico_Status (* pico_loadResource)(pico_System, const char*, pico_Resource*);
  pico_Status (* pico_unloadResource)(pico_System, pico_Resource*);
  pico_Status (* pico_getResourceName)(
    pico_System, pico_Resource, pico_Retstring);
  pico_Status (* pico_createVoiceDefinition)(pico_System, const char*);
  pico_Status (* pico_addResourceToVoiceDefinition)(
    pico_System, const char*, const char*);
  pico_Status (* pico_releaseVoiceDefinition)(pico_System, const char*);
  pico_Status (* pico_newEngine)(pico_System, const char*, pico_Engine*);
  pico_Status (* pico_disposeEngine)(pico_System, pico_Engine*);

  pico_Status (* pico_resetEngine)(pico_Engine, int32_t);
  pico_Status (* pico_putTextUtf8)(
    pico_Engine, const char*, const int16_t, int16_t*);
  pico_Status (* pico_getData)(
    pico_Engine, void*, const int16_t, int16_t*, int16_t*);

private:

  bool mInitialized;

} sPicoApi;

#define PICO_ENSURE_SUCCESS_VOID(_funcName, _status)                      \
  if (_status < 0) {                                                      \
    PicoApi::pico_Retstring message;                                      \
    sPicoApi.pico_getSystemStatusMessage(                                 \
      nsPicoService::sSingleton->mPicoSystem, _status, message);          \
    NS_WARNING(                                                           \
      nsPrintfCString("Error running %s: %s", _funcName, message).get()); \
    return;                                                               \
  }

#define PICO_ENSURE_SUCCESS(_funcName, _status, _rv)                      \
  if (_status < 0) {                                                      \
    PicoApi::pico_Retstring message;                                      \
    sPicoApi.pico_getSystemStatusMessage(                                 \
      nsPicoService::sSingleton->mPicoSystem, _status, message);          \
    NS_WARNING(                                                           \
      nsPrintfCString("Error running %s: %s", _funcName, message).get()); \
    return _rv;                                                           \
  }

class PicoVoice
{
public:

  PicoVoice(const nsAString& aLanguage)
    : mLanguage(aLanguage) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PicoVoice)

  // Voice language, in BCB-47 syntax
  nsString mLanguage;

  // Language resource file
  nsCString mTaFile;

  // Speaker resource file
  nsCString mSgFile;

private:
    ~PicoVoice() {}
};

class PicoCallbackRunnable : public nsRunnable,
                             public nsISpeechTaskCallback
{
  friend class PicoSynthDataRunnable;

public:
  PicoCallbackRunnable(const nsAString& aText, PicoVoice* aVoice,
                       float aRate, float aPitch, nsISpeechTask* aTask,
                       nsPicoService* aService)
    : mText(NS_ConvertUTF16toUTF8(aText))
    , mRate(aRate)
    , mPitch(aPitch)
    , mFirstData(true)
    , mTask(aTask)
    , mVoice(aVoice)
    , mService(aService) { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISPEECHTASKCALLBACK

  NS_IMETHOD Run() override;

  bool IsCurrentTask() { return mService->mCurrentTask == mTask; }

private:
  ~PicoCallbackRunnable() { }

  void DispatchSynthDataRunnable(already_AddRefed<SharedBuffer>&& aBuffer,
                                 size_t aBufferSize);

  nsCString mText;

  float mRate;

  float mPitch;

  bool mFirstData;

  // We use this pointer to compare it with the current service task.
  // If they differ, this runnable should stop.
  nsISpeechTask* mTask;

  // We hold a strong reference to the service, which in turn holds
  // a strong reference to this voice.
  PicoVoice* mVoice;

  // By holding a strong reference to the service we guarantee that it won't be
  // destroyed before this runnable.
  nsRefPtr<nsPicoService> mService;
};

NS_IMPL_ISUPPORTS_INHERITED(PicoCallbackRunnable, nsRunnable, nsISpeechTaskCallback)

// nsRunnable

NS_IMETHODIMP
PicoCallbackRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());
  PicoApi::pico_Status status = 0;

  if (mService->CurrentVoice() != mVoice) {
    mService->LoadEngine(mVoice);
  } else {
    status = sPicoApi.pico_resetEngine(mService->mPicoEngine, PICO_RESET_SOFT);
    PICO_ENSURE_SUCCESS("pico_unloadResource", status, NS_ERROR_FAILURE);
  }

  // Add SSML markup for pitch and rate. Pico uses a minimal parser,
  // so no namespace is needed.
  nsPrintfCString markedUpText(
    "<pitch level=\"%0.0f\"><speed level=\"%0.0f\">%s</speed></pitch>",
    std::min(std::max(50.0f, mPitch * 100), 200.0f),
    std::min(std::max(20.0f, mRate * 100), 500.0f),
    mText.get());

  const char* text = markedUpText.get();
  size_t buffer_size = 512, buffer_offset = 0;
  nsRefPtr<SharedBuffer> buffer = SharedBuffer::Create(buffer_size);
  int16_t text_offset = 0, bytes_recv = 0, bytes_sent = 0, out_data_type = 0;
  int16_t text_remaining = markedUpText.Length() + 1;

  // Run this loop while this is the current task
  while (IsCurrentTask()) {
    if (text_remaining) {
      status = sPicoApi.pico_putTextUtf8(mService->mPicoEngine,
                                         text + text_offset, text_remaining,
                                         &bytes_sent);
      PICO_ENSURE_SUCCESS("pico_putTextUtf8", status, NS_ERROR_FAILURE);
      // XXX: End speech task on error
      text_remaining -= bytes_sent;
      text_offset += bytes_sent;
    } else {
      // If we already fed all the text to the engine, send a zero length buffer
      // and quit.
      DispatchSynthDataRunnable(already_AddRefed<SharedBuffer>(), 0);
      break;
    }

    do {
      // Run this loop while the result of getData is STEP_BUSY, when it finishes
      // synthesizing audio for the given text, it returns STEP_IDLE. We then
      // break to the outer loop and feed more text, if there is any left.
      if (!IsCurrentTask()) {
        // If the task has changed, quit.
        break;
      }

      if (buffer_size - buffer_offset < PICO_MAX_CHUNK_SIZE) {
        // The next audio chunk retrieved may be bigger than our buffer,
        // so send the data and flush the buffer.
        DispatchSynthDataRunnable(buffer.forget(), buffer_offset);
        buffer_offset = 0;
        buffer = SharedBuffer::Create(buffer_size);
      }

      status = sPicoApi.pico_getData(mService->mPicoEngine,
                                     (uint8_t*)buffer->Data() + buffer_offset,
                                     PICO_MAX_CHUNK_SIZE,
                                     &bytes_recv, &out_data_type);
      PICO_ENSURE_SUCCESS("pico_getData", status, NS_ERROR_FAILURE);
      buffer_offset += bytes_recv;
    } while (status == PICO_STEP_BUSY);
  }

  return NS_OK;
}

void
PicoCallbackRunnable::DispatchSynthDataRunnable(
  already_AddRefed<SharedBuffer>&& aBuffer, size_t aBufferSize)
{
  class PicoSynthDataRunnable final : public nsRunnable
  {
  public:
    PicoSynthDataRunnable(already_AddRefed<SharedBuffer>& aBuffer,
                          size_t aBufferSize, bool aFirstData,
                          PicoCallbackRunnable* aCallback)
      : mBuffer(aBuffer)
      , mBufferSize(aBufferSize)
      , mFirstData(aFirstData)
      , mCallback(aCallback) {
    }

    NS_IMETHOD Run()
    {
      MOZ_ASSERT(NS_IsMainThread());

      if (!mCallback->IsCurrentTask()) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      nsISpeechTask* task = mCallback->mTask;

      if (mFirstData) {
        task->Setup(mCallback, PICO_CHANNELS_NUM, PICO_SAMPLE_RATE, 2);
      }

      return task->SendAudioNative(
        mBufferSize ? static_cast<short*>(mBuffer->Data()) : nullptr, mBufferSize / 2);
    }

  private:
    nsRefPtr<SharedBuffer> mBuffer;

    size_t mBufferSize;

    bool mFirstData;

    nsRefPtr<PicoCallbackRunnable> mCallback;
  };

  nsCOMPtr<nsIRunnable> sendEvent =
    new PicoSynthDataRunnable(aBuffer, aBufferSize, mFirstData, this);
  NS_DispatchToMainThread(sendEvent);
  mFirstData = false;
}

// nsISpeechTaskCallback

NS_IMETHODIMP
PicoCallbackRunnable::OnPause()
{
  return NS_OK;
}

NS_IMETHODIMP
PicoCallbackRunnable::OnResume()
{
  return NS_OK;
}

NS_IMETHODIMP
PicoCallbackRunnable::OnCancel()
{
  mService->mCurrentTask = nullptr;
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsPicoService)
  NS_INTERFACE_MAP_ENTRY(nsISpeechService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsPicoService)
NS_IMPL_RELEASE(nsPicoService)

nsPicoService::nsPicoService()
  : mInitialized(false)
  , mVoicesMonitor("nsPicoService::mVoices")
  , mCurrentTask(nullptr)
  , mPicoSystem(nullptr)
  , mPicoEngine(nullptr)
  , mSgResource(nullptr)
  , mTaResource(nullptr)
  , mPicoMemArea(nullptr)
{
}

nsPicoService::~nsPicoService()
{
  // We don't worry about removing the voices because this gets
  // destructed at shutdown along with the voice registry.
  MonitorAutoLock autoLock(mVoicesMonitor);
  mVoices.Clear();

  if (mThread) {
    mThread->Shutdown();
  }

  UnloadEngine();
}

// nsIObserver

NS_IMETHODIMP
nsPicoService::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if(NS_WARN_IF(!(!strcmp(aTopic, "profile-after-change")))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!Preferences::GetBool("media.webspeech.synth.enabled") ||
      Preferences::GetBool("media.webspeech.synth.test")) {
    return NS_OK;
  }

  DebugOnly<nsresult> rv = NS_NewNamedThread("Pico Worker", getter_AddRefs(mThread));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return mThread->Dispatch(
    NS_NewRunnableMethod(this, &nsPicoService::Init), NS_DISPATCH_NORMAL);
}
// nsISpeechService

NS_IMETHODIMP
nsPicoService::Speak(const nsAString& aText, const nsAString& aUri,
                     float aVolume, float aRate, float aPitch,
                     nsISpeechTask* aTask)
{
  if(NS_WARN_IF(!(mInitialized))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MonitorAutoLock autoLock(mVoicesMonitor);
  bool found = false;
  PicoVoice* voice = mVoices.GetWeak(aUri, &found);
  if(NS_WARN_IF(!(found))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mCurrentTask = aTask;
  nsRefPtr<PicoCallbackRunnable> cb = new PicoCallbackRunnable(aText, voice, aRate, aPitch, aTask, this);
  return mThread->Dispatch(cb, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
nsPicoService::GetServiceType(SpeechServiceType* aServiceType)
{
  *aServiceType = nsISpeechService::SERVICETYPE_DIRECT_AUDIO;
  return NS_OK;
}

struct VoiceTraverserData
{
  nsPicoService* mService;
  nsSynthVoiceRegistry* mRegistry;
};

// private methods

static PLDHashOperator
PicoAddVoiceTraverser(const nsAString& aUri,
                      nsRefPtr<PicoVoice>& aVoice,
                      void* aUserArg)
{
  // If we are missing either a language or a voice resource, it is invalid.
  if (aVoice->mTaFile.IsEmpty() || aVoice->mSgFile.IsEmpty()) {
    return PL_DHASH_REMOVE;
  }

  VoiceTraverserData* data = static_cast<VoiceTraverserData*>(aUserArg);

  nsAutoString name;
  name.AssignLiteral("Pico ");
  name.Append(aVoice->mLanguage);

  DebugOnly<nsresult> rv =
    data->mRegistry->AddVoice(
      data->mService, aUri, name, aVoice->mLanguage, true);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to add voice");

  return PL_DHASH_NEXT;
}

void
nsPicoService::Init()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);

  if (!sPicoApi.Init()) {
    NS_WARNING("Failed to initialize pico library");
    return;
  }

  // Use environment variable, or default android/b2g path
  nsAutoCString langPath(PR_GetEnv("PICO_LANG_PATH"));

  if (langPath.IsEmpty()) {
    langPath.AssignLiteral(GONK_PICO_LANG_PATH);
  }

  nsCOMPtr<nsIFile> voicesDir;
  NS_NewNativeLocalFile(langPath, true, getter_AddRefs(voicesDir));

  nsCOMPtr<nsISimpleEnumerator> dirIterator;
  nsresult rv = voicesDir->GetDirectoryEntries(getter_AddRefs(dirIterator));

  if (NS_FAILED(rv)) {
    NS_WARNING(nsPrintfCString("Failed to get contents of directory: %s", langPath.get()).get());
    return;
  }

  bool hasMoreElements = false;
  rv = dirIterator->HasMoreElements(&hasMoreElements);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  MonitorAutoLock autoLock(mVoicesMonitor);

  while (hasMoreElements && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISupports> supports;
    rv = dirIterator->GetNext(getter_AddRefs(supports));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    nsCOMPtr<nsIFile> voiceFile = do_QueryInterface(supports);
    MOZ_ASSERT(voiceFile);

    nsAutoCString leafName;
    voiceFile->GetNativeLeafName(leafName);

    nsAutoString lang;

    if (GetVoiceFileLanguage(leafName, lang)) {
      nsAutoString uri;
      uri.AssignLiteral("urn:moz-tts:pico:");
      uri.Append(lang);

      bool found = false;
      PicoVoice* voice = mVoices.GetWeak(uri, &found);

      if (!found) {
        voice = new PicoVoice(lang);
        mVoices.Put(uri, voice);
      }

      // Each voice consists of two lingware files: A language resource file,
      // suffixed by _ta.bin, and a speaker resource file, suffixed by _sb.bin.
      // We currently assume that there is a pair of files for each language.
      if (StringEndsWith(leafName, NS_LITERAL_CSTRING("_ta.bin"))) {
        rv = voiceFile->GetPersistentDescriptor(voice->mTaFile);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      } else if (StringEndsWith(leafName, NS_LITERAL_CSTRING("_sg.bin"))) {
        rv = voiceFile->GetPersistentDescriptor(voice->mSgFile);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
    }

    rv = dirIterator->HasMoreElements(&hasMoreElements);
  }

  NS_DispatchToMainThread(NS_NewRunnableMethod(this, &nsPicoService::RegisterVoices));
}

void
nsPicoService::RegisterVoices()
{
  VoiceTraverserData data = { this, nsSynthVoiceRegistry::GetInstance() };
  mVoices.Enumerate(PicoAddVoiceTraverser, &data);

  mInitialized = true;
}

bool
nsPicoService::GetVoiceFileLanguage(const nsACString& aFileName, nsAString& aLang)
{
  nsACString::const_iterator start, end;
  aFileName.BeginReading(start);
  aFileName.EndReading(end);

  // The lingware filename syntax is language_(ta/sg).bin,
  // we extract the language prefix here.
  if (FindInReadable(NS_LITERAL_CSTRING("_"), start, end)) {
    end = start;
    aFileName.BeginReading(start);
    aLang.Assign(NS_ConvertUTF8toUTF16(Substring(start, end)));
    return true;
  }

  return false;
}

void
nsPicoService::LoadEngine(PicoVoice* aVoice)
{
  PicoApi::pico_Status status = 0;

  if (mPicoSystem) {
    UnloadEngine();
  }

  if (!mPicoMemArea) {
    mPicoMemArea = new uint8_t[PICO_MEM_SIZE];
  }

  status = sPicoApi.pico_initialize(mPicoMemArea, PICO_MEM_SIZE, &mPicoSystem);
  PICO_ENSURE_SUCCESS_VOID("pico_initialize", status);

  status = sPicoApi.pico_loadResource(mPicoSystem, aVoice->mTaFile.get(), &mTaResource);
  PICO_ENSURE_SUCCESS_VOID("pico_loadResource", status);

  status = sPicoApi.pico_loadResource(mPicoSystem, aVoice->mSgFile.get(), &mSgResource);
  PICO_ENSURE_SUCCESS_VOID("pico_loadResource", status);

  status = sPicoApi.pico_createVoiceDefinition(mPicoSystem, PICO_VOICE_NAME);
  PICO_ENSURE_SUCCESS_VOID("pico_createVoiceDefinition", status);

  char taName[PICO_RETSTRINGSIZE];
  status = sPicoApi.pico_getResourceName(mPicoSystem, mTaResource, taName);
  PICO_ENSURE_SUCCESS_VOID("pico_getResourceName", status);

  status = sPicoApi.pico_addResourceToVoiceDefinition(
    mPicoSystem, PICO_VOICE_NAME, taName);
  PICO_ENSURE_SUCCESS_VOID("pico_addResourceToVoiceDefinition", status);

  char sgName[PICO_RETSTRINGSIZE];
  status = sPicoApi.pico_getResourceName(mPicoSystem, mSgResource, sgName);
  PICO_ENSURE_SUCCESS_VOID("pico_getResourceName", status);

  status = sPicoApi.pico_addResourceToVoiceDefinition(
    mPicoSystem, PICO_VOICE_NAME, sgName);
  PICO_ENSURE_SUCCESS_VOID("pico_addResourceToVoiceDefinition", status);

  status = sPicoApi.pico_newEngine(mPicoSystem, PICO_VOICE_NAME, &mPicoEngine);
  PICO_ENSURE_SUCCESS_VOID("pico_newEngine", status);

  if (sSingleton) {
    sSingleton->mCurrentVoice = aVoice;
  }
}

void
nsPicoService::UnloadEngine()
{
  PicoApi::pico_Status status = 0;

  if (mPicoEngine) {
    status = sPicoApi.pico_disposeEngine(mPicoSystem, &mPicoEngine);
    PICO_ENSURE_SUCCESS_VOID("pico_disposeEngine", status);
    status = sPicoApi.pico_releaseVoiceDefinition(mPicoSystem, PICO_VOICE_NAME);
    PICO_ENSURE_SUCCESS_VOID("pico_releaseVoiceDefinition", status);
    mPicoEngine = nullptr;
  }

  if (mSgResource) {
    status = sPicoApi.pico_unloadResource(mPicoSystem, &mSgResource);
    PICO_ENSURE_SUCCESS_VOID("pico_unloadResource", status);
    mSgResource = nullptr;
  }

  if (mTaResource) {
    status = sPicoApi.pico_unloadResource(mPicoSystem, &mTaResource);
    PICO_ENSURE_SUCCESS_VOID("pico_unloadResource", status);
    mTaResource = nullptr;
  }

  if (mPicoSystem) {
    status = sPicoApi.pico_terminate(&mPicoSystem);
    PICO_ENSURE_SUCCESS_VOID("pico_terminate", status);
    mPicoSystem = nullptr;
  }
}

PicoVoice*
nsPicoService::CurrentVoice()
{
  MOZ_ASSERT(!NS_IsMainThread());

  return mCurrentVoice;
}

// static methods

nsPicoService*
nsPicoService::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!XRE_IsParentProcess()) {
    MOZ_ASSERT(false, "nsPicoService can only be started on main gecko process");
    return nullptr;
  }

  if (!sSingleton) {
    sSingleton = new nsPicoService();
  }

  return sSingleton;
}

already_AddRefed<nsPicoService>
nsPicoService::GetInstanceForService()
{
  nsRefPtr<nsPicoService> picoService = GetInstance();
  return picoService.forget();
}

void
nsPicoService::Shutdown()
{
  if (!sSingleton) {
    return;
  }

  sSingleton->mCurrentTask = nullptr;

  sSingleton = nullptr;
}

} // namespace dom
} // namespace mozilla
