/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognition.h"

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"

#include "mozilla/dom/SpeechRecognitionBinding.h"

#include "AudioSegment.h"
#include "endpointer.h"

#include "GeneratedEvents.h"
#include "nsIDOMSpeechRecognitionEvent.h"

#include <algorithm>

namespace mozilla {
namespace dom {

#define PREFERENCE_DEFAULT_RECOGNITION_SERVICE "media.webspeech.service.default"
#define DEFAULT_RECOGNITION_SERVICE "google"

#define PREFERENCE_ENDPOINTER_SILENCE_LENGTH "media.webspeech.silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SILENCE_LENGTH "media.webspeech.long_silence_length"
#define PREFERENCE_ENDPOINTER_LONG_SPEECH_LENGTH "media.webspeech.long_speech_length"

static const uint32_t kSAMPLE_RATE = 16000;
static const uint32_t kSPEECH_DETECTION_TIMEOUT_MS = 10000;

// number of frames corresponding to 300ms of audio to send to endpointer while
// it's in environment estimation mode
// kSAMPLE_RATE frames = 1s, kESTIMATION_FRAMES frames = 300ms
static const uint32_t kESTIMATION_SAMPLES = 300 * kSAMPLE_RATE / 1000;

#define STATE_EQUALS(state) (mCurrentState == state)
#define STATE_BETWEEN(state1, state2) \
  (mCurrentState >= (state1) && mCurrentState <= (state2))

#ifdef PR_LOGGING
PRLogModuleInfo*
GetSpeechRecognitionLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("SpeechRecognition");
  }

  return sLog;
}
#define SR_LOG(...) PR_LOG(GetSpeechRecognitionLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define SR_LOG(...)
#endif

NS_INTERFACE_MAP_BEGIN(SpeechRecognition)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechRecognition, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechRecognition, nsDOMEventTargetHelper)

struct SpeechRecognition::TestConfig SpeechRecognition::mTestConfig;

SpeechRecognition::SpeechRecognition()
  : mProcessingEvent(false)
  , mEndpointer(kSAMPLE_RATE)
  , mAudioSamplesPerChunk(mEndpointer.FrameSize())
  , mSpeechDetectionTimer(do_CreateInstance(NS_TIMER_CONTRACTID))
{
  SR_LOG("created SpeechRecognition");
  SetIsDOMBinding();

  mTestConfig.Init();
  if (mTestConfig.mEnableTests) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC, false);
    obs->AddObserver(this, SPEECH_RECOGNITION_TEST_END_TOPIC, false);
  }

  mEndpointer.set_speech_input_complete_silence_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_SILENCE_LENGTH, 500000));
  mEndpointer.set_long_speech_input_complete_silence_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_LONG_SILENCE_LENGTH, 1000000));
  mEndpointer.set_long_speech_length(
      Preferences::GetInt(PREFERENCE_ENDPOINTER_SILENCE_LENGTH, 3 * 1000000));
  mCurrentState = Reset();
}

JSObject*
SpeechRecognition::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return SpeechRecognitionBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<SpeechRecognition>
SpeechRecognition::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.Get());
  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  MOZ_ASSERT(win->IsInnerWindow());
  nsRefPtr<SpeechRecognition> object = new SpeechRecognition();
  object->BindToOwner(win);
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
  SR_LOG("Processing event %d", aEvent->mType);

  MOZ_ASSERT(!mProcessingEvent, "Event dispatch should be sequential!");
  mProcessingEvent = true;

  SR_LOG("Current state: %d", mCurrentState);
  mCurrentState = TransitionAndGetNextState(aEvent);
  SR_LOG("Transitioned to state: %d", mCurrentState);
  mProcessingEvent = false;
}

