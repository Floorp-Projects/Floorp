/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaRecorder.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DOMMediaStream.h"
#include "EncodedBufferCache.h"
#include "MediaEncoder.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/RecordErrorEvent.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "GeckoProfiler.h"

#ifdef LOG
#undef LOG
#endif

PRLogModuleInfo* gMediaRecorderLog;
#define LOG(type, msg) MOZ_LOG(gMediaRecorderLog, type, msg)

namespace mozilla {

namespace dom {

/**
+ * MediaRecorderReporter measures memory being used by the Media Recorder.
+ *
+ * It is a singleton reporter and the single class object lives as long as at
+ * least one Recorder is registered. In MediaRecorder, the reporter is unregistered
+ * when it is destroyed.
+ */
class MediaRecorderReporter final : public nsIMemoryReporter
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  MediaRecorderReporter() {};
  static MediaRecorderReporter* UniqueInstance();
  void InitMemoryReporter();

  static void AddMediaRecorder(MediaRecorder *aRecorder)
  {
    GetRecorders().AppendElement(aRecorder);
  }

  static void RemoveMediaRecorder(MediaRecorder *aRecorder)
  {
    RecordersArray& recorders = GetRecorders();
    recorders.RemoveElement(aRecorder);
    if (recorders.IsEmpty()) {
      sUniqueInstance = nullptr;
    }
  }

