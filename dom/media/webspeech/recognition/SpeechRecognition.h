/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechRecognition_h
#define mozilla_dom_SpeechRecognition_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"
#include "js/TypeDecls.h"

#include "nsIDOMNavigatorUserMedia.h"
#include "nsITimer.h"
#include "MediaEngine.h"
#include "MediaStreamGraph.h"
#include "AudioSegment.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Preferences.h"

#include "SpeechGrammarList.h"
#include "SpeechRecognitionResultList.h"
#include "SpeechStreamListener.h"
#include "nsISpeechRecognitionService.h"
#include "endpointer.h"

#include "mozilla/dom/SpeechRecognitionError.h"

namespace mozilla {

namespace dom {

#define TEST_PREFERENCE_ENABLE "media.webspeech.test.enable"
#define TEST_PREFERENCE_FAKE_FSM_EVENTS "media.webspeech.test.fake_fsm_events"
#define TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE "media.webspeech.test.fake_recognition_service"
#define TEST_PREFERENCE_RECOGNITION_ENABLE "media.webspeech.recognition.enable"
#define TEST_PREFERENCE_RECOGNITION_FORCE_ENABLE "media.webspeech.recognition.force_enable"
#define SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC "SpeechRecognitionTest:RequestEvent"
#define SPEECH_RECOGNITION_TEST_END_TOPIC "SpeechRecognitionTest:End"

class GlobalObject;
class SpeechEvent;

PRLogModuleInfo* GetSpeechRecognitionLog();
#define SR_LOG(...) MOZ_LOG(GetSpeechRecognitionLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

class SpeechRecognition final : public DOMEventTargetHelper,
                                public nsIObserver,
                                public SupportsWeakPtr<SpeechRecognition>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(SpeechRecognition)
  explicit SpeechRecognition(nsPIDOMWindow* aOwnerWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SpeechRecognition, DOMEventTargetHelper)

  NS_DECL_NSIOBSERVER

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static bool IsAuthorized(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<SpeechRecognition>
  Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  already_AddRefed<SpeechGrammarList> Grammars() const;

  void SetGrammars(mozilla::dom::SpeechGrammarList& aArg);

  void GetLang(nsString& aRetVal) const;

  void SetLang(const nsAString& aArg);

  bool GetContinuous(ErrorResult& aRv) const;

  void SetContinuous(bool aArg, ErrorResult& aRv);

  bool GetInterimResults(ErrorResult& aRv) const;

  void SetInterimResults(bool aArg, ErrorResult& aRv);

  uint32_t GetMaxAlternatives(ErrorResult& aRv) const;

  void SetMaxAlternatives(uint32_t aArg, ErrorResult& aRv);

  void GetServiceURI(nsString& aRetVal, ErrorResult& aRv) const;

  void SetServiceURI(const nsAString& aArg, ErrorResult& aRv);

  void Start(const Optional<NonNull<DOMMediaStream>>& aStream, ErrorResult& aRv);

  void Stop();

  void Abort();

  IMPL_EVENT_HANDLER(audiostart)
  IMPL_EVENT_HANDLER(soundstart)
  IMPL_EVENT_HANDLER(speechstart)
  IMPL_EVENT_HANDLER(speechend)
  IMPL_EVENT_HANDLER(soundend)
  IMPL_EVENT_HANDLER(audioend)
  IMPL_EVENT_HANDLER(result)
  IMPL_EVENT_HANDLER(nomatch)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(start)
  IMPL_EVENT_HANDLER(end)

  enum EventType {
    EVENT_START,
    EVENT_STOP,
    EVENT_ABORT,
    EVENT_AUDIO_DATA,
    EVENT_AUDIO_ERROR,
    EVENT_RECOGNITIONSERVICE_INTERMEDIATE_RESULT,
    EVENT_RECOGNITIONSERVICE_FINAL_RESULT,
    EVENT_RECOGNITIONSERVICE_ERROR,
    EVENT_COUNT
  };

  void DispatchError(EventType aErrorType, SpeechRecognitionErrorCode aErrorCode, const nsAString& aMessage);
  uint32_t FillSamplesBuffer(const int16_t* aSamples, uint32_t aSampleCount);
  uint32_t SplitSamplesBuffer(const int16_t* aSamplesBuffer, uint32_t aSampleCount, nsTArray<nsRefPtr<SharedBuffer>>& aResult);
  AudioSegment* CreateAudioSegment(nsTArray<nsRefPtr<SharedBuffer>>& aChunks);
  void FeedAudioData(already_AddRefed<SharedBuffer> aSamples, uint32_t aDuration, MediaStreamListener* aProvider, TrackRate aTrackRate);

  static struct TestConfig
  {
  public:
    bool mEnableTests;
    bool mFakeFSMEvents;
    bool mFakeRecognitionService;

    void Init()
    {
      if (mInitialized) {
        return;
      }

      Preferences::AddBoolVarCache(&mEnableTests, TEST_PREFERENCE_ENABLE);

      if (mEnableTests) {
        Preferences::AddBoolVarCache(&mFakeFSMEvents, TEST_PREFERENCE_FAKE_FSM_EVENTS);
        Preferences::AddBoolVarCache(&mFakeRecognitionService, TEST_PREFERENCE_FAKE_RECOGNITION_SERVICE);
      }

      mInitialized = true;
    }
  private:
    bool mInitialized;
  } mTestConfig;


  friend class SpeechEvent;
private:
  virtual ~SpeechRecognition() {};

  enum FSMState {
    STATE_IDLE,
    STATE_STARTING,
    STATE_ESTIMATING,
    STATE_WAITING_FOR_SPEECH,
    STATE_RECOGNIZING,
    STATE_WAITING_FOR_RESULT,
    STATE_COUNT
  };

  void SetState(FSMState state);
  bool StateBetween(FSMState begin, FSMState end);

  bool SetRecognitionService(ErrorResult& aRv);
  bool ValidateAndSetGrammarList(ErrorResult& aRv);

  class GetUserMediaSuccessCallback : public nsIDOMGetUserMediaSuccessCallback
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMGETUSERMEDIASUCCESSCALLBACK

    explicit GetUserMediaSuccessCallback(SpeechRecognition* aRecognition)
      : mRecognition(aRecognition)
    {}

  private:
    virtual ~GetUserMediaSuccessCallback() {}

    nsRefPtr<SpeechRecognition> mRecognition;
  };

