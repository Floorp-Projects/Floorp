/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaRecorder.h"
#include "MediaEncoder.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "mozilla/dom/RecordErrorEvent.h"
#include "nsTArray.h"
#include "DOMMediaStream.h"
#include "EncodedBufferCache.h"
#include "nsIDOMFile.h"
#include "mozilla/dom/BlobEvent.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"

#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gMediaRecorderLog;
#define LOG(type, msg) PR_LOG(gMediaRecorderLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaRecorder, DOMEventTargetHelper,
                                   mStream)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaRecorder)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaRecorder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaRecorder, DOMEventTargetHelper)

/**
 * Session is an object to represent a single recording event.
 * In original design, all recording context is stored in MediaRecorder, which causes
 * a problem if someone calls MediaRecoder::Stop and MedaiRecorder::Start quickly.
 * To prevent blocking main thread, media encoding is executed in a second thread,
 * named as Read Thread. For the same reason, we do not wait Read Thread shutdown in
 * MediaRecorder::Stop. If someone call MediaRecoder::Start before Read Thread shutdown,
 * the same recording context in MediaRecoder might be access by two Reading Threads,
 * which cause a  problem.
 * In the new design, we put recording context into Session object, including Read
 * Thread.  Each Session has its own recording context and Read Thread, problem is been
 * resolved.
 *
 * Life cycle of a Session object.
 * 1) Initialization Stage (in main thread)
 *    Setup media streams in MSG, and bind MediaEncoder with Source Stream when mStream is available.
 *    Resource allocation, such as encoded data cache buffer and MediaEncoder.
 *    Create read thread.
 *    Automatically switch to Extract stage in the end of this stage.
 * 2) Extract Stage (in Read Thread)
 *    Pull encoded A/V frames from MediaEncoder, dispatch to OnDataAvailable handler.
 *    Unless a client calls Session::Stop, Session object keeps stay in this stage.
 * 3) Destroy Stage (in main thread)
 *    Switch from Extract stage to Destroy stage by calling Session::Stop.
 *    Release session resource and remove associated streams from MSG.
 *
 * Lifetime of MediaRecorder and Session objects.
 * 1) MediaRecorder creates a Session in MediaRecorder::Start function.
 *    And the Session registers itself to ShutdownObserver and also holds a
 *    MediaRecorder. Therefore, the reference dependency in gecko is:
 *    ShutdownObserver -> Session -> MediaRecorder
 * 2) A Session is destroyed in DestroyRunnable after MediaRecorder::Stop being called
 *    _and_ all encoded media data been passed to OnDataAvailable handler.
 * 3) MediaRecorder::Stop is called by user or the document is going to
 *    inactive or invisible.
 */
class MediaRecorder::Session: public nsIObserver
{
  NS_DECL_THREADSAFE_ISUPPORTS

  // Main thread task.
  // Create a blob event and send back to client.
  class PushBlobRunnable : public nsRunnable
  {
  public:
    PushBlobRunnable(Session* aSession)
      : mSession(aSession)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(PR_LOG_DEBUG, ("Session.PushBlobRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      nsRefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
      }

      if (mSession->IsEncoderError()) {
        recorder->NotifyError(NS_ERROR_UNEXPECTED);
      }
      nsresult rv = recorder->CreateAndDispatchBlobEvent(mSession->GetEncodedData());
      if (NS_FAILED(rv)) {
        recorder->NotifyError(rv);
      }

      return NS_OK;
    }

  private:
    nsRefPtr<Session> mSession;
  };

  // Fire start event and set mimeType, run in main thread task.
  class DispatchStartEventRunnable : public nsRunnable
  {
  public:
    DispatchStartEventRunnable(Session* aSession, const nsAString & aEventName)
      : mSession(aSession)
      , mEventName(aEventName)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(PR_LOG_DEBUG, ("Session.DispatchStartEventRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      NS_ENSURE_TRUE(mSession->mRecorder, NS_OK);
      nsRefPtr<MediaRecorder> recorder = mSession->mRecorder;

      recorder->SetMimeType(mSession->mMimeType);
      recorder->DispatchSimpleEvent(mEventName);

      return NS_OK;
    }

  private:
    nsRefPtr<Session> mSession;
    nsString mEventName;
  };

