/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognition.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SpeechRecognitionBinding.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/SpeechGrammar.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/AbstractThread.h"
#include "VideoUtils.h"
#include "AudioSegment.h"
#include "MediaEnginePrefs.h"
#include "endpointer.h"

#include "mozilla/dom/SpeechRecognitionEvent.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsQueryObject.h"
#include "SpeechTrackListener.h"

#include <algorithm>

// Undo the windows.h damage
#if defined(XP_WIN) && defined(GetMessage)
#  undef GetMessage
#endif

namespace mozilla::dom {

#define PREFERENCE_DEFAULT_RECOGNITION_SERVICE "media.webspeech.service.default"
#define DEFAULT_RECOGNITION_SERVICE "online"

#define PREFERENCE_ENDPOINTER_SILENCE_LENGTH "media.webspeech.silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SILENCE_LENGTH \
  "media.webspeech.long_silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SPEECH_LENGTH \
  "media.webspeech.long_speech_length"
#define PREFERENCE_SPEECH_DETECTION_TIMEOUT_MS \
  "media.webspeech.recognition.timeout"

static const uint32_t kSAMPLE_RATE = 16000;

// number of frames corresponding to 300ms of audio to send to endpointer while
// it's in environment estimation mode
// kSAMPLE_RATE frames = 1s, kESTIMATION_FRAMES frames = 300ms
static const uint32_t kESTIMATION_SAMPLES = 300 * kSAMPLE_RATE / 1000;

LogModule* GetSpeechRecognitionLog() {
  static LazyLogModule sLog("SpeechRecognition");
  return sLog;
}
#define SR_LOG(...) \
  MOZ_LOG(GetSpeechRecognitionLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

namespace {
class SpeechRecognitionShutdownBlocker : public media::ShutdownBlocker {
 public:
  SpeechRecognitionShutdownBlocker(SpeechRecognition* aRecognition,
                                   const nsString& aName)
      : media::ShutdownBlocker(aName), mRecognition(aRecognition) {}

  NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override {
    MOZ_ASSERT(NS_IsMainThread());
    // AbortSilently will eventually clear the blocker.
    mRecognition->Abort();
    return NS_OK;
  }

 private:
  const RefPtr<SpeechRecognition> mRecognition;
};

enum class ServiceCreationError {
  ServiceNotFound,
};

Result<nsCOMPtr<nsISpeechRecognitionService>, ServiceCreationError>
CreateSpeechRecognitionService(nsPIDOMWindowInner* aWindow,
                               SpeechRecognition* aRecognition,
                               const nsAString& aLang) {
  nsAutoCString speechRecognitionServiceCID;

  nsAutoCString prefValue;
  Preferences::GetCString(PREFERENCE_DEFAULT_RECOGNITION_SERVICE, prefValue);
  nsAutoCString speechRecognitionService;

  if (!prefValue.IsEmpty()) {
    speechRecognitionService = prefValue;
  } else {
    speechRecognitionService = DEFAULT_RECOGNITION_SERVICE;
  }

  if (StaticPrefs::media_webspeech_test_fake_recognition_service()) {
    speechRecognitionServiceCID =
        NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX "fake";
  } else {
    speechRecognitionServiceCID =
        nsLiteralCString(NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX) +
        speechRecognitionService;
  }

  nsresult rv;
  nsCOMPtr<nsISpeechRecognitionService> recognitionService;
  recognitionService =
      do_CreateInstance(speechRecognitionServiceCID.get(), &rv);
  if (!recognitionService) {
    return Err(ServiceCreationError::ServiceNotFound);
  }

  return recognitionService;
}
}  // namespace

NS_IMPL_CYCLE_COLLECTION_WEAK_PTR_INHERITED(SpeechRecognition,
                                            DOMEventTargetHelper, mStream,
                                            mTrack, mRecognitionService,
                                            mSpeechGrammarList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechRecognition)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechRecognition, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechRecognition, DOMEventTargetHelper)

SpeechRecognition::SpeechRecognition(nsPIDOMWindowInner* aOwnerWindow)
    : DOMEventTargetHelper(aOwnerWindow),
      mEndpointer(kSAMPLE_RATE),
      mAudioSamplesPerChunk(mEndpointer.FrameSize()),
      mSpeechDetectionTimer(NS_NewTimer()),
      mSpeechGrammarList(new SpeechGrammarList(GetOwner())),
      mContinuous(false),
      mInterimResults(false),
      mMaxAlternatives(1) {
  SR_LOG("created SpeechRecognition");

  if (StaticPrefs::media_webspeech_test_enable()) {
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

  mSpeechDetectionTimeoutMs =
      Preferences::GetInt(PREFERENCE_SPEECH_DETECTION_TIMEOUT_MS, 10000);

  Reset();
}

SpeechRecognition::~SpeechRecognition() = default;

bool SpeechRecognition::StateBetween(FSMState begin, FSMState end) {
  return mCurrentState >= begin && mCurrentState <= end;
}

void SpeechRecognition::SetState(FSMState state) {
  mCurrentState = state;
  SR_LOG("Transitioned to state %s", GetName(mCurrentState));
}

JSObject* SpeechRecognition::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return SpeechRecognition_Binding::Wrap(aCx, this, aGivenProto);
}

bool SpeechRecognition::IsAuthorized(JSContext* aCx, JSObject* aGlobal) {
  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(aGlobal);

  nsresult rv;
  nsCOMPtr<nsIPermissionManager> mgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  uint32_t speechRecognition = nsIPermissionManager::UNKNOWN_ACTION;
  rv = mgr->TestExactPermissionFromPrincipal(principal, "speech-recognition"_ns,
                                             &speechRecognition);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  bool hasPermission =
      (speechRecognition == nsIPermissionManager::ALLOW_ACTION);

  return (hasPermission ||
          StaticPrefs::media_webspeech_recognition_force_enable() ||
          StaticPrefs::media_webspeech_test_enable()) &&
         StaticPrefs::media_webspeech_recognition_enable();
}

already_AddRefed<SpeechRecognition> SpeechRecognition::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal.GetAsSupports());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<SpeechRecognition> object = new SpeechRecognition(win);
  return object.forget();
}

