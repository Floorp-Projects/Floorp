/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognition.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SpeechRecognitionBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Services.h"

#include "AudioSegment.h"
#include "endpointer.h"

#include "mozilla/dom/SpeechRecognitionEvent.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsQueryObject.h"

#include <algorithm>

// Undo the windows.h damage
#if defined(XP_WIN) && defined(GetMessage)
#undef GetMessage
#endif

namespace mozilla {
namespace dom {

#define PREFERENCE_DEFAULT_RECOGNITION_SERVICE "media.webspeech.service.default"
#define DEFAULT_RECOGNITION_SERVICE_PREFIX "pocketsphinx-"
#define DEFAULT_RECOGNITION_SERVICE "pocketsphinx-en-US"

#define PREFERENCE_ENDPOINTER_SILENCE_LENGTH "media.webspeech.silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SILENCE_LENGTH "media.webspeech.long_silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SPEECH_LENGTH "media.webspeech.long_speech_length"

static const uint32_t kSAMPLE_RATE = 16000;
static const uint32_t kSPEECH_DETECTION_TIMEOUT_MS = 10000;

// number of frames corresponding to 300ms of audio to send to endpointer while
// it's in environment estimation mode
// kSAMPLE_RATE frames = 1s, kESTIMATION_FRAMES frames = 300ms
static const uint32_t kESTIMATION_SAMPLES = 300 * kSAMPLE_RATE / 1000;

PRLogModuleInfo*
GetSpeechRecognitionLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("SpeechRecognition");
  }

  return sLog;
}
#define SR_LOG(...) MOZ_LOG(GetSpeechRecognitionLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

already_AddRefed<nsISpeechRecognitionService>
GetSpeechRecognitionService(const nsAString& aLang)
{
  nsAutoCString speechRecognitionServiceCID;

  nsAdoptingCString prefValue =
  Preferences::GetCString(PREFERENCE_DEFAULT_RECOGNITION_SERVICE);
  nsAutoCString speechRecognitionService;

  if (!aLang.IsEmpty()) {
    speechRecognitionService =
      NS_LITERAL_CSTRING(DEFAULT_RECOGNITION_SERVICE_PREFIX) +
      NS_ConvertUTF16toUTF8(aLang);
  } else if (!prefValue.IsEmpty()) {
    speechRecognitionService = prefValue;
  } else {
    speechRecognitionService = DEFAULT_RECOGNITION_SERVICE;
  }

  if (SpeechRecognition::mTestConfig.mFakeRecognitionService) {
    speechRecognitionServiceCID =
      NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX "fake";
  } else {
    speechRecognitionServiceCID =
      NS_LITERAL_CSTRING(NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX) +
      speechRecognitionService;
  }

  nsresult rv;
  nsCOMPtr<nsISpeechRecognitionService> recognitionService;
  recognitionService = do_GetService(speechRecognitionServiceCID.get(), &rv);
  return recognitionService.forget();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(SpeechRecognition, DOMEventTargetHelper, mDOMStream, mSpeechGrammarList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SpeechRecognition)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechRecognition, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechRecognition, DOMEventTargetHelper)

struct SpeechRecognition::TestConfig SpeechRecognition::mTestConfig;

SpeechRecognition::SpeechRecognition(nsPIDOMWindow* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mEndpointer(kSAMPLE_RATE)
  , mAudioSamplesPerChunk(mEndpointer.FrameSize())
  , mSpeechDetectionTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
  , mSpeechGrammarList(new SpeechGrammarList(GetParentObject()))
  , mInterimResults(false)
  , mMaxAlternatives(1)
{
  SR_LOG("created SpeechRecognition");

  mTestConfig.Init();
  if (mTestConfig.mEnableTests) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);
  }

  mEndpointer.set_speech_input_complete_silence_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_SILENCE_LENGTH, 1250000));
  mEndpointer.set_long_speech_input_complete_silence_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_LONG_SILENCE_LENGTH, 2500000));
  mEndpointer.set_long_speech_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_SILENCE_LENGTH, 3 * 1000000));
  Reset();
}