  // Record thread task and it run in Media Encoder thread.
  // Fetch encoded Audio/Video data from MediaEncoder.
  class ExtractRunnable : public nsRunnable
  {
  public:
    ExtractRunnable(already_AddRefed<Session>&& aSession)
      : mSession(aSession) {}

    ~ExtractRunnable()
    {
      if (mSession) {
        NS_WARNING("~ExtractRunnable something wrong the mSession should null");
        nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
        NS_WARN_IF_FALSE(mainThread, "Couldn't get the main thread!");
        NS_ProxyRelease(mainThread, mSession);
      }
    }

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_GetCurrentThread() == mSession->mReadThread);

      LOG(PR_LOG_DEBUG, ("Session.ExtractRunnable shutdown = %d", mSession->mEncoder->IsShutdown()));
      if (!mSession->mEncoder->IsShutdown()) {
        mSession->Extract(false);
        nsRefPtr<nsIRunnable> event = new ExtractRunnable(mSession.forget());
        if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
          NS_WARNING("Failed to dispatch ExtractRunnable to encoder thread");
        }
      } else {
        // Flush out remaining encoded data.
        mSession->Extract(true);
        // Destroy this Session object in main thread.
        if (NS_FAILED(NS_DispatchToMainThread(
                        new DestroyRunnable(mSession.forget())))) {
          MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
        }
      }
      return NS_OK;
    }

  private:
    nsRefPtr<Session> mSession;
  };

  // For Ensure recorder has tracks to record.
  class TracksAvailableCallback : public DOMMediaStream::OnTracksAvailableCallback
  {
  public:
    TracksAvailableCallback(Session *aSession)
     : mSession(aSession) {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream)
    {
      uint8_t trackType = aStream->GetHintContents();
      // ToDo: GetHintContents return 0 when recording media tags.
      if (trackType == 0) {
        nsTArray<nsRefPtr<mozilla::dom::AudioStreamTrack> > audioTracks;
        aStream->GetAudioTracks(audioTracks);
        nsTArray<nsRefPtr<mozilla::dom::VideoStreamTrack> > videoTracks;
        aStream->GetVideoTracks(videoTracks);
        // What is inside the track
        if (videoTracks.Length() > 0) {
          trackType |= DOMMediaStream::HINT_CONTENTS_VIDEO;
        }
        if (audioTracks.Length() > 0) {
          trackType |= DOMMediaStream::HINT_CONTENTS_AUDIO;
        }
      }
      LOG(PR_LOG_DEBUG, ("Session.NotifyTracksAvailable track type = (%d)", trackType));
      mSession->AfterTracksAdded(trackType);
    }
  private:
    nsRefPtr<Session> mSession;
  };
  // Main thread task.
  // To delete RecordingSession object.
  class DestroyRunnable : public nsRunnable
  {
  public:
    DestroyRunnable(already_AddRefed<Session>&& aSession)
      : mSession(aSession) {}

    NS_IMETHODIMP Run()
    {
      LOG(PR_LOG_DEBUG, ("Session.DestroyRunnable session refcnt = (%d) stopIssued %d s=(%p)",
                         (int)mSession->mRefCnt, mSession->mStopIssued, mSession.get()));
      MOZ_ASSERT(NS_IsMainThread() && mSession.get());
      nsRefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
      }
      // SourceMediaStream is ended, and send out TRACK_EVENT_END notification.
      // Read Thread will be terminate soon.
      // We need to switch MediaRecorder to "Stop" state first to make sure
      // MediaRecorder is not associated with this Session anymore, then, it's
      // safe to delete this Session.
      // Also avoid to run if this session already call stop before
      if (!mSession->mStopIssued) {
        ErrorResult result;
        mSession->mStopIssued = true;
        recorder->Stop(result);
        if (NS_FAILED(NS_DispatchToMainThread(
                        new DestroyRunnable(mSession.forget())))) {
          MOZ_ASSERT(false, "NS_DispatchToMainThread failed");
        }
        return NS_OK;
      }

      // Dispatch stop event and clear MIME type.
      mSession->mMimeType = NS_LITERAL_STRING("");
      recorder->SetMimeType(mSession->mMimeType);
      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));
      recorder->RemoveSession(mSession);
      mSession->mRecorder = nullptr;
      return NS_OK;
    }

  private:
    // Call mSession::Release automatically while DestroyRunnable be destroy.
    nsRefPtr<Session> mSession;
  };

  friend class PushBlobRunnable;
  friend class ExtractRunnable;
  friend class DestroyRunnable;
  friend class TracksAvailableCallback;