void SpeechRecognition::ProcessEvent(SpeechEvent* aEvent) {
  SR_LOG("Processing %s, current state is %s", GetName(aEvent),
         GetName(mCurrentState));

  if (mAborted && aEvent->mType != EVENT_ABORT) {
    // ignore all events while aborting
    return;
  }

  Transition(aEvent);
}

void SpeechRecognition::Transition(SpeechEvent* aEvent) {
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
        default:
          MOZ_CRASH("Invalid event");
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
          ResetAndEnd();
          break;
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          DoNothing(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_STARTING: Unhandled event %s", GetName(aEvent));
          MOZ_CRASH();
        default:
          MOZ_CRASH("Invalid event");
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
        default:
          MOZ_CRASH("Invalid event");
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
        default:
          MOZ_CRASH("Invalid event");
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
        default:
          MOZ_CRASH("Invalid event");
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
          SR_LOG("STATE_WAITING_FOR_RESULT: Unhandled aEvent %s",
                 GetName(aEvent));
          MOZ_CRASH();
        default:
          MOZ_CRASH("Invalid event");
      }
      break;
    case STATE_ABORTING:
      switch (aEvent->mType) {
        case EVENT_STOP:
        case EVENT_ABORT:
        case EVENT_AUDIO_DATA:
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          DoNothing(aEvent);
          break;
        case EVENT_START:
          SR_LOG("STATE_ABORTING: Unhandled aEvent %s", GetName(aEvent));
          MOZ_CRASH();
        default:
          MOZ_CRASH("Invalid event");
      }
      break;
    default:
      MOZ_CRASH("Invalid state");
  }
}