bool
SpeechRecognition::StateBetween(FSMState begin, FSMState end)
{
  return mCurrentState >= begin && mCurrentState <= end;
}

void
SpeechRecognition::SetState(FSMState state)
{
  mCurrentState = state;
  SR_LOG("Transitioned to state %s", GetName(mCurrentState));
  return;
}

JSObject*
SpeechRecognition::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechRecognitionBinding::Wrap(aCx, this, aGivenProto);
}

bool
SpeechRecognition::IsAuthorized(JSContext* aCx, JSObject* aGlobal)
{
  bool inCertifiedApp = IsInCertifiedApp(aCx, aGlobal);
  bool enableTests = Preferences::GetBool(TEST_PREFERENCE_ENABLE);
  bool enableRecognitionEnable = Preferences::GetBool(TEST_PREFERENCE_RECOGNITION_ENABLE);
  bool enableRecognitionForceEnable = Preferences::GetBool(TEST_PREFERENCE_RECOGNITION_FORCE_ENABLE);
  return (inCertifiedApp || enableRecognitionForceEnable || enableTests) && enableRecognitionEnable;
}

already_AddRefed<SpeechRecognition>
SpeechRecognition::Constructor(const GlobalObject& aGlobal,
                               ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  MOZ_ASSERT(win->IsInnerWindow());
  nsRefPtr<SpeechRecognition> object = new SpeechRecognition(win);
  return object.forget();
}

nsISupports*
SpeechRecognition::GetParentObject() const
{
  return GetOwner();
}

void
SpeechRecognition::ProcessEvent(SpeechEvent* aEvent)
{
  SR_LOG("Processing %s, current state is %s",
         GetName(aEvent),
         GetName(mCurrentState));

  if (mAborted && aEvent->mType != EVENT_ABORT) {
    // ignore all events while aborting
    return;
  }

  Transition(aEvent);
}