  NS_METHOD
  CollectReports(nsIHandleReportCallback* aHandleReport,
                 nsISupports* aData, bool aAnonymize) override
  {
    int64_t amount = 0;
    RecordersArray& recorders = GetRecorders();
    for (size_t i = 0; i < recorders.Length(); ++i) {
      amount += recorders[i]->SizeOfExcludingThis(MallocSizeOf);
    }

  #define MEMREPORT(_path, _amount, _desc)                                    \
    do {                                                                      \
      nsresult rv;                                                            \
      rv = aHandleReport->Callback(EmptyCString(), NS_LITERAL_CSTRING(_path), \
                                   KIND_HEAP, UNITS_BYTES, _amount,           \
                                   NS_LITERAL_CSTRING(_desc), aData);         \
      NS_ENSURE_SUCCESS(rv, rv);                                              \
    } while (0)

    MEMREPORT("explicit/media/recorder", amount,
              "Memory used by media recorder.");

    return NS_OK;
  }

private:
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)
  virtual ~MediaRecorderReporter();
  static StaticRefPtr<MediaRecorderReporter> sUniqueInstance;
  typedef nsTArray<MediaRecorder*> RecordersArray;
  static RecordersArray& GetRecorders()
  {
    return UniqueInstance()->mRecorders;
  }
  RecordersArray mRecorders;
};
NS_IMPL_ISUPPORTS(MediaRecorderReporter, nsIMemoryReporter);

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaRecorder, DOMEventTargetHelper,
                                   mDOMStream, mAudioNode)

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
 * 1) MediaRecorder creates a Session in MediaRecorder::Start function and holds
 *    a reference to Session. Then the Session registers itself to
 *    ShutdownObserver and also holds a reference to MediaRecorder.
 *    Therefore, the reference dependency in gecko is:
 *    ShutdownObserver -> Session <-> MediaRecorder, note that there is a cycle
 *    reference between Session and MediaRecorder.
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
    explicit PushBlobRunnable(Session* aSession)
      : mSession(aSession)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.PushBlobRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      nsRefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
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

  // Notify encoder error, run in main thread task. (Bug 1095381)
  class EncoderErrorNotifierRunnable : public nsRunnable
  {
  public:
    explicit EncoderErrorNotifierRunnable(Session* aSession)
      : mSession(aSession)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.ErrorNotifyRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      nsRefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
      }

      if (mSession->IsEncoderError()) {
        recorder->NotifyError(NS_ERROR_UNEXPECTED);
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
      LOG(LogLevel::Debug, ("Session.DispatchStartEventRunnable s=(%p)", mSession.get()));
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
    explicit ExtractRunnable(Session* aSession)
      : mSession(aSession) {}

    ~ExtractRunnable()
    {}

    NS_IMETHODIMP Run()
    {
      MOZ_ASSERT(NS_GetCurrentThread() == mSession->mReadThread);

      LOG(LogLevel::Debug, ("Session.ExtractRunnable shutdown = %d", mSession->mEncoder->IsShutdown()));
      if (!mSession->mEncoder->IsShutdown()) {
        mSession->Extract(false);
        nsCOMPtr<nsIRunnable> event = new ExtractRunnable(mSession);
        if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
          NS_WARNING("Failed to dispatch ExtractRunnable to encoder thread");
        }
      } else {
        // Flush out remaining encoded data.
        mSession->Extract(true);
        if (mSession->mIsRegisterProfiler)
           profiler_unregister_thread();
        if (NS_FAILED(NS_DispatchToMainThread(
                        new DestroyRunnable(mSession)))) {
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
    explicit TracksAvailableCallback(Session *aSession)
     : mSession(aSession) {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream)
    {
      uint8_t trackTypes = 0;
      nsTArray<nsRefPtr<mozilla::dom::AudioStreamTrack>> audioTracks;
      aStream->GetAudioTracks(audioTracks);
      if (!audioTracks.IsEmpty()) {
        trackTypes |= ContainerWriter::CREATE_AUDIO_TRACK;
      }

      nsTArray<nsRefPtr<mozilla::dom::VideoStreamTrack>> videoTracks;
      aStream->GetVideoTracks(videoTracks);
      if (!videoTracks.IsEmpty()) {
        trackTypes |= ContainerWriter::CREATE_VIDEO_TRACK;
      }

      LOG(LogLevel::Debug, ("Session.NotifyTracksAvailable track type = (%d)", trackTypes));
      mSession->InitEncoder(trackTypes);
    }
  private:
    nsRefPtr<Session> mSession;
  };
  // Main thread task.
  // To delete RecordingSession object.
  class DestroyRunnable : public nsRunnable
  {
  public:
    explicit DestroyRunnable(Session* aSession)
      : mSession(aSession) {}

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.DestroyRunnable session refcnt = (%d) stopIssued %d s=(%p)",
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
        if (NS_FAILED(NS_DispatchToMainThread(new DestroyRunnable(mSession)))) {
          MOZ_ASSERT(false, "NS_DispatchToMainThread failed");
        }
        return NS_OK;
      }

      // Dispatch stop event and clear MIME type.
      mSession->mMimeType = NS_LITERAL_STRING("");
      recorder->SetMimeType(mSession->mMimeType);
      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));
      mSession->BreakCycle();
      return NS_OK;
    }

  private:
    // Call mSession::Release automatically while DestroyRunnable be destroy.
    nsRefPtr<Session> mSession;
  };

  friend class EncoderErrorNotifierRunnable;
  friend class PushBlobRunnable;
  friend class ExtractRunnable;
  friend class DestroyRunnable;
  friend class TracksAvailableCallback;