/*
 * Handle a segment of recorded audio data.
 * Returns the number of samples that were processed.
 */
uint32_t SpeechRecognition::ProcessAudioSegment(AudioSegment* aSegment,
                                                TrackRate aTrackRate) {
  AudioSegment::ChunkIterator iterator(*aSegment);
  uint32_t samples = 0;
  while (!iterator.IsEnded()) {
    float out;
    mEndpointer.ProcessAudio(*iterator, &out);
    samples += iterator->GetDuration();
    iterator.Next();
  }

  // we need to call the nsISpeechRecognitionService::ProcessAudioSegment
  // in a separate thread so that any eventual encoding or pre-processing
  // of the audio does not block the main thread
  nsresult rv = mEncodeTaskQueue->Dispatch(
      NewRunnableMethod<StoreCopyPassByPtr<AudioSegment>, TrackRate>(
          "nsISpeechRecognitionService::ProcessAudioSegment",
          mRecognitionService,
          &nsISpeechRecognitionService::ProcessAudioSegment,
          std::move(*aSegment), aTrackRate));
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
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

void SpeechRecognition::Reset() {
  SetState(STATE_IDLE);

  // This breaks potential ref-cycles.
  mRecognitionService = nullptr;

  ++mStreamGeneration;
  if (mStream) {
    mStream->UnregisterTrackListener(this);
    mStream = nullptr;
  }
  mTrack = nullptr;
  mTrackIsOwned = false;
  mStopRecordingPromise = nullptr;
  mEncodeTaskQueue = nullptr;
  mEstimationSamples = 0;
  mBufferedSamples = 0;
  mSpeechDetectionTimer->Cancel();
  mAborted = false;
}

void SpeechRecognition::ResetAndEnd() {
  Reset();
  DispatchTrustedEvent(u"end"_ns);
}

void SpeechRecognition::WaitForAudioData(SpeechEvent* aEvent) {
  SetState(STATE_STARTING);
}

void SpeechRecognition::StartedAudioCapture(SpeechEvent* aEvent) {
  SetState(STATE_ESTIMATING);

  mEndpointer.SetEnvironmentEstimationMode();
  mEstimationSamples +=
      ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);

  DispatchTrustedEvent(u"audiostart"_ns);
  if (mCurrentState == STATE_ESTIMATING) {
    DispatchTrustedEvent(u"start"_ns);
  }
}

void SpeechRecognition::StopRecordingAndRecognize(SpeechEvent* aEvent) {
  SetState(STATE_WAITING_FOR_RESULT);

  MOZ_ASSERT(mRecognitionService, "Service deleted before recording done");

  // This will run SoundEnd on the service just before StopRecording begins
  // shutting the encode thread down.
  mSpeechListener->mRemovedPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [service = mRecognitionService] { service->SoundEnd(); });

  StopRecording();
}

void SpeechRecognition::WaitForEstimation(SpeechEvent* aEvent) {
  SetState(STATE_ESTIMATING);

  mEstimationSamples +=
      ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEstimationSamples > kESTIMATION_SAMPLES) {
    mEndpointer.SetUserInputMode();
    SetState(STATE_WAITING_FOR_SPEECH);
  }
}

void SpeechRecognition::DetectSpeech(SpeechEvent* aEvent) {
  SetState(STATE_WAITING_FOR_SPEECH);

  ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEndpointer.DidStartReceivingSpeech()) {
    mSpeechDetectionTimer->Cancel();
    SetState(STATE_RECOGNIZING);
    DispatchTrustedEvent(u"speechstart"_ns);
  }
}

void SpeechRecognition::WaitForSpeechEnd(SpeechEvent* aEvent) {
  SetState(STATE_RECOGNIZING);

  ProcessAudioSegment(aEvent->mAudioSegment, aEvent->mTrackRate);
  if (mEndpointer.speech_input_complete()) {
    DispatchTrustedEvent(u"speechend"_ns);

    if (mCurrentState == STATE_RECOGNIZING) {
      // FIXME: StopRecordingAndRecognize should only be called for single
      // shot services for continuous we should just inform the service
      StopRecordingAndRecognize(aEvent);
    }
  }
}