void
SpeechRecognition::Transition(SpeechEvent* aEvent)
{
  switch (mCurrentState) {
    case STATE_IDLE:
      switch (aEvent->mType) {
        case EVENT_START:
          // TODO: may want to time out if we wait too long
          // for user to approve
          WaitForAudioData(aEvent);
          break;
        case EVENT_STOP:
        case EVENT_ABORT:
        case EVENT_AUDIO_DATA:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          DoNothing(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_STARTING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          StartedAudioCapture(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_ABORT:
          AbortSilently(aEvent);
          break;
        case EVENT_STOP:
          Reset();
          break;
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          DoNothing(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_STARTING: Unhandled event %s", GetName(aEvent));
          MOZ_CRASH();
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_ESTIMATING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          WaitForEstimation(aEvent);
          break;
        case EVENT_STOP:
          StopRecordingAndRecognize(aEvent);
          break;
        case EVENT_ABORT:
          AbortSilently(aEvent);
          break;
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          DoNothing(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_ESTIMATING: Unhandled event %d", aEvent->mType);
          MOZ_CRASH();
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_WAITING_FOR_SPEECH:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          DetectSpeech(aEvent);
          break;
        case EVENT_STOP:
          StopRecordingAndRecognize(aEvent);
          break;
        case EVENT_ABORT:
          AbortSilently(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          DoNothing(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_STARTING: Unhandled event %s", GetName(aEvent));
          MOZ_CRASH();
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_RECOGNIZING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          WaitForSpeechEnd(aEvent);
          break;
        case EVENT_STOP:
          StopRecordingAndRecognize(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_ABORT:
          AbortSilently(aEvent);
          break;
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
          DoNothing(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_RECOGNIZING: Unhandled aEvent %s", GetName(aEvent));
          MOZ_CRASH();
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_WAITING_FOR_RESULT:
      switch (aEvent->mType) {
        case EVENT_STOP:
          DoNothing(aEvent);
          break;
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          AbortError(aEvent);
          break;
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          NotifyFinalResult(aEvent);
          break;
        case EVENT_AUDIO_DATA:
          DoNothing(aEvent);
          break;
        case EVENT_ABORT:
          AbortSilently(aEvent);
          break;
        case EVENT_START:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
          SR_LOG("STATE_WAITING_FOR_RESULT: Unhandled aEvent %s", GetName(aEvent));
          MOZ_CRASH();
        case EVENT_COUNT:
          MOZ_CRASH("Invalid event EVENT_COUNT");
      }
      break;
    case STATE_COUNT:
      MOZ_CRASH("Invalid state STATE_COUNT");
  }

  return;
}

/*
 * Handle a segment of recorded audio data.
 * Returns the number of samples that were processed.
 */
uint32_t
SpeechRecognition::ProcessAudioSegment(AudioSegment* aSegment, TrackRate aTrackRate)
{
  AudioSegment::ChunkIterator iterator(*aSegment);
  uint32_t samples = 0;
  while (!iterator.IsEnded()) {
    float out;
    mEndpointer.ProcessAudio(*iterator, &out);
    samples += iterator->GetDuration();
    iterator.Next();
  }

  mRecognitionService->ProcessAudioSegment(aSegment, aTrackRate);
  return samples;
}

/****************************************************************************
 * FSM Transition functions
 *
 * If a transition function may cause a DOM event to be fired,
 * it may also be re-entered, since the event handler may cause the
 * event loop to spin and new SpeechEvents to be processed.
 *
 * Rules:
 * 1) These methods should call SetState as soon as possible.
 * 2) If these methods dispatch DOM events, or call methods that dispatch
 * DOM events, that should be done as late as possible.
 * 3) If anything must happen after dispatching a DOM event, make sure
 * the state is still what the method expected it to be.
 ****************************************************************************/

void
SpeechRecognition::Reset()
{
  SetState(STATE_IDLE);
  mRecognitionService = nullptr;
  mEstimationSamples = 0;
  mBufferedSamples = 0;
  mSpeechDetectionTimer->Cancel();
  mAborted = false;
}

void
SpeechRecognition::ResetAndEnd()
{
  Reset();
  DispatchTrustedEvent(NS_LITERAL_STRING("end"));
}

void
SpeechRecognition::WaitForAudioData(SpeechEvent* aEvent)
{
  SetState(STATE_STARTING);
}

void
SpeechRecognition::StartedAudioCapture(SpeechEvent* aEvent)
{
  SetState(STATE_ESTIMATING);

  mEndpointer.SetEnvironmentEstimationMode();
  mEstimationSamples += ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);

  DispatchTrustedEvent(NS_LITERAL_STRING("audiostart"));
  if (mCurrentState == STATE_ESTIMATING) {
    DispatchTrustedEvent(NS_LITERAL_STRING("start"));
  }
}

void
SpeechRecognition::StopRecordingAndRecognize(SpeechEvent* aEvent)
{
  SetState(STATE_WAITING_FOR_RESULT);

  MOZ_ASSERT(mRecognitionService, "Service deleted before recording done");
  mRecognitionService->SoundEnd();

  StopRecording();
}

void
SpeechRecognition::WaitForEstimation(SpeechEvent* aEvent)
{
  SetState(STATE_ESTIMATING);

  mEstimationSamples += ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEstimationSamples > kESTIMATION_SAMPLES) {
    mEndpointer.SetUserInputMode();
    SetState(STATE_WAITING_FOR_SPEECH);
  }
}

void
SpeechRecognition::DetectSpeech(SpeechEvent* aEvent)
{
  SetState(STATE_WAITING_FOR_SPEECH);

  ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEndpointer.DidStartReceivingSpeech()) {
    mSpeechDetectionTimer->Cancel();
    SetState(STATE_RECOGNIZING);
    DispatchTrustedEvent(NS_LITERAL_STRING("speechstart"));
  }
}

void
SpeechRecognition::WaitForSpeechEnd(SpeechEvent* aEvent)
{
  SetState(STATE_RECOGNIZING);

  ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEndpointer.speech_input_complete()) {
    DispatchTrustedEvent(NS_LITERAL_STRING("speechend"));

    if (mCurrentState == STATE_RECOGNIZING) {
      // FIXME: StopRecordingAndRecognize should only be called for single
      // shot services for continuous we should just inform the service
      StopRecordingAndRecognize(aEvent);
    }
  }
}

void
SpeechRecognition::NotifyFinalResult(SpeechEvent* aEvent)
{
  ResetAndEnd();

  RootedDictionary<SpeechRecognitionEventInit> init(nsContentUtils::RootingCx());
  init.mBubbles = true;
  init.mCancelable = false;
  // init.mResultIndex = 0;
  init.mResults = aEvent->mRecognitionResultList;
  init.mInterpretation = JS::NullValue();
  // init.mEmma = nullptr;

  nsRefPtr<SpeechRecognitionEvent> event =
    SpeechRecognitionEvent::Constructor(this, NS_LITERAL_STRING("result"), init);
  event->SetTrusted(true);

  bool defaultActionEnabled;
  this->DispatchEvent(event, &defaultActionEnabled);
}

void
SpeechRecognition::DoNothing(SpeechEvent* aEvent)
{
}

void
SpeechRecognition::AbortSilently(SpeechEvent* aEvent)
{
  if (mRecognitionService) {
    mRecognitionService->Abort();
  }

  if (mDOMStream) {
    StopRecording();
  }

  ResetAndEnd();
}

void
SpeechRecognition::AbortError(SpeechEvent* aEvent)
{
  AbortSilently(aEvent);
  NotifyError(aEvent);
}

void
SpeechRecognition::NotifyError(SpeechEvent* aEvent)
{
  aEvent->mError->SetTrusted(true);

  bool defaultActionEnabled;
  this->DispatchEvent(aEvent->mError, &defaultActionEnabled);

  return;
}

/**************************************
 * Event triggers and other functions *
 **************************************/
NS_IMETHODIMP
SpeechRecognition::StartRecording(DOMMediaStream* aDOMStream)
{
  // hold a reference so that the underlying stream
  // doesn't get Destroy()'ed
  mDOMStream = aDOMStream;

  if (NS_WARN_IF(!mDOMStream->GetPlaybackStream())) {
    return NS_ERROR_UNEXPECTED;
  }
  mSpeechListener = new SpeechStreamListener(this);
  mDOMStream->GetPlaybackStream()->AddListener(mSpeechListener);

  mEndpointer.StartSession();

  return mSpeechDetectionTimer->Init(this, kSPEECH_DETECTION_TIMEOUT_MS,
                                     nsITimer::TYPE_ONE_SHOT);
}

NS_IMETHODIMP
SpeechRecognition::StopRecording()
{
  // we only really need to remove the listener explicitly when testing,
  // as our JS code still holds a reference to mDOMStream and only assigning
  // it to nullptr isn't guaranteed to free the stream and the listener.
  mDOMStream->GetPlaybackStream()->RemoveListener(mSpeechListener);
  mSpeechListener = nullptr;
  mDOMStream = nullptr;

  mEndpointer.EndSession();
  DispatchTrustedEvent(NS_LITERAL_STRING("audioend"));

  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread(), "Observer invoked off the main thread");

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) &&
      StateBetween(STATE_IDLE, STATE_WAITING_FOR_SPEECH)) {

    DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR,
                  SpeechRecognitionErrorCode::No_speech,
                  NS_LITERAL_STRING("No speech detected (timeout)"));
  } else if (!strcmp(aTopic, SPEECH_RECOGNITION_TEST_END_TOPIC)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC);
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC);
  } else if (mTestConfig.mFakeFSMEvents &&
             !strcmp(aTopic, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC)) {
    ProcessTestEventRequest(aSubject, nsDependentString(aData));
  }

  return NS_OK;
}