public:
  Session(MediaRecorder* aRecorder, int32_t aTimeSlice)
    : mRecorder(aRecorder),
      mTimeSlice(aTimeSlice),
      mStopIssued(false),
      mCanRetrieveData(false)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mEncodedBufferCache = new EncodedBufferCache(MAX_ALLOW_MEMORY_BUFFER);
    mLastBlobTimeStamp = TimeStamp::Now();
  }

  void Start()
  {
    LOG(PR_LOG_DEBUG, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    SetupStreams();
  }

  void Stop()
  {
    LOG(PR_LOG_DEBUG, ("Session.Stop %p", this));
    MOZ_ASSERT(NS_IsMainThread());
    mStopIssued = true;
    CleanupStreams();
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  nsresult Pause()
  {
    LOG(PR_LOG_DEBUG, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(1);

    return NS_OK;
  }

  nsresult Resume()
  {
    LOG(PR_LOG_DEBUG, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(-1);

    return NS_OK;
  }

  already_AddRefed<nsIDOMBlob> GetEncodedData()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mEncodedBufferCache->ExtractBlob(mMimeType);
  }

  bool IsEncoderError()
  {
    if (mEncoder && mEncoder->HasError()) {
      return true;
    }
    return false;
  }

private:
  // Only DestroyRunnable is allowed to delete Session object.
  virtual ~Session()
  {
    LOG(PR_LOG_DEBUG, ("Session.~Session (%p)", this));
    CleanupStreams();
  }
  // Pull encoded media data from MediaEncoder and put into EncodedBufferCache.
  // Destroy this session object in the end of this function.
  // If the bool aForceFlush is true, we will force to dispatch a
  // PushBlobRunnable to main thread.
  void Extract(bool aForceFlush)
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mReadThread);
    LOG(PR_LOG_DEBUG, ("Session.Extract %p", this));

    // Pull encoded media data from MediaEncoder
    nsTArray<nsTArray<uint8_t> > encodedBuf;
    mEncoder->GetEncodedData(&encodedBuf, mMimeType);

    // Append pulled data into cache buffer.
    for (uint32_t i = 0; i < encodedBuf.Length(); i++) {
      if (!encodedBuf[i].IsEmpty()) {
        mEncodedBufferCache->AppendBuffer(encodedBuf[i]);
        // Fire the start event when encoded data is available.
        if (!mCanRetrieveData) {
          NS_DispatchToMainThread(
            new DispatchStartEventRunnable(this, NS_LITERAL_STRING("start")));
          mCanRetrieveData = true;
        }
      }
    }

    // Whether push encoded data back to onDataAvailable automatically or we
    // need a flush.
    bool pushBlob = false;
    if ((mTimeSlice > 0) &&
        ((TimeStamp::Now()-mLastBlobTimeStamp).ToMilliseconds() > mTimeSlice)) {
      pushBlob = true;
    }
    if (pushBlob || aForceFlush) {
      if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this)))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
      } else {
        mLastBlobTimeStamp = TimeStamp::Now();
      }
    }
  }

  // Bind media source with MediaEncoder to receive raw media data.
  void SetupStreams()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Create a Track Union Stream
    MediaStreamGraph* gm = mRecorder->mStream->GetStream()->Graph();
    mTrackUnionStream = gm->CreateTrackUnionStream(nullptr);
    MOZ_ASSERT(mTrackUnionStream, "CreateTrackUnionStream failed");

    mTrackUnionStream->SetAutofinish(true);

    // Bind this Track Union Stream with Source Media
    mInputPort = mTrackUnionStream->AllocateInputPort(mRecorder->mStream->GetStream(), MediaInputPort::FLAG_BLOCK_OUTPUT);

    // Allocate encoder and bind with the Track Union Stream.
    TracksAvailableCallback* tracksAvailableCallback = new TracksAvailableCallback(this);
    mRecorder->mStream->OnTracksAvailable(tracksAvailableCallback);
  }

  void AfterTracksAdded(uint8_t aTrackTypes)
  {
    LOG(PR_LOG_DEBUG, ("Session.AfterTracksAdded %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    // Allocate encoder and bind with union stream.
    // At this stage, the API doesn't allow UA to choose the output mimeType format.

    nsCOMPtr<nsIDocument> doc = mRecorder->GetOwner()->GetExtantDoc();
    uint16_t appStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    if (doc) {
      doc->NodePrincipal()->GetAppStatus(&appStatus);
    }
    // Only allow certificated application can assign AUDIO_3GPP
    if (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED &&
         mRecorder->mMimeType.EqualsLiteral(AUDIO_3GPP)) {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(AUDIO_3GPP), aTrackTypes);
    } else {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(""), aTrackTypes);
    }

    if (!mEncoder) {
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    // Media stream is ready but UA issues a stop method follow by start method.
    // The Session::stop would clean the mTrackUnionStream. If the AfterTracksAdded
    // comes after stop command, this function would crash.
    if (!mTrackUnionStream) {
      DoSessionEndTask(NS_OK);
      return;
    }
    mTrackUnionStream->AddListener(mEncoder);
    // Create a thread to read encode media data from MediaEncoder.
    if (!mReadThread) {
      nsresult rv = NS_NewNamedThread("Media Encoder", getter_AddRefs(mReadThread));
      if (NS_FAILED(rv)) {
        DoSessionEndTask(rv);
        return;
      }
    }

    // In case source media stream does not notify track end, recieve
    // shutdown notification and stop Read Thread.
    nsContentUtils::RegisterShutdownObserver(this);

    nsRefPtr<nsIRunnable> event = new ExtractRunnable(this);
    if (NS_FAILED(mReadThread->Dispatch(event, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch ExtractRunnable at beginning");
    }
  }
  // application should get blob and onstop event
  void DoSessionEndTask(nsresult rv)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (NS_FAILED(rv)) {
      mRecorder->NotifyError(rv);
    }

    CleanupStreams();
    if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
    }
    // Destroy this session object in main thread.
    if (NS_FAILED(NS_DispatchToMainThread(new DestroyRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
    }
  }
  void CleanupStreams()
  {
    if (mInputPort.get()) {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }

    if (mTrackUnionStream.get()) {
      mTrackUnionStream->Destroy();
      mTrackUnionStream = nullptr;
    }
  }

  NS_IMETHODIMP Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
  {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(PR_LOG_DEBUG, ("Session.Observe XPCOM_SHUTDOWN %p", this));
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      // Force stop Session to terminate Read Thread.
      mEncoder->Cancel();
      if (mReadThread) {
        mReadThread->Shutdown();
        mReadThread = nullptr;
      }
      if (mRecorder) {
        mRecorder->RemoveSession(this);
        mRecorder = nullptr;
      }
      Stop();
    }

    return NS_OK;
  }

private:
  // Hold reference to MediaRecoder that ensure MediaRecorder is alive
  // if there is an active session. Access ONLY on main thread.
  nsRefPtr<MediaRecorder> mRecorder;

  // Receive track data from source and dispatch to Encoder.
  // Pause/ Resume controller.
  nsRefPtr<ProcessedMediaStream> mTrackUnionStream;
  nsRefPtr<MediaInputPort> mInputPort;

  // Runnable thread for read data from MediaEncode.
  nsCOMPtr<nsIThread> mReadThread;
  // MediaEncoder pipeline.
  nsRefPtr<MediaEncoder> mEncoder;
  // A buffer to cache encoded meda data.
  nsAutoPtr<EncodedBufferCache> mEncodedBufferCache;
  // Current session mimeType
  nsString mMimeType;
  // Timestamp of the last fired dataavailable event.
  TimeStamp mLastBlobTimeStamp;
  // The interval of passing encoded data from EncodedBufferCache to onDataAvailable
  // handler. "mTimeSlice < 0" means Session object does not push encoded data to
  // onDataAvailable, instead, it passive wait the client side pull encoded data
  // by calling requestData API.
  const int32_t mTimeSlice;
  // Indicate this session's stop has been called.
  bool mStopIssued;
  // Indicate session has encoded data. This can be changed in recording thread.
  bool mCanRetrieveData;
};

NS_IMPL_ISUPPORTS(MediaRecorder::Session, nsIObserver)

MediaRecorder::~MediaRecorder()
{
  LOG(PR_LOG_DEBUG, ("~MediaRecorder (%p)", this));
  UnRegisterActivityObserver();
}

MediaRecorder::MediaRecorder(DOMMediaStream& aStream, nsPIDOMWindow* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow),
    mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
  mStream = &aStream;
#ifdef PR_LOGGING
  if (!gMediaRecorderLog) {
    gMediaRecorderLog = PR_NewLogModule("MediaRecorder");
  }
#endif
  RegisterActivityObserver();
}