public:
  Session(MediaRecorder* aRecorder, int32_t aTimeSlice)
    : mRecorder(aRecorder)
    , mTimeSlice(aTimeSlice)
    , mStopIssued(false)
    , mCanRetrieveData(false)
    , mIsRegisterProfiler(false)
    , mNeedSessionEndTask(true)
  {
    MOZ_ASSERT(NS_IsMainThread());

    uint32_t maxMem = Preferences::GetUint("media.recorder.max_memory",
                                           MAX_ALLOW_MEMORY_BUFFER);
    mEncodedBufferCache = new EncodedBufferCache(maxMem);
    mLastBlobTimeStamp = TimeStamp::Now();
  }

  void Start()
  {
    LOG(LogLevel::Debug, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    SetupStreams();
  }

  void Stop()
  {
    LOG(LogLevel::Debug, ("Session.Stop %p", this));
    MOZ_ASSERT(NS_IsMainThread());
    mStopIssued = true;
    CleanupStreams();
    if (mNeedSessionEndTask) {
      LOG(LogLevel::Debug, ("Session.Stop mNeedSessionEndTask %p", this));
      // End the Session directly if there is no ExtractRunnable.
      DoSessionEndTask(NS_OK);
    }
    nsContentUtils::UnregisterShutdownObserver(this);
  }

  nsresult Pause()
  {
    LOG(LogLevel::Debug, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(1);

    return NS_OK;
  }

  nsresult Resume()
  {
    LOG(LogLevel::Debug, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->ChangeExplicitBlockerCount(-1);

    return NS_OK;
  }

  nsresult RequestData()
  {
    LOG(LogLevel::Debug, ("Session.RequestData"));
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(NS_DispatchToMainThread(new EncoderErrorNotifierRunnable(this))) ||
        NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this)))) {
      MOZ_ASSERT(false, "RequestData NS_DispatchToMainThread failed");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  already_AddRefed<nsIDOMBlob> GetEncodedData()
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mEncodedBufferCache->ExtractBlob(mRecorder->GetParentObject(),
                                            mMimeType);
  }

  bool IsEncoderError()
  {
    if (mEncoder && mEncoder->HasError()) {
      return true;
    }
    return false;
  }

  size_t
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t amount = mEncoder->SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }


private:
  // Only DestroyRunnable is allowed to delete Session object.
  virtual ~Session()
  {
    LOG(LogLevel::Debug, ("Session.~Session (%p)", this));
    CleanupStreams();
  }
  // Pull encoded media data from MediaEncoder and put into EncodedBufferCache.
  // Destroy this session object in the end of this function.
  // If the bool aForceFlush is true, we will force to dispatch a
  // PushBlobRunnable to main thread.
  void Extract(bool aForceFlush)
  {
    MOZ_ASSERT(NS_GetCurrentThread() == mReadThread);
    LOG(LogLevel::Debug, ("Session.Extract %p", this));

    if (!mIsRegisterProfiler) {
      char aLocal;
      profiler_register_thread("Media_Encoder", &aLocal);
      mIsRegisterProfiler = true;
    }

    PROFILER_LABEL("MediaRecorder", "Session Extract",
      js::ProfileEntry::Category::OTHER);

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
      if (NS_FAILED(NS_DispatchToMainThread(new EncoderErrorNotifierRunnable(this)))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread EncoderErrorNotifierRunnable failed");
      }
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
    MediaStreamGraph* gm = mRecorder->GetSourceMediaStream()->Graph();
    mTrackUnionStream = gm->CreateTrackUnionStream(nullptr);
    MOZ_ASSERT(mTrackUnionStream, "CreateTrackUnionStream failed");

    mTrackUnionStream->SetAutofinish(true);

    // Bind this Track Union Stream with Source Media.
    mInputPort = mTrackUnionStream->AllocateInputPort(mRecorder->GetSourceMediaStream(),
                                                      MediaInputPort::FLAG_BLOCK_OUTPUT);

    DOMMediaStream* domStream = mRecorder->Stream();
    if (domStream) {
      // Get the track type hint from DOM media stream.
      TracksAvailableCallback* tracksAvailableCallback = new TracksAvailableCallback(this);
      domStream->OnTracksAvailable(tracksAvailableCallback);
    } else {
      // Web Audio node has only audio.
      InitEncoder(ContainerWriter::CREATE_AUDIO_TRACK);
    }
  }

  bool Check3gppPermission()
  {
    nsCOMPtr<nsIDocument> doc = mRecorder->GetOwner()->GetExtantDoc();
    if (!doc) {
      return false;
    }

    uint16_t appStatus = nsIPrincipal::APP_STATUS_NOT_INSTALLED;
    doc->NodePrincipal()->GetAppStatus(&appStatus);

    // Certified applications can always assign AUDIO_3GPP
    if (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED) {
      return true;
    }

    nsCOMPtr<nsIPermissionManager> pm =
       do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

    if (!pm) {
      return false;
    }

    uint32_t perm = nsIPermissionManager::DENY_ACTION;
    pm->TestExactPermissionFromPrincipal(doc->NodePrincipal(), "audio-capture:3gpp", &perm);
    return perm == nsIPermissionManager::ALLOW_ACTION;
  }

  void InitEncoder(uint8_t aTrackTypes)
  {
    LOG(LogLevel::Debug, ("Session.InitEncoder %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mRecorder) {
      LOG(LogLevel::Debug, ("Session.InitEncoder failure, mRecorder is null %p", this));
      return;
    }
    // Allocate encoder and bind with union stream.
    // At this stage, the API doesn't allow UA to choose the output mimeType format.

    // Make sure the application has permission to assign AUDIO_3GPP
    if (mRecorder->mMimeType.EqualsLiteral(AUDIO_3GPP) && Check3gppPermission()) {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(AUDIO_3GPP), aTrackTypes);
    } else {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(""), aTrackTypes);
    }

    if (!mEncoder) {
      LOG(LogLevel::Debug, ("Session.InitEncoder !mEncoder %p", this));
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    // Media stream is ready but UA issues a stop method follow by start method.
    // The Session::stop would clean the mTrackUnionStream. If the AfterTracksAdded
    // comes after stop command, this function would crash.
    if (!mTrackUnionStream) {
      LOG(LogLevel::Debug, ("Session.InitEncoder !mTrackUnionStream %p", this));
      DoSessionEndTask(NS_OK);
      return;
    }
    mTrackUnionStream->AddListener(mEncoder);
    // Create a thread to read encode media data from MediaEncoder.
    if (!mReadThread) {
      nsresult rv = NS_NewNamedThread("Media_Encoder", getter_AddRefs(mReadThread));
      if (NS_FAILED(rv)) {
        LOG(LogLevel::Debug, ("Session.InitEncoder !mReadThread %p", this));
        DoSessionEndTask(rv);
        return;
      }
    }

    // In case source media stream does not notify track end, receive
    // shutdown notification and stop Read Thread.
    nsContentUtils::RegisterShutdownObserver(this);

    nsCOMPtr<nsIRunnable> event = new ExtractRunnable(this);
    if (NS_FAILED(mReadThread->Dispatch(event, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch ExtractRunnable at beginning");
      LOG(LogLevel::Debug, ("Session.InitEncoder !ReadThread->Dispatch %p", this));
      DoSessionEndTask(NS_ERROR_ABORT);
    }
    // Set mNeedSessionEndTask to false because the
    // ExtractRunnable/DestroyRunnable will take the response to
    // end the session.
    mNeedSessionEndTask = false;
  }
  // application should get blob and onstop event
  void DoSessionEndTask(nsresult rv)
  {
    MOZ_ASSERT(NS_IsMainThread());
    CleanupStreams();
    if (NS_FAILED(rv)) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethodWithArg<nsresult>(mRecorder,
                                              &MediaRecorder::NotifyError, rv);
      NS_DispatchToMainThread(runnable);
    }
    if (NS_FAILED(NS_DispatchToMainThread(new EncoderErrorNotifierRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread EncoderErrorNotifierRunnable failed");
    }
    if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
    }
    if (NS_FAILED(NS_DispatchToMainThread(new DestroyRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
    }
    mNeedSessionEndTask = false;
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

  NS_IMETHODIMP Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(LogLevel::Debug, ("Session.Observe XPCOM_SHUTDOWN %p", this));
    if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
      // Force stop Session to terminate Read Thread.
      mEncoder->Cancel();
      if (mReadThread) {
        mReadThread->Shutdown();
        mReadThread = nullptr;
      }
      BreakCycle();
      Stop();
    }

    return NS_OK;
  }

  // Break the cycle reference between Session and MediaRecorder.
  void BreakCycle()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mRecorder) {
      mRecorder->RemoveSession(this);
      mRecorder = nullptr;
    }
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
  // The register flag for "Media_Encoder" thread to profiler
  bool mIsRegisterProfiler;
  // False if the InitEncoder called successfully, ensure the
  // ExtractRunnable/DestroyRunnable will end the session.
  // Main thread only.
  bool mNeedSessionEndTask;
};

NS_IMPL_ISUPPORTS(MediaRecorder::Session, nsIObserver)

MediaRecorder::~MediaRecorder()
{
  if (mPipeStream != nullptr) {
    mInputPort->Destroy();
    mPipeStream->Destroy();
  }
  LOG(LogLevel::Debug, ("~MediaRecorder (%p)", this));
  UnRegisterActivityObserver();
}

MediaRecorder::MediaRecorder(DOMMediaStream& aSourceMediaStream,
                             nsPIDOMWindow* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
  mDOMStream = &aSourceMediaStream;
  if (!gMediaRecorderLog) {
    gMediaRecorderLog = PR_NewLogModule("MediaRecorder");
  }
  RegisterActivityObserver();
}

MediaRecorder::MediaRecorder(AudioNode& aSrcAudioNode,
                             uint32_t aSrcOutput,
                             nsPIDOMWindow* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());

  // Only AudioNodeStream of kind EXTERNAL_STREAM stores output audio data in
  // the track (see AudioNodeStream::AdvanceOutputSegment()). That means track
  // union stream in recorder session won't be able to copy data from the
  // stream of non-destination node. Create a pipe stream in this case.
  if (aSrcAudioNode.NumberOfOutputs() > 0) {
    AudioContext* ctx = aSrcAudioNode.Context();
    AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
    mPipeStream = ctx->Graph()->CreateAudioNodeStream(engine,
                                                      MediaStreamGraph::EXTERNAL_STREAM,
                                                      ctx->SampleRate());
    AudioNodeStream* ns = aSrcAudioNode.GetStream();
    if (ns) {
      mInputPort = mPipeStream->AllocateInputPort(aSrcAudioNode.GetStream(),
                                                  MediaInputPort::FLAG_BLOCK_INPUT,
                                                  0,
                                                  aSrcOutput);
    }
  }
  mAudioNode = &aSrcAudioNode;
  if (!gMediaRecorderLog) {
    gMediaRecorderLog = PR_NewLogModule("MediaRecorder");
  }
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
  LOG(LogLevel::Debug, ("MediaRecorder.Start %p", this));
  if (mState != RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (GetSourceMediaStream()->IsFinished() || GetSourceMediaStream()->IsDestroyed()) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Check if source media stream is valid. See bug 919051.
  if (mDOMStream && !mDOMStream->GetPrincipal()) {
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
  MediaRecorderReporter::AddMediaRecorder(this);
  mState = RecordingState::Recording;
  // Start a session.
  mSessions.AppendElement();
  mSessions.LastElement() = new Session(this, timeSlice);
  mSessions.LastElement()->Start();
}

void
MediaRecorder::Stop(ErrorResult& aResult)
{
  LOG(LogLevel::Debug, ("MediaRecorder.Stop %p", this));
  MediaRecorderReporter::RemoveMediaRecorder(this);
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
  LOG(LogLevel::Debug, ("MediaRecorder.Pause"));
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
  LOG(LogLevel::Debug, ("MediaRecorder.Resume"));
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

void
MediaRecorder::RequestData(ErrorResult& aResult)
{
  if (mState != RecordingState::Recording) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  MOZ_ASSERT(mSessions.Length() > 0);
  nsresult rv = mSessions.LastElement()->RequestData();
  if (NS_FAILED(rv)) {
    NotifyError(rv);
  }
}

JSObject*
MediaRecorder::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaRecorderBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<MediaRecorder>
MediaRecorder::Constructor(const GlobalObject& aGlobal,
                           DOMMediaStream& aStream,
                           const MediaRecorderOptions& aInitDict,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<MediaRecorder> object = new MediaRecorder(aStream, ownerWindow);
  object->SetMimeType(aInitDict.mMimeType);
  return object.forget();
}

/* static */ already_AddRefed<MediaRecorder>
MediaRecorder::Constructor(const GlobalObject& aGlobal,
                           AudioNode& aSrcAudioNode,
                           uint32_t aSrcOutput,
                           const MediaRecorderOptions& aInitDict,
                           ErrorResult& aRv)
{
  // Allow recording from audio node only when pref is on.
  if (!Preferences::GetBool("media.recorder.audio_node.enabled", false)) {
    // Pretending that this constructor is not defined.
    NS_NAMED_LITERAL_STRING(argStr, "Argument 1 of MediaRecorder.constructor");
    NS_NAMED_LITERAL_STRING(typeStr, "MediaStream");
    aRv.ThrowTypeError(MSG_DOES_NOT_IMPLEMENT_INTERFACE, &argStr, &typeStr);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // aSrcOutput doesn't matter to destination node because it has no output.
  if (aSrcAudioNode.NumberOfOutputs() > 0 &&
       aSrcOutput >= aSrcAudioNode.NumberOfOutputs()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<MediaRecorder> object = new MediaRecorder(aSrcAudioNode,
                                                     aSrcOutput,
                                                     ownerWindow);
  object->SetMimeType(aInitDict.mMimeType);
  return object.forget();
}

nsresult
MediaRecorder::CreateAndDispatchBlobEvent(already_AddRefed<nsIDOMBlob>&& aBlob)
{
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  if (!CheckPrincipal()) {
    // Media is not same-origin, don't allow the data out.
    nsRefPtr<nsIDOMBlob> blob = aBlob;
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  nsCOMPtr<nsIDOMBlob> blob = aBlob;
  init.mData = static_cast<Blob*>(blob.get());

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
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
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
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
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
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  if (!mDOMStream && !mAudioNode) {
    return false;
  }
  if (!GetOwner())
    return false;
  nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
  if (!doc) {
    return false;
  }
  nsIPrincipal* srcPrincipal = GetSourcePrincipal();
  if (!srcPrincipal) {
    return false;
  }
  bool subsumes;
  if (NS_FAILED(doc->NodePrincipal()->Subsumes(srcPrincipal, &subsumes))) {
    return false;
  }
  return subsumes;
}

void
MediaRecorder::RemoveSession(Session* aSession)
{
  LOG(LogLevel::Debug, ("MediaRecorder.RemoveSession (%p)", aSession));
  mSessions.RemoveElement(aSession);
}

void
MediaRecorder::NotifyOwnerDocumentActivityChanged()
{
  nsPIDOMWindow* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  nsIDocument* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  LOG(LogLevel::Debug, ("MediaRecorder %p document IsActive %d isVisible %d\n",
                     this, doc->IsActive(), doc->IsVisible()));
  if (!doc->IsActive() || !doc->IsVisible()) {
    // Stop the session.
    ErrorResult result;
    Stop(result);
  }
}

MediaStream*
MediaRecorder::GetSourceMediaStream()
{
  if (mDOMStream != nullptr) {
    return mDOMStream->GetStream();
  }
  MOZ_ASSERT(mAudioNode != nullptr);
  return mPipeStream ? mPipeStream.get() : mAudioNode->GetStream();
}

nsIPrincipal*
MediaRecorder::GetSourcePrincipal()
{
  if (mDOMStream != nullptr) {
    return mDOMStream->GetPrincipal();
  }
  MOZ_ASSERT(mAudioNode != nullptr);
  nsIDocument* doc = mAudioNode->GetOwner()->GetExtantDoc();
  return doc ? doc->NodePrincipal() : nullptr;
}

size_t
MediaRecorder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t amount = 42;
  for (size_t i = 0; i < mSessions.Length(); ++i) {
    amount += mSessions[i]->SizeOfExcludingThis(aMallocSizeOf);
  }
  return amount;
}

StaticRefPtr<MediaRecorderReporter> MediaRecorderReporter::sUniqueInstance;

MediaRecorderReporter* MediaRecorderReporter::UniqueInstance()
{
  if (!sUniqueInstance) {
    sUniqueInstance = new MediaRecorderReporter();
    sUniqueInstance->InitMemoryReporter();
  }
  return sUniqueInstance;
 }

void MediaRecorderReporter::InitMemoryReporter()
{
  RegisterWeakMemoryReporter(this);
}

MediaRecorderReporter::~MediaRecorderReporter()
{
  UnregisterWeakMemoryReporter(this);
}

}
}