void SpeechRecognition::NotifyFinalResult(SpeechEvent* aEvent) {
  ResetAndEnd();

  RootedDictionary<SpeechRecognitionEventInit> init(RootingCx());
  init.mBubbles = true;
  init.mCancelable = false;
  // init.mResultIndex = 0;
  init.mResults = aEvent->mRecognitionResultList;
  init.mInterpretation = JS::NullValue();
  // init.mEmma = nullptr;

  RefPtr<SpeechRecognitionEvent> event =
      SpeechRecognitionEvent::Constructor(this, u"result"_ns, init);
  event->SetTrusted(true);

  DispatchEvent(*event);
}

void SpeechRecognition::DoNothing(SpeechEvent* aEvent) {}

void SpeechRecognition::AbortSilently(SpeechEvent* aEvent) {
  if (mRecognitionService) {
    if (mTrack) {
      // This will run Abort on the service just before StopRecording begins
      // shutting the encode thread down.
      mSpeechListener->mRemovedPromise->Then(
          GetCurrentSerialEventTarget(), __func__,
          [service = mRecognitionService] { service->Abort(); });
    } else {
      // Recording hasn't started yet. We can just call Abort().
      mRecognitionService->Abort();
    }
  }

  StopRecording()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr<SpeechRecognition>(this), this] { ResetAndEnd(); });

  SetState(STATE_ABORTING);
}

void SpeechRecognition::AbortError(SpeechEvent* aEvent) {
  AbortSilently(aEvent);
  NotifyError(aEvent);
}

void SpeechRecognition::NotifyError(SpeechEvent* aEvent) {
  aEvent->mError->SetTrusted(true);

  DispatchEvent(*aEvent->mError);
}

/**************************************
 * Event triggers and other functions *
 **************************************/
NS_IMETHODIMP
SpeechRecognition::StartRecording(RefPtr<AudioStreamTrack>& aTrack) {
  // hold a reference so that the underlying track doesn't get collected.
  mTrack = aTrack;
  MOZ_ASSERT(!mTrack->Ended());

  mSpeechListener = new SpeechTrackListener(this);
  mTrack->AddListener(mSpeechListener);

  nsString blockerName;
  blockerName.AppendPrintf("SpeechRecognition %p shutdown", this);
  mShutdownBlocker =
      MakeAndAddRef<SpeechRecognitionShutdownBlocker>(this, blockerName);
  media::MustGetShutdownBarrier()->AddBlocker(
      mShutdownBlocker, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
      u"SpeechRecognition shutdown"_ns);

  mEndpointer.StartSession();

  return mSpeechDetectionTimer->Init(this, mSpeechDetectionTimeoutMs,
                                     nsITimer::TYPE_ONE_SHOT);
}

RefPtr<GenericNonExclusivePromise> SpeechRecognition::StopRecording() {
  if (!mTrack) {
    // Recording wasn't started, or has already been stopped.
    if (mStream) {
      // Ensure we don't start recording because a track became available
      // before we get reset.
      mStream->UnregisterTrackListener(this);
    }
    return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  }

  if (mStopRecordingPromise) {
    return mStopRecordingPromise;
  }

  mTrack->RemoveListener(mSpeechListener);
  if (mTrackIsOwned) {
    mTrack->Stop();
  }

  mEndpointer.EndSession();
  DispatchTrustedEvent(u"audioend"_ns);

  // Block shutdown until the speech track listener has been removed from the
  // MSG, as it holds a reference to us, and we reference the world, which we
  // don't want to leak.
  mStopRecordingPromise =
      mSpeechListener->mRemovedPromise
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [self = RefPtr<SpeechRecognition>(this), this] {
                SR_LOG("Shutting down encoding thread");
                return mEncodeTaskQueue->BeginShutdown();
              },
              [] {
                MOZ_CRASH("Unexpected rejection");
                return ShutdownPromise::CreateAndResolve(false, __func__);
              })
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [self = RefPtr<SpeechRecognition>(this), this] {
                media::MustGetShutdownBarrier()->RemoveBlocker(
                    mShutdownBlocker);
                mShutdownBlocker = nullptr;

                MOZ_DIAGNOSTIC_ASSERT(mCurrentState != STATE_IDLE);
                return GenericNonExclusivePromise::CreateAndResolve(true,
                                                                    __func__);
              },
              [] {
                MOZ_CRASH("Unexpected rejection");
                return GenericNonExclusivePromise::CreateAndResolve(false,
                                                                    __func__);
              });
  return mStopRecordingPromise;
}