SpeechRecognition::FSMState
SpeechRecognition::TransitionAndGetNextState(SpeechEvent* aEvent)
{
  switch (mCurrentState) {
    case STATE_IDLE:
      switch (aEvent->mType) {
        case EVENT_START:
          // TODO: may want to time out if we wait too long
          // for user to approve
          return STATE_STARTING;
        case EVENT_STOP:
        case EVENT_ABORT:
        case EVENT_AUDIO_DATA:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          return DoNothing(aEvent);
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return AbortError(aEvent);
      }
    case STATE_STARTING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          return StartedAudioCapture(aEvent);
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return AbortError(aEvent);
        case EVENT_ABORT:
          return AbortSilently(aEvent);
        case EVENT_STOP:
          return Reset();
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          return DoNothing(aEvent);
        case EVENT_START:
          SR_LOG("STATE_STARTING: Unhandled event %d", aEvent->mType);
          MOZ_NOT_REACHED("");
      }
    case STATE_ESTIMATING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          return WaitForEstimation(aEvent);
        case EVENT_STOP:
          return StopRecordingAndRecognize(aEvent);
        case EVENT_ABORT:
          return AbortSilently(aEvent);
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return DoNothing(aEvent);
        case EVENT_START:
        case EVENT_AUDIO_ERROR:
          SR_LOG("STATE_ESTIMATING: Unhandled event %d", aEvent->mType);
          MOZ_NOT_REACHED("");
      }
    case STATE_WAITING_FOR_SPEECH:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          return DetectSpeech(aEvent);
        case EVENT_STOP:
          return StopRecordingAndRecognize(aEvent);
        case EVENT_ABORT:
          return AbortSilently(aEvent);
        case EVENT_AUDIO_ERROR:
          return AbortError(aEvent);
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return DoNothing(aEvent);
        case EVENT_START:
          SR_LOG("STATE_STARTING: Unhandled event %d", aEvent->mType);
          MOZ_NOT_REACHED("");
      }
    case STATE_RECOGNIZING:
      switch (aEvent->mType) {
        case EVENT_AUDIO_DATA:
          return WaitForSpeechEnd(aEvent);
        case EVENT_STOP:
          return StopRecordingAndRecognize(aEvent);
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return AbortError(aEvent);
        case EVENT_ABORT:
          return AbortSilently(aEvent);
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
          return DoNothing(aEvent);
        case EVENT_START:
          SR_LOG("STATE_RECOGNIZING: Unhandled aEvent %d", aEvent->mType);
          MOZ_NOT_REACHED("");
      }
    case STATE_WAITING_FOR_RESULT:
      switch (aEvent->mType) {
        case EVENT_STOP:
          return DoNothing(aEvent);
        case EVENT_AUDIO_ERROR:
        case EVENT_RECOGNITIONSERVICE_ERROR:
          return AbortError(aEvent);
        case EVENT_RECOGNITIONSERVICE_FINAL_RESULT:
          return NotifyFinalResult(aEvent);
        case EVENT_AUDIO_DATA:
          return DoNothing(aEvent);
        case EVENT_ABORT:
          return AbortSilently(aEvent);
        case EVENT_START:
        case EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT:
          SR_LOG("STATE_WAITING_FOR_RESULT: Unhandled aEvent %d", aEvent->mType);
          MOZ_NOT_REACHED("");
      }
  }
  SR_LOG("Unhandled state %d", mCurrentState);
  MOZ_NOT_REACHED("");
  return mCurrentState;
}

/*
 * Handle a segment of recorded audio data.
 * Returns the number of samples that were processed.
 */
uint32_t
SpeechRecognition::ProcessAudioSegment(AudioSegment* aSegment)
{
  AudioSegment::ChunkIterator iterator(*aSegment);
  uint32_t samples = 0;
  while (!iterator.IsEnded()) {
    float out;
    mEndpointer.ProcessAudio(*iterator, &out);
    samples += iterator->GetDuration();
    iterator.Next();
  }

  mRecognitionService->ProcessAudioSegment(aSegment);
  return samples;
}

void
SpeechRecognition::GetRecognitionServiceCID(nsACString& aResultCID)
{
  if (mTestConfig.mFakeRecognitionService) {
    aResultCID =
      NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX "fake";

    return;
  }

  nsAdoptingCString prefValue =
    Preferences::GetCString(PREFERENCE_DEFAULT_RECOGNITION_SERVICE);

  nsAutoCString speechRecognitionService;
  if (!prefValue.get() || prefValue.IsEmpty()) {
    speechRecognitionService = DEFAULT_RECOGNITION_SERVICE;
  } else {
    speechRecognitionService = prefValue;
  }

  aResultCID =
    NS_LITERAL_CSTRING(NS_SPEECH_RECOGNITION_SERVICE_CONTRACTID_PREFIX) +
    speechRecognitionService;

  return;
}