void
SpeechRecognition::ProcessTestEventRequest(nsISupports* aSubject, const nsAString& aEventName)
{
  if (aEventName.EqualsLiteral("EVENT_ABORT")) {
    Abort();
  } else if (aEventName.EqualsLiteral("EVENT_AUDIO_ERROR")) {
    DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR,
                  SpeechRecognitionErrorCode::Audio_capture, // TODO different codes?
                  NS_LITERAL_STRING("AUDIO_ERROR test event"));
  } else {
    NS_ASSERTION(mTestConfig.mFakeRecognitionService,
                 "Got request for fake recognition service event, but "
                 TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE " is unset");

    // let the fake recognition service handle the request
  }

  return;
}

already_AddRefed<SpeechGrammarList>
SpeechRecognition::Grammars() const
{
  nsRefPtr<SpeechGrammarList> speechGrammarList = mSpeechGrammarList;
  return speechGrammarList.forget();
}

void
SpeechRecognition::SetGrammars(SpeechGrammarList& aArg)
{
  mSpeechGrammarList = &aArg;
}

void
SpeechRecognition::GetLang(nsString& aRetVal) const
{
  aRetVal = mLang;
}

void
SpeechRecognition::SetLang(const nsAString& aArg)
{
  mLang = aArg;
}

bool
SpeechRecognition::GetContinuous(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return false;
}