NS_IMETHODIMP
SpeechRecognition::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread(), "Observer invoked off the main thread");

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) &&
      StateBetween(STATE_IDLE, STATE_WAITING_FOR_SPEECH)) {
    DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR,
                  SpeechRecognitionErrorCode::No_speech,
                  "No speech detected (timeout)");
  } else if (!strcmp(aTopic, SPEECH_RECOGNITION_TEST_END_TOPIC)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC);
    obs->RemoveObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC);
  } else if (StaticPrefs::media_webspeech_test_fake_fsm_events() &&
             !strcmp(aTopic, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC)) {
    ProcessTestEventRequest(aSubject, nsDependentString(aData));
  }

  return NS_OK;
}

void SpeechRecognition::ProcessTestEventRequest(nsISupports* aSubject,
                                                const nsAString& aEventName) {
  if (aEventName.EqualsLiteral("EVENT_ABORT")) {
    Abort();
  } else if (aEventName.EqualsLiteral("EVENT_AUDIO_ERROR")) {
    DispatchError(
        SpeechRecognition::EVENT_AUDIO_ERROR,
        SpeechRecognitionErrorCode::Audio_capture,  // TODO different codes?
        "AUDIO_ERROR test event");
  } else {
    NS_ASSERTION(StaticPrefs::media_webspeech_test_fake_recognition_service(),
                 "Got request for fake recognition service event, but "
                 "media.webspeech.test.fake_recognition_service is unset");

    // let the fake recognition service handle the request
  }
}

already_AddRefed<SpeechGrammarList> SpeechRecognition::Grammars() const {
  RefPtr<SpeechGrammarList> speechGrammarList = mSpeechGrammarList;
  return speechGrammarList.forget();
}

void SpeechRecognition::SetGrammars(SpeechGrammarList& aArg) {
  mSpeechGrammarList = &aArg;
}

void SpeechRecognition::GetLang(nsString& aRetVal) const { aRetVal = mLang; }

void SpeechRecognition::SetLang(const nsAString& aArg) { mLang = aArg; }

bool SpeechRecognition::GetContinuous(ErrorResult& aRv) const {
  return mContinuous;
}

void SpeechRecognition::SetContinuous(bool aArg, ErrorResult& aRv) {
  mContinuous = aArg;
}

bool SpeechRecognition::InterimResults() const { return mInterimResults; }

void SpeechRecognition::SetInterimResults(bool aArg) { mInterimResults = aArg; }

uint32_t SpeechRecognition::MaxAlternatives() const { return mMaxAlternatives; }

void SpeechRecognition::SetMaxAlternatives(uint32_t aArg) {
  mMaxAlternatives = aArg;
}