void
MediaRecorder::RegisterActivityObserver()
{
  nsPIDOMWindow* window = GetOwner();
  if (window) {
    nsIDocument* doc = window->GetExtantDoc();
    if (doc) {
      doc->RegisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void
MediaRecorder::UnRegisterActivityObserver()
{
  nsPIDOMWindow* window = GetOwner();
  if (window) {
    nsIDocument* doc = window->GetExtantDoc();
    if (doc) {
      doc->UnregisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void
MediaRecorder::SetMimeType(const nsString &aMimeType)
{
  mMimeType = aMimeType;
}

void
MediaRecorder::GetMimeType(nsString &aMimeType)
{
  aMimeType = mMimeType;
}

void
MediaRecorder::Start(const Optional<int32_t>& aTimeSlice, ErrorResult& aResult)
{
  LOG(PR_LOG_DEBUG, ("MediaRecorder.Start %p", this));
  if (mState != RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mStream->GetStream()->IsFinished() || mStream->GetStream()->IsDestroyed()) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!mStream->GetPrincipal()) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!CheckPrincipal()) {
    aResult.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  int32_t timeSlice = 0;
  if (aTimeSlice.WasPassed()) {
    if (aTimeSlice.Value() < 0) {
      aResult.Throw(NS_ERROR_INVALID_ARG);
      return;
    }

    timeSlice = aTimeSlice.Value();
  }

  mState = RecordingState::Recording;
  // Start a session.
  // Add session's reference here and pass to ExtractRunnable since the
  // MediaRecorder doesn't hold any reference to Session. Also
  // the DoSessionEndTask need this reference for DestroyRunnable.
  // Note that the reference count is not balance here due to the
  // DestroyRunnable will destroyed the last reference.
  nsRefPtr<Session> session = new Session(this, timeSlice);
  Session* rawPtr;
  session.forget(&rawPtr);
  mSessions.AppendElement();
  mSessions.LastElement() = rawPtr;
  mSessions.LastElement()->Start();
}

void
MediaRecorder::Stop(ErrorResult& aResult)
{
  LOG(PR_LOG_DEBUG, ("MediaRecorder.Stop %p", this));
  if (mState == RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mState = RecordingState::Inactive;
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->Stop();
}

void
MediaRecorder::Pause(ErrorResult& aResult)
{
  LOG(PR_LOG_DEBUG, ("MediaRecorder.Pause"));
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mSessions.Length() > 0);
  nsresult rv = mSessions.LastElement()->Pause();
  if (NS_FAILED(rv)) {
    NotifyError(rv);
    return;
  }
  mState = RecordingState::Paused;
}

void
MediaRecorder::Resume(ErrorResult& aResult)
{
  LOG(PR_LOG_DEBUG, ("MediaRecorder.Resume"));
  if (mState != RecordingState::Paused) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mSessions.Length() > 0);
  nsresult rv = mSessions.LastElement()->Resume();
  if (NS_FAILED(rv)) {
    NotifyError(rv);
    return;
  }
  mState = RecordingState::Recording;
}

class CreateAndDispatchBlobEventRunnable : public nsRunnable {
  nsCOMPtr<nsIDOMBlob> mBlob;
  nsRefPtr<MediaRecorder> mRecorder;
public:
  CreateAndDispatchBlobEventRunnable(already_AddRefed<nsIDOMBlob>&& aBlob,
                                     MediaRecorder* aRecorder)
    : mBlob(aBlob), mRecorder(aRecorder)
  { }

  NS_IMETHOD
  Run();
};

NS_IMETHODIMP
CreateAndDispatchBlobEventRunnable::Run()
{
  return mRecorder->CreateAndDispatchBlobEvent(mBlob.forget());
}

void
MediaRecorder::RequestData(ErrorResult& aResult)
{
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  MOZ_ASSERT(mSessions.Length() > 0);
  if (NS_FAILED(NS_DispatchToMainThread(
                  new CreateAndDispatchBlobEventRunnable(
                    mSessions.LastElement()->GetEncodedData(), this)))) {
    MOZ_ASSERT(false, "NS_DispatchToMainThread CreateAndDispatchBlobEventRunnable failed");
  }
}

JSObject*
MediaRecorder::WrapObject(JSContext* aCx)
{
  return MediaRecorderBinding::Wrap(aCx, this);
}

/* static */ already_AddRefed<MediaRecorder>
MediaRecorder::Constructor(const GlobalObject& aGlobal,
                           DOMMediaStream& aStream,
                           const MediaRecorderOptions& aInitDict,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobal.GetAsSupports());
  if (!sgo) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MediaRecorder> object = new MediaRecorder(aStream, ownerWindow);
  object->SetMimeType(aInitDict.mMimeType);
  return object.forget();
}

nsresult
MediaRecorder::CreateAndDispatchBlobEvent(already_AddRefed<nsIDOMBlob>&& aBlob)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (!CheckPrincipal()) {
    // Media is not same-origin, don't allow the data out.
    nsRefPtr<nsIDOMBlob> blob = aBlob;
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aBlob;
  nsRefPtr<BlobEvent> event =
    BlobEvent::Constructor(this,
                           NS_LITERAL_STRING("dataavailable"),
                           init);
  event->SetTrusted(true);
  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

void
MediaRecorder::DispatchSimpleEvent(const nsAString & aStr)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create the error event!!!");
    return;
  }
  rv = event->InitEvent(aStr, false, false);

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to init the error event!!!");
    return;
  }

  event->SetTrusted(true);

  rv = DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch the event!!!");
    return;
  }
}