  class GetUserMediaErrorCallback : public nsIDOMGetUserMediaErrorCallback
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMGETUSERMEDIAERRORCALLBACK

    explicit GetUserMediaErrorCallback(SpeechRecognition* aRecognition)
      : mRecognition(aRecognition)
    {}

  private:
    virtual ~GetUserMediaErrorCallback() {}

    nsRefPtr<SpeechRecognition> mRecognition;
  };

  NS_IMETHOD StartRecording(DOMMediaStream* aDOMStream);
  NS_IMETHOD StopRecording();

  uint32_t ProcessAudioSegment(AudioSegment* aSegment, TrackRate aTrackRate);
  void NotifyError(SpeechEvent* aEvent);

  void ProcessEvent(SpeechEvent* aEvent);
  void Transition(SpeechEvent* aEvent);

  void Reset();
  void ResetAndEnd();
  void WaitForAudioData(SpeechEvent* aEvent);
  void StartedAudioCapture(SpeechEvent* aEvent);
  void StopRecordingAndRecognize(SpeechEvent* aEvent);
  void WaitForEstimation(SpeechEvent* aEvent);
  void DetectSpeech(SpeechEvent* aEvent);
  void WaitForSpeechEnd(SpeechEvent* aEvent);
  void NotifyFinalResult(SpeechEvent* aEvent);
  void DoNothing(SpeechEvent* aEvent);
  void AbortSilently(SpeechEvent* aEvent);
  void AbortError(SpeechEvent* aEvent);

  nsRefPtr<DOMMediaStream> mDOMStream;
  nsRefPtr<SpeechStreamListener> mSpeechListener;
  nsCOMPtr<nsISpeechRecognitionService> mRecognitionService;

  FSMState mCurrentState;

  Endpointer mEndpointer;
  uint32_t mEstimationSamples;

  uint32_t mAudioSamplesPerChunk;

  // buffer holds one chunk of mAudioSamplesPerChunk
  // samples before feeding it to mEndpointer
  nsRefPtr<SharedBuffer> mAudioSamplesBuffer;
  uint32_t mBufferedSamples;

  nsCOMPtr<nsITimer> mSpeechDetectionTimer;
  bool mAborted;

  nsString mLang;

  nsRefPtr<SpeechGrammarList> mSpeechGrammarList;

  void ProcessTestEventRequest(nsISupports* aSubject, const nsAString& aEventName);

  const char* GetName(FSMState aId);
  const char* GetName(SpeechEvent* aId);
};

class SpeechEvent : public nsRunnable
{
public:
  SpeechEvent(SpeechRecognition* aRecognition, SpeechRecognition::EventType aType)
  : mAudioSegment(0)
  , mRecognitionResultList(0)
  , mError(0)
  , mRecognition(aRecognition)
  , mType(aType)
  , mTrackRate(0)
  {
  }

  ~SpeechEvent();

  NS_IMETHOD Run() override;
  AudioSegment* mAudioSegment;
  nsRefPtr<SpeechRecognitionResultList> mRecognitionResultList; // TODO: make this a session being passed which also has index and stuff
  nsRefPtr<SpeechRecognitionError> mError;

  friend class SpeechRecognition;
private:
  SpeechRecognition* mRecognition;

  // for AUDIO_DATA events, keep a reference to the provider
  // of the data (i.e., the SpeechStreamListener) to ensure it
  // is kept alive (and keeps SpeechRecognition alive) until this
  // event gets processed.
  nsRefPtr<MediaStreamListener> mProvider;
  SpeechRecognition::EventType mType;
  TrackRate mTrackRate;
};

} // namespace dom

inline nsISupports*
ToSupports(dom::SpeechRecognition* aRec)
{
  return ToSupports(static_cast<DOMEventTargetHelper*>(aRec));
}

} // namespace mozilla

#endif
