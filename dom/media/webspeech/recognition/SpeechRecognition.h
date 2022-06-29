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
#include "nsProxyRelease.h"
#include "DOMMediaStream.h"
#include "nsITimer.h"
#include "MediaTrackGraph.h"
#include "AudioSegment.h"
#include "mozilla/WeakPtr.h"

#include "SpeechGrammarList.h"
#include "SpeechRecognitionResultList.h"
#include "nsISpeechRecognitionService.h"
#include "endpointer.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SpeechRecognitionError.h"

namespace mozilla {

namespace media {
class ShutdownBlocker;
}

namespace dom {

#define SPEECH_RECOGNITION_TEST_EVENT_REQUEST_TOPIC \
  "SpeechRecognitionTest:RequestEvent"
#define SPEECH_RECOGNITION_TEST_END_TOPIC "SpeechRecognitionTest:End"

class GlobalObject;
class AudioStreamTrack;
class SpeechEvent;
class SpeechTrackListener;

LogModule* GetSpeechRecognitionLog();
#define SR_LOG(...) \
  MOZ_LOG(GetSpeechRecognitionLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))

class SpeechRecognition final : public DOMEventTargetHelper,
                                public nsIObserver,
                                public DOMMediaStream::TrackListener,
                                public SupportsWeakPtr {
 public:
  explicit SpeechRecognition(nsPIDOMWindowInner* aOwnerWindow);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SpeechRecognition,
                                           DOMEventTargetHelper)