void
SpeechRecognition::SetContinuous(bool aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

bool
SpeechRecognition::InterimResults() const
{
  return mInterimResults;
}

void
SpeechRecognition::SetInterimResults(bool aArg)
{
  mInterimResults = aArg;
  return;
}

uint32_t
SpeechRecognition::MaxAlternatives() const
{
  return mMaxAlternatives;
}

void
SpeechRecognition::SetMaxAlternatives(uint32_t aArg)
{
  mMaxAlternatives = aArg;
  return;
}

void
SpeechRecognition::GetServiceURI(nsString& aRetVal, ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechRecognition::SetServiceURI(const nsAString& aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechRecognition::Start(const Optional<NonNull<DOMMediaStream>>& aStream, ErrorResult& aRv)
{
  if (mCurrentState != STATE_IDLE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!SetRecognitionService(aRv)) {
    return;
  }

  if (!ValidateAndSetGrammarList(aRv)) {
    return;
  }

  nsresult rv;
  rv = mRecognitionService->Initialize(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MediaStreamConstraints constraints;
  constraints.mAudio.SetAsBoolean() = true;

  if (aStream.WasPassed()) {
    StartRecording(&aStream.Value());
  } else {
    AutoNoJSAPI();
    MediaManager* manager = MediaManager::Get();
    manager->GetUserMedia(GetOwner(),
                          constraints,
                          new GetUserMediaSuccessCallback(this),
                          new GetUserMediaErrorCallback(this));
  }

  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_START);
  NS_DispatchToMainThread(event);
}

bool
SpeechRecognition::SetRecognitionService(ErrorResult& aRv)
{
  // See: https://dvcs.w3.org/hg/speech-api/raw-file/tip/webspeechapi.html#dfn-lang
  if (!mLang.IsEmpty()) {
    mRecognitionService = GetSpeechRecognitionService(mLang);

    if (!mRecognitionService) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return false;
    }

    return true;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetOwner();
  if(!window) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }
  nsCOMPtr<nsIDocument> document = window->GetExtantDoc();
  if(!document) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }
  nsCOMPtr<Element> element = document->GetRootElement();
  if(!element) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsAutoString lang;
  element->GetLang(lang);
  mRecognitionService = GetSpeechRecognitionService(lang);

  if (!mRecognitionService) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  return true;
}

bool
SpeechRecognition::ValidateAndSetGrammarList(ErrorResult& aRv)
{
  if (!mSpeechGrammarList) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  uint32_t grammarListLength = mSpeechGrammarList->Length();
  if (0 == grammarListLength) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  for (uint32_t count = 0; count < grammarListLength; ++count) {
    nsRefPtr<SpeechGrammar> speechGrammar = mSpeechGrammarList->Item(count, aRv);
    if (aRv.Failed()) {
      return false;
    }
    if (NS_FAILED(mRecognitionService->ValidateAndSetGrammarList(speechGrammar.get(), nullptr))) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return false;
    }
  }

  return true;
}

void
SpeechRecognition::Stop()
{
  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_STOP);
  NS_DispatchToMainThread(event);
}

void
SpeechRecognition::Abort()
{
  if (mAborted) {
    return;
  }

  mAborted = true;
  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_ABORT);
  NS_DispatchToMainThread(event);
}