void SpeechRecognition::GetServiceURI(nsString& aRetVal,
                                      ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void SpeechRecognition::SetServiceURI(const nsAString& aArg, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void SpeechRecognition::Start(const Optional<NonNull<DOMMediaStream>>& aStream,
                              CallerType aCallerType, ErrorResult& aRv) {
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

  mEncodeTaskQueue = MakeAndAddRef<TaskQueue>(
      GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER),
      "WebSpeechEncoderThread");

  nsresult rv;
  rv = mRecognitionService->Initialize(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  MediaStreamConstraints constraints;
  constraints.mAudio.SetAsBoolean() = true;

  if (aStream.WasPassed()) {
    mStream = &aStream.Value();
    mTrackIsOwned = false;
    mStream->RegisterTrackListener(this);
    nsTArray<RefPtr<AudioStreamTrack>> tracks;
    mStream->GetAudioTracks(tracks);
    for (const RefPtr<AudioStreamTrack>& track : tracks) {
      if (!track->Ended()) {
        NotifyTrackAdded(track);
        break;
      }
    }
  } else {
    mTrackIsOwned = true;
    nsPIDOMWindowInner* win = GetOwner();
    if (!win || !win->IsFullyActive()) {
      aRv.ThrowInvalidStateError("The document is not fully active.");
      return;
    }
    AutoNoJSAPI nojsapi;
    RefPtr<SpeechRecognition> self(this);
    MediaManager::Get()
        ->GetUserMedia(win, constraints, aCallerType)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [this, self,
             generation = mStreamGeneration](RefPtr<DOMMediaStream>&& aStream) {
              nsTArray<RefPtr<AudioStreamTrack>> tracks;
              aStream->GetAudioTracks(tracks);
              if (mAborted || mCurrentState != STATE_STARTING ||
                  mStreamGeneration != generation) {
                // We were probably aborted. Exit early.
                for (const RefPtr<AudioStreamTrack>& track : tracks) {
                  track->Stop();
                }
                return;
              }
              mStream = std::move(aStream);
              mStream->RegisterTrackListener(this);
              for (const RefPtr<AudioStreamTrack>& track : tracks) {
                if (!track->Ended()) {
                  NotifyTrackAdded(track);
                }
              }
            },
            [this, self,
             generation = mStreamGeneration](RefPtr<MediaMgrError>&& error) {
              if (mAborted || mCurrentState != STATE_STARTING ||
                  mStreamGeneration != generation) {
                // We were probably aborted. Exit early.
                return;
              }
              SpeechRecognitionErrorCode errorCode;

              if (error->mName == MediaMgrError::Name::NotAllowedError) {
                errorCode = SpeechRecognitionErrorCode::Not_allowed;
              } else {
                errorCode = SpeechRecognitionErrorCode::Audio_capture;
              }
              DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR, errorCode,
                            error->mMessage);
            });
  }

  RefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_START);
  NS_DispatchToMainThread(event);
}

bool SpeechRecognition::SetRecognitionService(ErrorResult& aRv) {
  if (!GetOwner()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  // See:
  // https://dvcs.w3.org/hg/speech-api/raw-file/tip/webspeechapi.html#dfn-lang
  nsAutoString lang;
  if (!mLang.IsEmpty()) {
    lang = mLang;
  } else {
    nsCOMPtr<Document> document = GetOwner()->GetExtantDoc();
    if (!document) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return false;
    }
    nsCOMPtr<Element> element = document->GetRootElement();
    if (!element) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return false;
    }

    nsAutoString lang;
    element->GetLang(lang);
  }

  auto result = CreateSpeechRecognitionService(GetOwner(), this, lang);

  if (result.isErr()) {
    switch (result.unwrapErr()) {
      case ServiceCreationError::ServiceNotFound:
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        break;
      default:
        MOZ_CRASH("Unknown error");
    }
    return false;
  }

  mRecognitionService = result.unwrap();
  MOZ_DIAGNOSTIC_ASSERT(mRecognitionService);
  return true;
}