/****************************
 * FSM Transition functions *
 ****************************/

SpeechRecognition::FSMState
SpeechRecognition::Reset()
{
  mRecognitionService = nullptr;
  mEstimationSamples = 0;
  mBufferedSamples = 0;
  mSpeechDetectionTimer->Cancel();

  return STATE_IDLE;
}

/*
 * Since the handler for "end" may call
 * start(), we want to fully reset before dispatching
 * the event.
 */
SpeechRecognition::FSMState
SpeechRecognition::ResetAndEnd()
{
  mCurrentState = Reset();
  DispatchTrustedEvent(NS_LITERAL_STRING("end"));
  return mCurrentState;
}

SpeechRecognition::FSMState
SpeechRecognition::StartedAudioCapture(SpeechEvent* aEvent)
{
  mEndpointer.SetEnvironmentEstimationMode();
  mEstimationSamples += ProcessAudioSegment(aEvent->mAudioSegment);

  DispatchTrustedEvent(NS_LITERAL_STRING("start"));
  DispatchTrustedEvent(NS_LITERAL_STRING("audiostart"));

  return STATE_ESTIMATING;
}

SpeechRecognition::FSMState
SpeechRecognition::StopRecordingAndRecognize(SpeechEvent* aEvent)
{
  StopRecording();
  MOZ_ASSERT(mRecognitionService, "Service deleted before recording done");
  mRecognitionService->SoundEnd();

  return STATE_WAITING_FOR_RESULT;
}

SpeechRecognition::FSMState
SpeechRecognition::WaitForEstimation(SpeechEvent* aEvent)
{
  mEstimationSamples += ProcessAudioSegment(aEvent->mAudioSegment);

  if (mEstimationSamples > kESTIMATION_SAMPLES) {
    mEndpointer.SetUserInputMode();
    return STATE_WAITING_FOR_SPEECH;
  }

  return STATE_ESTIMATING;
}

SpeechRecognition::FSMState
SpeechRecognition::DetectSpeech(SpeechEvent* aEvent)
{
  ProcessAudioSegment(aEvent->mAudioSegment);

  if (mEndpointer.DidStartReceivingSpeech()) {
    mSpeechDetectionTimer->Cancel();
    DispatchTrustedEvent(NS_LITERAL_STRING("speechstart"));
    return STATE_RECOGNIZING;
  }

  return STATE_WAITING_FOR_SPEECH;
}

SpeechRecognition::FSMState
SpeechRecognition::WaitForSpeechEnd(SpeechEvent* aEvent)
{
  ProcessAudioSegment(aEvent->mAudioSegment);

  if (mEndpointer.speech_input_complete()) {
    // FIXME: StopRecordingAndRecognize should only be called for single
    // shot services for continous we should just inform the service
    DispatchTrustedEvent(NS_LITERAL_STRING("speechend"));
    return StopRecordingAndRecognize(aEvent);
  }

   return STATE_RECOGNIZING;
}

SpeechRecognition::FSMState
SpeechRecognition::NotifyFinalResult(SpeechEvent* aEvent)
{
  nsCOMPtr<nsIDOMEvent> domEvent;
  NS_NewDOMSpeechRecognitionEvent(getter_AddRefs(domEvent), nullptr, nullptr, nullptr);

  nsCOMPtr<nsIDOMSpeechRecognitionEvent> srEvent = do_QueryInterface(domEvent);
  nsRefPtr<SpeechRecognitionResultList> rlist = aEvent->mRecognitionResultList;
  nsCOMPtr<nsISupports> ilist = do_QueryInterface(rlist);
  srEvent->InitSpeechRecognitionEvent(NS_LITERAL_STRING("result"),
                                      true, false, 0, ilist,
                                      NS_LITERAL_STRING("NOT_IMPLEMENTED"),
                                      NULL);
  domEvent->SetTrusted(true);

  bool defaultActionEnabled;
  this->DispatchEvent(domEvent, &defaultActionEnabled);
  return ResetAndEnd();
}

