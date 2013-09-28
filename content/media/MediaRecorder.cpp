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

namespace mozilla {

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(MediaRecorder, nsDOMEventTargetHelper,
                                     mStream)

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
 *    Setup media streams in MSG, and bind MediaEncoder with Source Stream.
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
class MediaRecorder::Session
{
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
      nsresult rv = recorder->CreateAndDispatchBlobEvent(mSession);
      if (NS_FAILED(rv)) {
        recorder->NotifyError(rv);
      }

      return NS_OK;
    }

  private:
    Session *mSession;
  };

  // Record thread task.
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
      return NS_OK;
    }

  private:
    Session *mSession;
  };

  // Main thread task.
  // To delete RecordingSession object.
  class DestroyRunnable : public nsRunnable
  {
  public:
    DestroyRunnable(Session *aSession)
      : mSession(aSession) {}

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_IsMainThread() && mSession.get());
      MediaRecorder *recorder = mSession->mRecorder;

      // If MediaRecoder is not in Inactive mode, call MediaRecoder::Stop
      // and dispatch DestroyRunnable again.
      if (recorder->mState != RecordingState::Inactive) {
        ErrorResult result;
        recorder->Stop(result);
        NS_DispatchToMainThread(new DestroyRunnable(mSession.forget()));
        return NS_OK;
      }

      // Dispatch stop event and clear MIME type.
      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));
      recorder->SetMimeType(NS_LITERAL_STRING(""));

      // Delete session object.
      mSession = nullptr;

      return NS_OK;
    }

  private:
    nsAutoPtr<Session> mSession;
  };

  friend class PushBlobRunnable;
  friend class ExtractRunnable;
  friend class DestroyRunnable;

public:
  Session(MediaRecorder* aRecorder, int32_t aTimeSlice)
    : mRecorder(aRecorder),
      mTimeSlice(aTimeSlice)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mEncodedBufferCache = new EncodedBufferCache(MAX_ALLOW_MEMORY_BUFFER);
  }

  // Only DestroyRunnable is allowed to delete Session object.
  ~Session()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mInputPort.get()) {
      mInputPort->Destroy();
    }

    if (mTrackUnionStream.get()) {
      mTrackUnionStream->Destroy();
    }
  }

  void Start()
  {
    MOZ_ASSERT(NS_IsMainThread());

    SetupStreams();

    // Create a thread to read encode media data from MediaEncoder.
    if (!mReadThread) {
      nsresult rv = NS_NewNamedThread("Media Encoder", getter_AddRefs(mReadThread));
      if (NS_FAILED(rv)) {
        if (mInputPort.get()) {
          mInputPort->Destroy();
        }
        if (mTrackUnionStream.get()) {
          mTrackUnionStream->Destroy();
        }
        mRecorder->NotifyError(rv);
        return;
      }
    }

    mReadThread->Dispatch(new ExtractRunnable(this), NS_DISPATCH_NORMAL);
  }

  void Stop()
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Shutdown mEncoder to stop Session::Extract
    if (mInputPort.get())
    {
      mInputPort->Destroy();
      mInputPort = nullptr;
    }

    if (mTrackUnionStream.get())
    {
      mTrackUnionStream->Destroy();
      mTrackUnionStream = nullptr;
    }
  }

  void Pause()
  {
    MOZ_ASSERT(NS_IsMainThread() && mTrackUnionStream);

    mTrackUnionStream->ChangeExplicitBlockerCount(-1);
  }

  void Resume()
  {
    MOZ_ASSERT(NS_IsMainThread() && mTrackUnionStream);

    mTrackUnionStream->ChangeExplicitBlockerCount(1);
  }

  already_AddRefed<nsIDOMBlob> GetEncodedData()
  {
    nsString mimeType;
    mRecorder->GetMimeType(mimeType);
    return mEncodedBufferCache->ExtractBlob(mimeType);
  }