bool SpeechRecognition::ValidateAndSetGrammarList(ErrorResult& aRv) {
  if (!mSpeechGrammarList) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  uint32_t grammarListLength = mSpeechGrammarList->Length();
  for (uint32_t count = 0; count < grammarListLength; ++count) {
    RefPtr<SpeechGrammar> speechGrammar = mSpeechGrammarList->Item(count, aRv);
    if (aRv.Failed()) {
      return false;
    }
    if (NS_FAILED(mRecognitionService->ValidateAndSetGrammarList(
            speechGrammar.get(), nullptr))) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return false;
    }
  }

  return true;
}

void SpeechRecognition::Stop() {
  RefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_STOP);
  NS_DispatchToMainThread(event);
}

void SpeechRecognition::Abort() {
  if (mAborted) {
    return;
  }

  mAborted = true;

  RefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_ABORT);
  NS_DispatchToMainThread(event);
}

void SpeechRecognition::NotifyTrackAdded(
    const RefPtr<MediaStreamTrack>& aTrack) {
  if (mTrack) {
    return;
  }

  RefPtr<AudioStreamTrack> audioTrack = aTrack->AsAudioStreamTrack();
  if (!audioTrack) {
    return;
  }

  if (audioTrack->Ended()) {
    return;
  }

  StartRecording(audioTrack);
}

void SpeechRecognition::DispatchError(EventType aErrorType,
                                      SpeechRecognitionErrorCode aErrorCode,
                                      const nsACString& aMessage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aErrorType == EVENT_RECOGNITIONSERVICE_ERROR ||
                 aErrorType == EVENT_AUDIO_ERROR,
             "Invalid error type!");

  RefPtr<SpeechRecognitionError> srError =
      new SpeechRecognitionError(nullptr, nullptr, nullptr);

  srError->InitSpeechRecognitionError(u"error"_ns, true, false, aErrorCode,
                                      aMessage);

  RefPtr<SpeechEvent> event = new SpeechEvent(this, aErrorType);
  event->mError = srError;
  NS_DispatchToMainThread(event);
}

/*
 * Buffer audio samples into mAudioSamplesBuffer until aBufferSize.
 * Updates mBufferedSamples and returns the number of samples that were
 * buffered.
 */