void
MediaRecorder::NotifyError(nsresult aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }
  nsString errorMsg;
  switch (aRv) {
  case NS_ERROR_DOM_SECURITY_ERR:
    errorMsg = NS_LITERAL_STRING("SecurityError");
    break;
  case NS_ERROR_OUT_OF_MEMORY:
    errorMsg = NS_LITERAL_STRING("OutOfMemoryError");
    break;
  default:
    errorMsg = NS_LITERAL_STRING("GenericError");
  }

  RecordErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mName = errorMsg;

  nsRefPtr<RecordErrorEvent> event =
    RecordErrorEvent::Constructor(this, NS_LITERAL_STRING("error"), init);
  event->SetTrusted(true);

  rv = DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch the error event!!!");
    return;
  }
  return;
}

bool MediaRecorder::CheckPrincipal()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (!mStream) {
    return false;
  }
  nsCOMPtr<nsIPrincipal> principal = mStream->GetPrincipal();
  if (!GetOwner())
    return false;
  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (!doc || !principal)
    return false;

  bool subsumes;
  if (NS_FAILED(doc->NodePrincipal()->Subsumes(principal, &subsumes)))
    return false;

  return subsumes;
}

void
MediaRecorder::RemoveSession(Session* aSession)
{
  LOG(PR_LOG_DEBUG, ("MediaRecorder.RemoveSession (%p)", aSession));
  mSessions.RemoveElement(aSession);
}

void
MediaRecorder::NotifyOwnerDocumentActivityChanged()
{
  nsPIDOMWindow* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  nsIDocument* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  LOG(PR_LOG_DEBUG, ("MediaRecorder %p document IsActive %d isVisible %d\n",
                     this, doc->IsActive(), doc->IsVisible()));
  if (!doc->IsActive() || !doc->IsVisible()) {
    // Stop the session.
    ErrorResult result;
    Stop(result);
  }
}

}
}