void
SpeechRecognition::DispatchError(EventType aErrorType,
                                 SpeechRecognitionErrorCode aErrorCode,
                                 const nsAString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aErrorType == EVENT_RECOGNITIONSERVICE_ERROR ||
             aErrorType == EVENT_AUDIO_ERROR, "Invalid error type!");

  nsRefPtr<SpeechRecognitionError> srError =
    new SpeechRecognitionError(nullptr, nullptr, nullptr);

  ErrorResult err;
  srError->InitSpeechRecognitionError(NS_LITERAL_STRING("error"), true, false,
                                      aErrorCode, aMessage, err);

  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, aErrorType);
  event->mError = srError;
  NS_DispatchToMainThread(event);
}

/*
 * Buffer audio samples into mAudioSamplesBuffer until aBufferSize.
 * Updates mBufferedSamples and returns the number of samples that were buffered.
 */
uint32_t
SpeechRecognition::FillSamplesBuffer(const int16_t* aSamples,
                                     uint32_t aSampleCount)
{
  MOZ_ASSERT(mBufferedSamples < mAudioSamplesPerChunk);
  MOZ_ASSERT(mAudioSamplesBuffer.get());

  int16_t* samplesBuffer = static_cast<int16_t*>(mAudioSamplesBuffer->Data());
  size_t samplesToCopy = std::min(aSampleCount,
                                  mAudioSamplesPerChunk - mBufferedSamples);

  memcpy(samplesBuffer + mBufferedSamples, aSamples,
         samplesToCopy * sizeof(int16_t));

  mBufferedSamples += samplesToCopy;
  return samplesToCopy;
}

/*
 * Split a samples buffer starting of a given size into
 * chunks of equal size. The chunks are stored in the array
 * received as argument.
 * Returns the offset of the end of the last chunk that was
 * created.
 */
uint32_t
SpeechRecognition::SplitSamplesBuffer(const int16_t* aSamplesBuffer,
                                      uint32_t aSampleCount,
                                      nsTArray<nsRefPtr<SharedBuffer>>& aResult)
{
  uint32_t chunkStart = 0;

  while (chunkStart + mAudioSamplesPerChunk <= aSampleCount) {
    nsRefPtr<SharedBuffer> chunk =
      SharedBuffer::Create(mAudioSamplesPerChunk * sizeof(int16_t));

    memcpy(chunk->Data(), aSamplesBuffer + chunkStart,
           mAudioSamplesPerChunk * sizeof(int16_t));

    aResult.AppendElement(chunk.forget());
    chunkStart += mAudioSamplesPerChunk;
  }

  return chunkStart;
}

AudioSegment*
SpeechRecognition::CreateAudioSegment(nsTArray<nsRefPtr<SharedBuffer>>& aChunks)
{
  AudioSegment* segment = new AudioSegment();
  for (uint32_t i = 0; i < aChunks.Length(); ++i) {
    nsRefPtr<SharedBuffer> buffer = aChunks[i];
    const int16_t* chunkData = static_cast<const int16_t*>(buffer->Data());

    nsAutoTArray<const int16_t*, 1> channels;
    channels.AppendElement(chunkData);
    segment->AppendFrames(buffer.forget(), channels, mAudioSamplesPerChunk);
  }

  return segment;
}