SpeechRecognition::FSMState
SpeechRecognition::DoNothing(SpeechEvent* aEvent)
{
  return mCurrentState;
}

SpeechRecognition::FSMState
SpeechRecognition::AbortSilently(SpeechEvent* aEvent)
{
  if (mRecognitionService) {
    mRecognitionService->Abort();
  }

  if (STATE_BETWEEN(STATE_ESTIMATING, STATE_RECOGNIZING)) {
    StopRecording();
  }

  return ResetAndEnd();
}

SpeechRecognition::FSMState
SpeechRecognition::AbortError(SpeechEvent* aEvent)
{
  FSMState nextState = AbortSilently(aEvent);
  NotifyError(aEvent);
  return nextState;
}

void
SpeechRecognition::NotifyError(SpeechEvent* aEvent)
{
  nsCOMPtr<nsIDOMEvent> domEvent = do_QueryInterface(aEvent->mError);
  domEvent->SetTrusted(true);

  bool defaultActionEnabled;
  this->DispatchEvent(domEvent, &defaultActionEnabled);

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

  NS_ENSURE_STATE(mDOMStream->GetStream());
  mSpeechListener = new SpeechStreamListener(this);
  mDOMStream->GetStream()->AddListener(mSpeechListener);

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
  mDOMStream->GetStream()->RemoveListener(mSpeechListener);
  mSpeechListener = nullptr;
  mDOMStream = nullptr;

  mEndpointer.EndSession();
  DispatchTrustedEvent(NS_LITERAL_STRING("audioend"));

  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::Observe(nsISupports* aSubject, const char* aTopic,
                           const PRUnichar* aData)
{
  MOZ_ASSERT(NS_IsMainThread(), "Observer invoked off the main thread");

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC) &&
      STATE_BETWEEN(STATE_IDLE, STATE_WAITING_FOR_SPEECH)) {

    DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR,
                  nsIDOMSpeechRecognitionError::NO_SPEECH,
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
  if (aEventName.EqualsLiteral("EVENT_START")) {
    ErrorResult err;
    Start(err);
  } else if (aEventName.EqualsLiteral("EVENT_STOP")) {
    Stop();
  } else if (aEventName.EqualsLiteral("EVENT_ABORT")) {
    Abort();
  } else if (aEventName.EqualsLiteral("EVENT_AUDIO_ERROR")) {
    DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR,
                  nsIDOMSpeechRecognitionError::AUDIO_CAPTURE, // TODO different codes?
                  NS_LITERAL_STRING("AUDIO_ERROR test event"));
  } else if (aEventName.EqualsLiteral("EVENT_AUDIO_DATA")) {
    StartRecording(static_cast<DOMMediaStream*>(aSubject));
  } else {
    NS_ASSERTION(mTestConfig.mFakeRecognitionService,
                 "Got request for fake recognition service event, but "
                 TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE " is unset");

    // let the fake recognition service handle the request
  }

  return;
}