uint32_t SpeechRecognition::FillSamplesBuffer(const int16_t* aSamples,
                                              uint32_t aSampleCount) {
  MOZ_ASSERT(mBufferedSamples < mAudioSamplesPerChunk);
  MOZ_ASSERT(mAudioSamplesBuffer);

  int16_t* samplesBuffer = static_cast<int16_t*>(mAudioSamplesBuffer->Data());
  size_t samplesToCopy =
      std::min(aSampleCount, mAudioSamplesPerChunk - mBufferedSamples);

  PodCopy(samplesBuffer + mBufferedSamples, aSamples, samplesToCopy);

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
uint32_t SpeechRecognition::SplitSamplesBuffer(
    const int16_t* aSamplesBuffer, uint32_t aSampleCount,
    nsTArray<RefPtr<SharedBuffer>>& aResult) {
  uint32_t chunkStart = 0;

  while (chunkStart + mAudioSamplesPerChunk <= aSampleCount) {
    CheckedInt<size_t> bufferSize(sizeof(int16_t));
    bufferSize *= mAudioSamplesPerChunk;
    RefPtr<SharedBuffer> chunk = SharedBuffer::Create(bufferSize);

    PodCopy(static_cast<short*>(chunk->Data()), aSamplesBuffer + chunkStart,
            mAudioSamplesPerChunk);

    aResult.AppendElement(chunk.forget());
    chunkStart += mAudioSamplesPerChunk;
  }

  return chunkStart;
}

AudioSegment* SpeechRecognition::CreateAudioSegment(
    nsTArray<RefPtr<SharedBuffer>>& aChunks) {
  AudioSegment* segment = new AudioSegment();
  for (uint32_t i = 0; i < aChunks.Length(); ++i) {
    RefPtr<SharedBuffer> buffer = aChunks[i];
    const int16_t* chunkData = static_cast<const int16_t*>(buffer->Data());

    AutoTArray<const int16_t*, 1> channels;
    channels.AppendElement(chunkData);
    segment->AppendFrames(buffer.forget(), channels, mAudioSamplesPerChunk,
                          PRINCIPAL_HANDLE_NONE);
  }

  return segment;
}

void SpeechRecognition::FeedAudioData(already_AddRefed<SharedBuffer> aSamples,
                                      uint32_t aDuration,
                                      MediaTrackListener* aProvider,
                                      TrackRate aTrackRate) {
  NS_ASSERTION(!NS_IsMainThread(),
               "FeedAudioData should not be called in the main thread");

  // Endpointer expects to receive samples in chunks whose size is a
  // multiple of its frame size.
  // Since we can't assume we will receive the frames in appropriate-sized
  // chunks, we must buffer and split them in chunks of mAudioSamplesPerChunk
  // (a multiple of Endpointer's frame size) before feeding to Endpointer.

  // ensure aSamples is deleted
  RefPtr<SharedBuffer> refSamples = aSamples;

  uint32_t samplesIndex = 0;
  const int16_t* samples = static_cast<int16_t*>(refSamples->Data());
  AutoTArray<RefPtr<SharedBuffer>, 5> chunksToSend;

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
                                       aDuration - samplesIndex, chunksToSend);
  }

  // buffer remaining samples
  if (samplesIndex < aDuration) {
    mBufferedSamples = 0;
    CheckedInt<size_t> bufferSize(sizeof(int16_t));
    bufferSize *= mAudioSamplesPerChunk;
    mAudioSamplesBuffer = SharedBuffer::Create(bufferSize);

    FillSamplesBuffer(samples + samplesIndex, aDuration - samplesIndex);
  }

  AudioSegment* segment = CreateAudioSegment(chunksToSend);
  RefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_AUDIO_DATA);
  event->mAudioSegment = segment;
  event->mProvider = aProvider;
  event->mTrackRate = aTrackRate;
  NS_DispatchToMainThread(event);
}

const char* SpeechRecognition::GetName(FSMState aId) {
  static const char* names[] = {
      "STATE_IDLE",        "STATE_STARTING",
      "STATE_ESTIMATING",  "STATE_WAITING_FOR_SPEECH",
      "STATE_RECOGNIZING", "STATE_WAITING_FOR_RESULT",
      "STATE_ABORTING",
  };

  MOZ_ASSERT(aId < STATE_COUNT);
  MOZ_ASSERT(ArrayLength(names) == STATE_COUNT);
  return names[aId];
}

const char* SpeechRecognition::GetName(SpeechEvent* aEvent) {
  static const char* names[] = {"EVENT_START",
                                "EVENT_STOP",
                                "EVENT_ABORT",
                                "EVENT_AUDIO_DATA",
                                "EVENT_AUDIO_ERROR",
                                "EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT",
                                "EVENT_RECOGNITIONSERVICE_FINAL_RESULT",
                                "EVENT_RECOGNITIONSERVICE_ERROR"};

  MOZ_ASSERT(aEvent->mType < EVENT_COUNT);
  MOZ_ASSERT(ArrayLength(names) == EVENT_COUNT);
  return names[aEvent->mType];
}

TaskQueue* SpeechRecognition::GetTaskQueueForEncoding() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mEncodeTaskQueue;
}

SpeechEvent::SpeechEvent(SpeechRecognition* aRecognition,
                         SpeechRecognition::EventType aType)
    : Runnable("dom::SpeechEvent"),
      mAudioSegment(nullptr),
      mRecognitionResultList(nullptr),
      mError(nullptr),
      mRecognition(aRecognition),
      mType(aType),
      mTrackRate(0) {}

SpeechEvent::~SpeechEvent() { delete mAudioSegment; }

NS_IMETHODIMP
SpeechEvent::Run() {
  mRecognition->ProcessEvent(this);
  return NS_OK;
}

}  // namespace mozilla::dom
