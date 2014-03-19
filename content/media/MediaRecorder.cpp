/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaRecorder.h"
#include "GeneratedEvents.h"
#include "MediaEncoder.h"
#include "nsDOMEventTargetHelper.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIDOMRecordErrorEvent.h"
#include "nsTArray.h"
#include "DOMMediaStream.h"
#include "EncodedBufferCache.h"
#include "nsIDOMFile.h"
#include "mozilla/dom/BlobEvent.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"

#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/VideoStreamTrack.h"

namespace mozilla {

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(MediaRecorder, nsDOMEventTargetHelper,
                                     mStream, mSession)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaRecorder)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaRecorder, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaRecorder, nsDOMEventTargetHelper)

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
 * Lifetime of a Session object.
 * 1) MediaRecorder creates a Session in MediaRecorder::Start function.
 * 2) A Session is destroyed in DestroyRunnable after MediaRecorder::Stop being called
 *    _and_ all encoded media data been passed to OnDataAvailable handler.
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
      MOZ_ASSERT(NS_IsMainThread());

      MediaRecorder *recorder = mSession->mRecorder;
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

  // Record thread task and it run in Media Encoder thread.
  // Fetch encoded Audio/Video data from MediaEncoder.
  class ExtractRunnable : public nsRunnable
  {
  public:
    ExtractRunnable(Session *aSession)
      : mSession(aSession) {}

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_GetCurrentThread() == mSession->mReadThread);

      mSession->Extract();
      if (!mSession->mEncoder->IsShutdown()) {
        NS_DispatchToCurrentThread(new ExtractRunnable(mSession));
      } else {
        // Flush out remainding encoded data.
        NS_DispatchToMainThread(new PushBlobRunnable(mSession));
        // Destroy this Session object in main thread.
        NS_DispatchToMainThread(new DestroyRunnable(already_AddRefed<Session>(mSession)));
      }
      return NS_OK;
    }

  private:
    Session* mSession;
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
      MOZ_ASSERT(NS_IsMainThread() && mSession.get());
      MediaRecorder *recorder = mSession->mRecorder;

      // SourceMediaStream is ended, and send out TRACK_EVENT_END notification.
      // Read Thread will be terminate soon.
      // We need to switch MediaRecorder to "Stop" state first to make sure
      // MediaRecorder is not associated with this Session anymore, then, it's
      // safe to delete this Session.
      // Also avoid to run if this session already call stop before
      if (!mSession->mStopIssued) {
        ErrorResult result;
        recorder->Stop(result);
        NS_DispatchToMainThread(new DestroyRunnable(mSession.forget()));

        return NS_OK;
      }

      // Dispatch stop event and clear MIME type.
      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));
      mSession->mMimeType = NS_LITERAL_STRING("");
      recorder->SetMimeType(mSession->mMimeType);
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
      mStopIssued(false)
  {
    MOZ_ASSERT(NS_IsMainThread());

    AddRef();
    mEncodedBufferCache = new EncodedBufferCache(MAX_ALLOW_MEMORY_BUFFER);
    mLastBlobTimeStamp = TimeStamp::Now();
  }

  // Only DestroyRunnable is allowed to delete Session object.
  virtual ~Session()
  {
    CleanupStreams();
  }

  void Start()
  {
    MOZ_ASSERT(NS_IsMainThread());

    SetupStreams();
  }

  void Stop()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mStopIssued = true;
    CleanupStreams();
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  nsresult Pause()
  {
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(-1);

    return NS_OK;
  }

  nsresult Resume()
  {
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(1);

    return NS_OK;
  }

  already_AddRefed<nsIDOMBlob> GetEncodedData()
  {
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

  // Pull encoded meida data from MediaEncoder and put into EncodedBufferCache.
  // Destroy this session object in the end of this function.
  void Extract()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mReadThread);

    // Whether push encoded data back to onDataAvailable automatically.
    const bool pushBlob = (mTimeSlice > 0) ? true : false;

    // Pull encoded media data from MediaEncoder
    nsTArray<nsTArray<uint8_t> > encodedBuf;
    mEncoder->GetEncodedData(&encodedBuf, mMimeType);
    mRecorder->SetMimeType(mMimeType);

    // Append pulled data into cache buffer.
    for (uint32_t i = 0; i < encodedBuf.Length(); i++) {
      mEncodedBufferCache->AppendBuffer(encodedBuf[i]);
    }

    if (pushBlob) {
      if ((TimeStamp::Now() - mLastBlobTimeStamp).ToMilliseconds() > mTimeSlice) {
        NS_DispatchToMainThread(new PushBlobRunnable(this));
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
    TracksAvailableCallback* tracksAvailableCallback = new TracksAvailableCallback(mRecorder->mSession);
    mRecorder->mStream->OnTracksAvailable(tracksAvailableCallback);
  }

  void AfterTracksAdded(uint8_t aTrackTypes)
  {
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

    mReadThread->Dispatch(new ExtractRunnable(this), NS_DISPATCH_NORMAL);
  }
  // application should get blob and onstop event
  void DoSessionEndTask(nsresult rv)
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(rv)) {
      mRecorder->NotifyError(rv);
    }
    CleanupStreams();
    // Destroy this session object in main thread.
    NS_DispatchToMainThread(new PushBlobRunnable(this));
    NS_DispatchToMainThread(new DestroyRunnable(already_AddRefed<Session>(this)));
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

    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      // Force stop Session to terminate Read Thread.
      Stop();
    }

    return NS_OK;
  }