already_AddRefed<SpeechGrammarList>
SpeechRecognition::GetGrammars(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

void
SpeechRecognition::SetGrammars(mozilla::dom::SpeechGrammarList& aArg,
                               ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechRecognition::GetLang(nsString& aRetVal, ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechRecognition::SetLang(const nsAString& aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
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
SpeechRecognition::GetInterimResults(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return false;
}

void
SpeechRecognition::SetInterimResults(bool aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

uint32_t
SpeechRecognition::GetMaxAlternatives(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return 0;
}

void
SpeechRecognition::SetMaxAlternatives(uint32_t aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
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
SpeechRecognition::Start(ErrorResult& aRv)
{
  if (!STATE_EQUALS(STATE_IDLE)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsAutoCString speechRecognitionServiceCID;
  GetRecognitionServiceCID(speechRecognitionServiceCID);

  nsresult rv;
  mRecognitionService = do_GetService(speechRecognitionServiceCID.get(), &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = mRecognitionService->Initialize(this->asWeakPtr());
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!mTestConfig.mFakeFSMEvents) {
    MediaManager* manager = MediaManager::Get();
    manager->GetUserMedia(false,
                          GetOwner(),
                          new GetUserMediaStreamOptions(),
                          new GetUserMediaSuccessCallback(this),
                          new GetUserMediaErrorCallback(this));
  }

  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_START);
  NS_DispatchToMainThread(event);
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
  nsRefPtr<SpeechEvent> event = new SpeechEvent(this, EVENT_ABORT);
  NS_DispatchToMainThread(event);
}

void
SpeechRecognition::DispatchError(EventType aErrorType, int aErrorCode,
                                 const nsAString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aErrorType == EVENT_RECOGNITIONSERVICE_ERROR ||
             aErrorType == EVENT_AUDIO_ERROR, "Invalid error type!");

  nsCOMPtr<nsIDOMEvent> domEvent;
  NS_NewDOMSpeechRecognitionError(getter_AddRefs(domEvent), nullptr, nullptr, nullptr);

  nsCOMPtr<nsIDOMSpeechRecognitionError> srError = do_QueryInterface(domEvent);
  srError->InitSpeechRecognitionError(NS_LITERAL_STRING("error"), true, false,
                                      aErrorCode, aMessage);
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
                                      nsTArray<already_AddRefed<SharedBuffer> >& aResult)
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
SpeechRecognition::CreateAudioSegment(nsTArray<already_AddRefed<SharedBuffer> >& aChunks)
{
  AudioSegment* segment = new AudioSegment();
  for (uint32_t i = 0; i < aChunks.Length(); ++i) {
    const int16_t* chunkData =
      static_cast<const int16_t*>(aChunks[i].get()->Data());

    nsAutoTArray<const int16_t*, 1> channels;
    channels.AppendElement(chunkData);
    segment->AppendFrames(aChunks[i], channels, mAudioSamplesPerChunk);
  }

  return segment;
}

void
SpeechRecognition::FeedAudioData(already_AddRefed<SharedBuffer> aSamples,
                                 uint32_t aDuration,
                                 MediaStreamListener* aProvider)
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
  nsAutoTArray<already_AddRefed<SharedBuffer>, 5> chunksToSend;

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
  NS_DispatchToMainThread(event);

  return;
}

NS_IMPL_ISUPPORTS1(SpeechRecognition::GetUserMediaStreamOptions, nsIMediaStreamOptions)

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetFake(bool* aFake)
{
  *aFake = false;
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetAudio(bool* aAudio)
{
  *aAudio = true;
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetVideo(bool* aVideo)
{
  *aVideo = false;
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetPicture(bool* aPicture)
{
  *aPicture = false;
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetCamera(nsAString& aCamera)
{
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetAudioDevice(nsIMediaDevice** aAudioDevice)
{
  *aAudioDevice = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
SpeechRecognition::GetUserMediaStreamOptions::GetVideoDevice(nsIMediaDevice** aVideoDevice)
{
  *aVideoDevice = nullptr;
  return NS_OK;
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

NS_IMPL_ISUPPORTS1(SpeechRecognition::GetUserMediaSuccessCallback, nsIDOMGetUserMediaSuccessCallback)

NS_IMETHODIMP
SpeechRecognition::GetUserMediaSuccessCallback::OnSuccess(nsISupports* aStream)
{
  nsCOMPtr<nsIDOMLocalMediaStream> localStream = do_QueryInterface(aStream);
  mRecognition->StartRecording(static_cast<DOMLocalMediaStream*>(localStream.get()));
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(SpeechRecognition::GetUserMediaErrorCallback, nsIDOMGetUserMediaErrorCallback)

NS_IMETHODIMP
SpeechRecognition::GetUserMediaErrorCallback::OnError(const nsAString& aError)
{
  int errorCode;

  if (aError.Equals(NS_LITERAL_STRING("PERMISSION_DENIED"))) {
    errorCode = nsIDOMSpeechRecognitionError::NOT_ALLOWED;
  } else {
    errorCode = nsIDOMSpeechRecognitionError::AUDIO_CAPTURE;
  }

  mRecognition->DispatchError(SpeechRecognition::EVENT_AUDIO_ERROR, errorCode,
                              aError);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