void
SpeechRecognition::FeedAudioData(already_AddRefed<SharedBuffer> aSamples,
                                 uint32_t aDuration,
                                 MediaStreamListener* aProvider, TrackRate aTrackRate)
{
  NS_ASSERTION(!NS_IsMainThread(),
               "FeedAudioData should not be called in the main thread");

  // Endpointer expects to receive samples in chunks whose size is a
  // multiple of its frame size.
  // Since we can't assume we will receive the frames in appropriate-sized
  // chunks, we must buffer and split them in chunks of mAudioSamplesPerChunk
  // (a multiple of Endpointer's frame size) before feeding to Endpointer.

  // ensure aSamples is deleted
  nsRefPtr<SharedBuffer> refSamples = aSamples;

  uint32_t samplesIndex = 0;
  const int16_t* samples = static_cast<int16_t*>(refSamples->Data());
  nsAutoTArray<nsRefPtr<SharedBuffer>, 5> chunksToSend;

  // fill up our buffer and make a chunk out of it, if possible
  if (mBufferedSamples > 0) {
    samplesIndex += FillSamplesBuffer(samples, aDuration);

    if (mBufferedSamples == mAudioSamplesPerChunk) {
      chunksToSend.AppendElement(mAudioSamplesBuffer.forget());
      mBufferedSamples = 0;
    }
  }

  // create sample chunks of correct size
  if (samplesIndex < aDuration) {
    samplesIndex += SplitSamplesBuffer(samples + samplesIndex,
                                       aDuration - samplesIndex,
                                       chunksToSend);
  }

  // buffer remaining samples
  if (samplesIndex < aDuration) {
    mBufferedSamples = 0;
    mAudioSamplesBuffer =
      SharedBuffer::Create(mAudioSamplesPerChunk * sizeof(int16_t));

    FillSamplesBuffer(samples + samplesIndex, aDuration - samplesIndex);
  }

  AudioSegment* segment = CreateAudioSegment(chunksToSend);
  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_AUDIO_DATA);
  event->mAudioSegment = segment;
  event->mProvider = aProvider;
  event->mTrackRate = aTrackRate;
  NS_DispatchToMainThread(event);

  return;
}

const char*
SpeechRecognition::GetName(FSMState aId)
{
  static const char* names[] = {
    "STATE_IDLE",
    "STATE_STARTING",
    "STATE_ESTIMATING",
    "STATE_WAITING_FOR_SPEECH",
    "STATE_RECOGNIZING",
    "STATE_WAITING_FOR_RESULT",
  };

  MOZ_ASSERT(aId < STATE_COUNT);
  MOZ_ASSERT(ArrayLength(names) == STATE_COUNT);
  return names[aId];
}

const char*
SpeechRecognition::GetName(SpeechEvent* aEvent)
{
  static const char* names[] = {
    "EVENT_START",
    "EVENT_STOP",
    "EVENT_ABORT",
    "EVENT_AUDIO_DATA",
    "EVENT_AUDIO_ERROR",
    "EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT",
    "EVENT_RECOGNITIONSERVICE_FINAL_RESULT",
    "EVENT_RECOGNITIONSERVICE_ERROR"
  };

  MOZ_ASSERT(aEvent->mType < EVENT_COUNT);
  MOZ_ASSERT(ArrayLength(names) == EVENT_COUNT);
  return names[aEvent->mType];
}

SpeechEvent::~SpeechEvent()
{
  delete mAudioSegment;
}

NS_IMETHODIMP
SpeechEvent::Run()
{
  mRecognition->ProcessEvent(this);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(SpeechRecognition::GetUserMediaSuccessCallback, nsIDOMGetUserMediaSuccessCallback)

NS_IMETHODIMP
SpeechRecognition::GetUserMediaSuccessCallback::OnSuccess(nsISupports* aStream)
{
  nsRefPtr<DOMMediaStream> stream = do_QueryObject(aStream);
  if (!stream) {
    return NS_ERROR_NO_INTERFACE;
  }
  mRecognition->StartRecording(stream);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(SpeechRecognition::GetUserMediaErrorCallback, nsIDOMGetUserMediaErrorCallback)

NS_IMETHODIMP
SpeechRecognition::GetUserMediaErrorCallback::OnError(nsISupports* aError)
{
  nsRefPtr<MediaStreamError> error = do_QueryObject(aError);
  if (!error) {
    return NS_OK;
  }
  SpeechRecognitionErrorCode errorCode;

  nsAutoString name;
  error->GetName(name);
  if (name.EqualsLiteral("PERMISSION_DENIED")) {
    errorCode = SpeechRecognitionErrorCode::Not_allowed;
  } else {
    errorCode = SpeechRecognitionErrorCode::Audio_capture;
  }

  nsAutoString message;
  error->GetMessage(message);
  mRecognition->DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR, errorCode,
                              message);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