private:
  // Hold a reference to MediaRecoder to make sure MediaRecoder be
  // destroyed after all session object dead.
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
};

NS_IMPL_ISUPPORTS1(MediaRecorder::Session, nsIObserver)

MediaRecorder::~MediaRecorder()
{
  MOZ_ASSERT(mSession == nullptr);
}

MediaRecorder::MediaRecorder(DOMMediaStream& aStream, nsPIDOMWindow* aOwnerWindow)
  : nsDOMEventTargetHelper(aOwnerWindow),
    mState(RecordingState::Inactive),
    mSession(nullptr),
    mMutex("Session.Data.Mutex")
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
  mStream = &aStream;
}

void
MediaRecorder::SetMimeType(const nsString &aMimeType)
{
  MutexAutoLock lock(mMutex);
  mMimeType = aMimeType;
}

void
MediaRecorder::GetMimeType(nsString &aMimeType)
{
  MutexAutoLock lock(mMutex);
  aMimeType = mMimeType;
}

void
MediaRecorder::Start(const Optional<int32_t>& aTimeSlice, ErrorResult& aResult)
{
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
  // Start a session
  mSession = new Session(this, timeSlice);
  mSession->Start();
}

void
MediaRecorder::Stop(ErrorResult& aResult)
{
  if (mState == RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mState = RecordingState::Inactive;

  mSession->Stop();
  mSession = nullptr;
}

void
MediaRecorder::Pause(ErrorResult& aResult)
{
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mSession != nullptr);
  if (mSession) {
    nsresult rv = mSession->Pause();
    if (NS_FAILED(rv)) {
      NotifyError(rv);
      return;
    }
    mState = RecordingState::Paused;
  }
}

void
MediaRecorder::Resume(ErrorResult& aResult)
{
  if (mState != RecordingState::Paused) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mSession != nullptr);
  if (mSession) {
    nsresult rv = mSession->Resume();
    if (NS_FAILED(rv)) {
      NotifyError(rv);
      return;
    }
    mState = RecordingState::Recording;
  }
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

  NS_DispatchToMainThread(
    new CreateAndDispatchBlobEventRunnable(mSession->GetEncodedData(), this),
    NS_DISPATCH_NORMAL);
}

JSObject*
MediaRecorder::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaRecorderBinding::Wrap(aCx, aScope, this);
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

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMRecordErrorEvent(getter_AddRefs(event), this, nullptr, nullptr);

  nsCOMPtr<nsIDOMRecordErrorEvent> errorEvent = do_QueryInterface(event);
  rv = errorEvent->InitRecordErrorEvent(NS_LITERAL_STRING("error"),
                                        false, false, errorMsg);

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

}
}