  NS_DECL_NSIOBSERVER

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static bool IsAuthorized(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<SpeechRecognition> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  static already_AddRefed<SpeechRecognition> WebkitSpeechRecognition(
      const GlobalObject& aGlobal, ErrorResult& aRv) {
    return Constructor(aGlobal, aRv);
  }

  already_AddRefed<SpeechGrammarList> Grammars() const;

  void SetGrammars(mozilla::dom::SpeechGrammarList& aArg);

  void GetLang(nsString& aRetVal) const;

  void SetLang(const nsAString& aArg);

  bool GetContinuous(ErrorResult& aRv) const;

  void SetContinuous(bool aArg, ErrorResult& aRv);

  bool InterimResults() const;

  void SetInterimResults(bool aArg);

  uint32_t MaxAlternatives() const;

  TaskQueue* GetTaskQueueForEncoding() const;

  void SetMaxAlternatives(uint32_t aArg);

  void GetServiceURI(nsString& aRetVal, ErrorResult& aRv) const;

  void SetServiceURI(const nsAString& aArg, ErrorResult& aRv);

  void Start(const Optional<NonNull<DOMMediaStream>>& aStream,
             CallerType aCallerType, ErrorResult& aRv);

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

  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override;
  // aMessage should be valid UTF-8, but invalid UTF-8 byte sequences are
  // replaced with the REPLACEMENT CHARACTER on conversion to UTF-16.
  void DispatchError(EventType aErrorType,
                     SpeechRecognitionErrorCode aErrorCode,
                     const nsACString& aMessage);
  template <int N>
  void DispatchError(EventType aErrorType,
                     SpeechRecognitionErrorCode aErrorCode,
                     const char (&aMessage)[N]) {
    DispatchError(aErrorType, aErrorCode, nsLiteralCString(aMessage));
  }
  uint32_t FillSamplesBuffer(const int16_t* aSamples, uint32_t aSampleCount);
  uint32_t SplitSamplesBuffer(const int16_t* aSamplesBuffer,
                              uint32_t aSampleCount,
                              nsTArray<RefPtr<SharedBuffer>>& aResult);
  AudioSegment* CreateAudioSegment(nsTArray<RefPtr<SharedBuffer>>& aChunks);
  void FeedAudioData(nsMainThreadPtrHandle<SpeechRecognition>& aRecognition,
                     already_AddRefed<SharedBuffer> aSamples,
                     uint32_t aDuration, MediaTrackListener* aProvider,
                     TrackRate aTrackRate);

  friend class SpeechEvent;

 private:
  virtual ~SpeechRecognition();

  enum FSMState {
    STATE_IDLE,
    STATE_STARTING,
    STATE_ESTIMATING,
    STATE_WAITING_FOR_SPEECH,
    STATE_RECOGNIZING,
    STATE_WAITING_FOR_RESULT,
    STATE_ABORTING,
    STATE_COUNT
  };

  void SetState(FSMState state);
  bool StateBetween(FSMState begin, FSMState end);

  bool SetRecognitionService(ErrorResult& aRv);
  bool ValidateAndSetGrammarList(ErrorResult& aRv);

  NS_IMETHOD StartRecording(RefPtr<AudioStreamTrack>& aDOMStream);
  RefPtr<GenericNonExclusivePromise> StopRecording();

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

  RefPtr<DOMMediaStream> mStream;
  RefPtr<AudioStreamTrack> mTrack;
  bool mTrackIsOwned = false;
  RefPtr<GenericNonExclusivePromise> mStopRecordingPromise;
  RefPtr<SpeechTrackListener> mSpeechListener;
  nsCOMPtr<nsISpeechRecognitionService> mRecognitionService;
  RefPtr<media::ShutdownBlocker> mShutdownBlocker;
  // TaskQueue responsible for pre-processing the samples by the service
  // it runs in a separate thread from the main thread
  RefPtr<TaskQueue> mEncodeTaskQueue;

  // A generation ID of the MediaStream a started session is for, so that
  // a gUM request that resolves after the session has stopped, and a new
  // one has started, can exit early. Main thread only. Can wrap.
  uint8_t mStreamGeneration = 0;

  FSMState mCurrentState;

  Endpointer mEndpointer;
  uint32_t mEstimationSamples;

  uint32_t mAudioSamplesPerChunk;

  // maximum amount of seconds the engine will wait for voice
  // until returning a 'no speech detected' error
  uint32_t mSpeechDetectionTimeoutMs;

  // buffer holds one chunk of mAudioSamplesPerChunk
  // samples before feeding it to mEndpointer
  RefPtr<SharedBuffer> mAudioSamplesBuffer;
  uint32_t mBufferedSamples;

  nsCOMPtr<nsITimer> mSpeechDetectionTimer;
  bool mAborted;

  nsString mLang;

  RefPtr<SpeechGrammarList> mSpeechGrammarList;

  // private flag used to hold if the user called the setContinuous() method
  // of the API
  bool mContinuous;

  // WebSpeechAPI (http://bit.ly/1gIl7DC) states:
  //
  // 1. Default value MUST be false
  // 2. If true, interim results SHOULD be returned
  // 3. If false, interim results MUST NOT be returned
  //
  // Pocketsphinx does not return interm results; so, defaulting
  // mInterimResults to false, then ignoring its subsequent value
  // is a conforming implementation.
  bool mInterimResults;

  // WebSpeechAPI (http://bit.ly/1JAiqeo) states:
  //
  // 1. Default value is 1
  // 2. Subsequent value is the "maximum number of SpeechRecognitionAlternatives
  // per result"
  //
  // Pocketsphinx can only return at maximum a single
  // SpeechRecognitionAlternative per SpeechRecognitionResult. So defaulting
  // mMaxAlternatives to 1, for all non zero values ignoring mMaxAlternatives
  // while for a 0 value returning no SpeechRecognitionAlternative per result is
  // a conforming implementation.
  uint32_t mMaxAlternatives;

  void ProcessTestEventRequest(nsISupports* aSubject,
                               const nsAString& aEventName);

  const char* GetName(FSMState aId);
  const char* GetName(SpeechEvent* aEvent);
};

class SpeechEvent : public Runnable {
 public:
  SpeechEvent(SpeechRecognition* aRecognition,
              SpeechRecognition::EventType aType);
  SpeechEvent(nsMainThreadPtrHandle<SpeechRecognition>& aRecognition,
              SpeechRecognition::EventType aType);

  ~SpeechEvent();

  NS_IMETHOD Run() override;
  AudioSegment* mAudioSegment;
  RefPtr<SpeechRecognitionResultList>
      mRecognitionResultList;  // TODO: make this a session being passed which
                               // also has index and stuff
  RefPtr<SpeechRecognitionError> mError;

  friend class SpeechRecognition;

 private:
  nsMainThreadPtrHandle<SpeechRecognition> mRecognition;

  // for AUDIO_DATA events, keep a reference to the provider
  // of the data (i.e., the SpeechTrackListener) to ensure it
  // is kept alive (and keeps SpeechRecognition alive) until this
  // event gets processed.
  RefPtr<MediaTrackListener> mProvider;
  SpeechRecognition::EventType mType;
  TrackRate mTrackRate;
};

}  // namespace dom

inline nsISupports* ToSupports(dom::SpeechRecognition* aRec) {
  return ToSupports(static_cast<DOMEventTargetHelper*>(aRec));
}

}  // namespace mozilla

#endif