private:

  // Pull encoded meida data from MediaEncoder and put into EncodedBufferCache.
  // Destroy this session object in the end of this function.
  void Extract()
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mReadThread);

    TimeStamp lastBlobTimeStamp = TimeStamp::Now();
    // Whether push encoded data back to onDataAvailable automatically.
    const bool pushBlob = (mTimeSlice > 0) ? true : false;

    do {
      // Pull encoded media data from MediaEncoder
      nsTArray<nsTArray<uint8_t> > encodedBuf;
      nsString mimeType;
      mEncoder->GetEncodedData(&encodedBuf, mimeType);

      mRecorder->SetMimeType(mimeType);

      // Append pulled data into cache buffer.
      for (uint32_t i = 0; i < encodedBuf.Length(); i++) {
        mEncodedBufferCache->AppendBuffer(encodedBuf[i]);
      }

      if (pushBlob) {
        if ((TimeStamp::Now() - lastBlobTimeStamp).ToMilliseconds() > mTimeSlice) {
          NS_DispatchToMainThread(new PushBlobRunnable(this));
          lastBlobTimeStamp = TimeStamp::Now();
        }
      }
    } while (!mEncoder->IsShutdown());

    // Flush out remainding encoded data.
    NS_DispatchToMainThread(new PushBlobRunnable(this));

    // Destroy this session object in main thread.
    NS_DispatchToMainThread(new DestroyRunnable(this));
  }

  // Bind media source with MediaEncoder to receive raw media data.
  void SetupStreams()
  {
    MOZ_ASSERT(NS_IsMainThread());

    MediaStreamGraph* gm = mRecorder->mStream->GetStream()->Graph();
    mTrackUnionStream = gm->CreateTrackUnionStream(nullptr);
    MOZ_ASSERT(mTrackUnionStream, "CreateTrackUnionStream failed");

    mTrackUnionStream->SetAutofinish(true);

    mInputPort = mTrackUnionStream->AllocateInputPort(mRecorder->mStream->GetStream(), MediaInputPort::FLAG_BLOCK_OUTPUT);

    // Allocate encoder and bind with union stream.
    mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(""));
    MOZ_ASSERT(mEncoder, "CreateEncoder failed");

    if (mEncoder) {
      mTrackUnionStream->AddListener(mEncoder);
    }
  }

private:
  // Hold a reference to MediaRecoder to make sure MediaRecoder be
  // destroyed after all session object dead.
  nsRefPtr<MediaRecorder> mRecorder;

  // Pause/ Resume controller.
  nsRefPtr<ProcessedMediaStream> mTrackUnionStream;
  nsRefPtr<MediaInputPort> mInputPort;

  // Runnable thread for read data from MediaEncode.
  nsCOMPtr<nsIThread> mReadThread;
  // MediaEncoder pipeline.
  nsRefPtr<MediaEncoder> mEncoder;
  // A buffer to cache encoded meda data.
  nsAutoPtr<EncodedBufferCache> mEncodedBufferCache;
  // The interval of passing encoded data from EncodedBufferCache to onDataAvailable
  // handler. "mTimeSlice < 0" means Session object does not push encoded data to
  // onDataAvailable, instead, it passive wait the client side pull encoded data
  // by calling requestData API.
  const int32_t mTimeSlice;
};

MediaRecorder::~MediaRecorder()
{
  MOZ_ASSERT(mSession == nullptr);
}

void
MediaRecorder::Init(nsPIDOMWindow* aOwnerWindow)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
  BindToOwner(aOwnerWindow);
}

MediaRecorder::MediaRecorder(DOMMediaStream& aStream)
  : mState(RecordingState::Inactive),
    mSession(nullptr),
    mMutex("Session.Data.Mutex")
{
  mStream = &aStream;
  SetIsDOMBinding();
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

  mSession->Stop();
  mSession = nullptr;

  mState = RecordingState::Inactive;
}

void
MediaRecorder::Pause(ErrorResult& aResult)
{
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mState = RecordingState::Paused;

  MOZ_ASSERT(mSession != nullptr);
  if (mSession) {
    mSession->Pause();
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
    mSession->Resume();
    mState = RecordingState::Recording;
  }
}

void
MediaRecorder::RequestData(ErrorResult& aResult)
{
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  NS_DispatchToMainThread(
    NS_NewRunnableMethodWithArg<Session *>(this,
                                           &MediaRecorder::CreateAndDispatchBlobEvent, mSession),
    NS_DISPATCH_NORMAL);
}

JSObject*
MediaRecorder::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaRecorderBinding::Wrap(aCx, aScope, this);
}

/* static */ already_AddRefed<MediaRecorder>
MediaRecorder::Constructor(const GlobalObject& aGlobal,
                           DOMMediaStream& aStream, ErrorResult& aRv)
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

  nsRefPtr<MediaRecorder> object = new MediaRecorder(aStream);
  object->Init(ownerWindow);
  return object.forget();
}

nsresult
MediaRecorder::CreateAndDispatchBlobEvent(Session *aSession)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (!CheckPrincipal()) {
    // Media is not same-origin, don't allow the data out.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  BlobEventInitInitializer init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aSession->GetEncodedData();
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
