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
#include "MediaDecoder.h"
#include "MediaEncoder.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/RecordErrorEvent.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"
#include "GeckoProfiler.h"
#include "nsContentTypeParser.h"
#include "nsCharSeparatedTokenizer.h"

#ifdef LOG
#undef LOG
#endif

mozilla::LazyLogModule gMediaRecorderLog("MediaRecorder");
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
class MediaRecorder::Session: public nsIObserver,
                              public PrincipalChangeObserver<MediaStreamTrack>,
                              public DOMMediaStream::TrackListener
{
  NS_DECL_THREADSAFE_ISUPPORTS

  // Main thread task.
  // Create a blob event and send back to client.
  class PushBlobRunnable : public Runnable
  {
  public:
    explicit PushBlobRunnable(Session* aSession)
      : mSession(aSession)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.PushBlobRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      RefPtr<MediaRecorder> recorder = mSession->mRecorder;
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
    RefPtr<Session> mSession;
  };

  // Notify encoder error, run in main thread task. (Bug 1095381)
  class EncoderErrorNotifierRunnable : public Runnable
  {
  public:
    explicit EncoderErrorNotifierRunnable(Session* aSession)
      : mSession(aSession)
    { }

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.ErrorNotifyRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      RefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
      }

      if (mSession->IsEncoderError()) {
        recorder->NotifyError(NS_ERROR_UNEXPECTED);
      }
      return NS_OK;
    }

  private:
    RefPtr<Session> mSession;
  };

  // Fire start event and set mimeType, run in main thread task.
  class DispatchStartEventRunnable : public Runnable
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
      RefPtr<MediaRecorder> recorder = mSession->mRecorder;

      recorder->SetMimeType(mSession->mMimeType);
      recorder->DispatchSimpleEvent(mEventName);

      return NS_OK;
    }

  private:
    RefPtr<Session> mSession;
    nsString mEventName;
  };

  // Record thread task and it run in Media Encoder thread.
  // Fetch encoded Audio/Video data from MediaEncoder.
  class ExtractRunnable : public Runnable
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
    RefPtr<Session> mSession;
  };

  // For Ensure recorder has tracks to record.
  class TracksAvailableCallback : public OnTracksAvailableCallback
  {
  public:
    explicit TracksAvailableCallback(Session *aSession)
     : mSession(aSession) {}
    virtual void NotifyTracksAvailable(DOMMediaStream* aStream)
    {
      if (mSession->mStopIssued) {
        return;
      }

      MOZ_RELEASE_ASSERT(aStream);
      mSession->MediaStreamReady(*aStream);

      uint8_t trackTypes = 0;
      nsTArray<RefPtr<mozilla::dom::AudioStreamTrack>> audioTracks;
      aStream->GetAudioTracks(audioTracks);
      if (!audioTracks.IsEmpty()) {
        trackTypes |= ContainerWriter::CREATE_AUDIO_TRACK;
        mSession->ConnectMediaStreamTrack(*audioTracks[0]);
      }

      nsTArray<RefPtr<mozilla::dom::VideoStreamTrack>> videoTracks;
      aStream->GetVideoTracks(videoTracks);
      if (!videoTracks.IsEmpty()) {
        trackTypes |= ContainerWriter::CREATE_VIDEO_TRACK;
        mSession->ConnectMediaStreamTrack(*videoTracks[0]);
      }

      if (audioTracks.Length() > 1 ||
          videoTracks.Length() > 1) {
        // When MediaRecorder supports multiple tracks, we should set up a single
        // MediaInputPort from the input stream, and let main thread check
        // track principals async later.
        nsPIDOMWindowInner* window = mSession->mRecorder->GetParentObject();
        nsIDocument* document = window ? window->GetExtantDoc() : nullptr;
        nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                        NS_LITERAL_CSTRING("Media"),
                                        document,
                                        nsContentUtils::eDOM_PROPERTIES,
                                        "MediaRecorderMultiTracksNotSupported");
        mSession->DoSessionEndTask(NS_ERROR_ABORT);
        return;
      }

      NS_ASSERTION(trackTypes != 0, "TracksAvailableCallback without any tracks available");

      // Check that we may access the tracks' content.
      if (!mSession->MediaStreamTracksPrincipalSubsumes()) {
        LOG(LogLevel::Warning, ("Session.NotifyTracksAvailable MediaStreamTracks principal check failed"));
        mSession->DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }

      LOG(LogLevel::Debug, ("Session.NotifyTracksAvailable track type = (%d)", trackTypes));
      mSession->InitEncoder(trackTypes);
    }
  private:
    RefPtr<Session> mSession;
  };
  // Main thread task.
  // To delete RecordingSession object.
  class DestroyRunnable : public Runnable
  {
  public:
    explicit DestroyRunnable(Session* aSession)
      : mSession(aSession) {}

    NS_IMETHODIMP Run()
    {
      LOG(LogLevel::Debug, ("Session.DestroyRunnable session refcnt = (%d) stopIssued %d s=(%p)",
                         (int)mSession->mRefCnt, mSession->mStopIssued, mSession.get()));
      MOZ_ASSERT(NS_IsMainThread() && mSession);
      RefPtr<MediaRecorder> recorder = mSession->mRecorder;
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
    RefPtr<Session> mSession;
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
    , mIsStartEventFired(false)
    , mIsRegisterProfiler(false)
    , mNeedSessionEndTask(true)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_COUNT_CTOR(MediaRecorder::Session);

    uint32_t maxMem = Preferences::GetUint("media.recorder.max_memory",
                                           MAX_ALLOW_MEMORY_BUFFER);
    mEncodedBufferCache = new EncodedBufferCache(maxMem);
    mLastBlobTimeStamp = TimeStamp::Now();
  }

  void PrincipalChanged(MediaStreamTrack* aTrack) override
  {
    NS_ASSERTION(mMediaStreamTracks.Contains(aTrack),
                 "Principal changed for unrecorded track");
    if (!MediaStreamTracksPrincipalSubsumes()) {
      DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override
  {
    LOG(LogLevel::Warning, ("Session.NotifyTrackAdded %p Raising error due to track set change", this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) override
  {
    RefPtr<MediaInputPort> foundInputPort;
    for (RefPtr<MediaInputPort> inputPort : mInputPorts) {
      if (aTrack->IsForwardedThrough(inputPort)) {
        foundInputPort = inputPort;
        break;
      }
    }

    if (foundInputPort) {
      // A recorded track was removed or ended. End it in the recording.
      // Don't raise an error.
      foundInputPort->Destroy();
      DebugOnly<bool> removed = mInputPorts.RemoveElement(foundInputPort);
      MOZ_ASSERT(removed);
      return;
    }

    LOG(LogLevel::Warning, ("Session.NotifyTrackRemoved %p Raising error due to track set change", this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void Start()
  {
    LOG(LogLevel::Debug, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    // Create a Track Union Stream
    MediaStreamGraph* gm = mRecorder->GetSourceMediaStream()->Graph();
    mTrackUnionStream = gm->CreateTrackUnionStream(nullptr);
    MOZ_ASSERT(mTrackUnionStream, "CreateTrackUnionStream failed");

    mTrackUnionStream->SetAutofinish(true);

    DOMMediaStream* domStream = mRecorder->Stream();
    if (domStream) {
      // Get the available tracks from the DOMMediaStream.
      // The callback will report back tracks that we have to connect to
      // mTrackUnionStream and listen to principal changes on.
      TracksAvailableCallback* tracksAvailableCallback = new TracksAvailableCallback(this);
      domStream->OnTracksAvailable(tracksAvailableCallback);
    } else {
      // Check that we may access the audio node's content.
      if (!AudioNodePrincipalSubsumes()) {
        LOG(LogLevel::Warning, ("Session.Start AudioNode principal check failed"));
        DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }
      // Bind this Track Union Stream with Source Media.
      RefPtr<MediaInputPort> inputPort =
        mTrackUnionStream->AllocateInputPort(mRecorder->GetSourceMediaStream());
      mInputPorts.AppendElement(inputPort.forget());
      MOZ_ASSERT(mInputPorts[mInputPorts.Length()-1]);

      // Web Audio node has only audio.
      InitEncoder(ContainerWriter::CREATE_AUDIO_TRACK);
    }
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
  }

  nsresult Pause()
  {
    LOG(LogLevel::Debug, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    mTrackUnionStream->Suspend();
    if (mEncoder) {
      mEncoder->Suspend();
    }

    return NS_OK;
  }

  nsresult Resume()
  {
    LOG(LogLevel::Debug, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());

    NS_ENSURE_TRUE(mTrackUnionStream, NS_ERROR_FAILURE);
    if (mEncoder) {
      mEncoder->Resume();
    }
    mTrackUnionStream->Resume();

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
    return (mEncoder ?  mEncoder->SizeOfExcludingThis(aMallocSizeOf) : 0);
  }


private:
  // Only DestroyRunnable is allowed to delete Session object.
  virtual ~Session()
  {
    MOZ_COUNT_DTOR(MediaRecorder::Session);
    LOG(LogLevel::Debug, ("Session.~Session (%p)", this));
    CleanupStreams();
    if (mReadThread) {
      mReadThread->Shutdown();
      mReadThread = nullptr;
      // Inside the if() so that if we delete after xpcom-shutdown's Observe(), we
      // won't try to remove it after the observer service is shut down.
      nsContentUtils::UnregisterShutdownObserver(this);
    }
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
        if (!mIsStartEventFired) {
          NS_DispatchToMainThread(
            new DispatchStartEventRunnable(this, NS_LITERAL_STRING("start")));
          mIsStartEventFired = true;
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
      // Fire the start event before the blob.
      if (!mIsStartEventFired) {
        NS_DispatchToMainThread(
          new DispatchStartEventRunnable(this, NS_LITERAL_STRING("start")));
        mIsStartEventFired = true;
      }
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

  void MediaStreamReady(DOMMediaStream& aStream) {
    mMediaStream = &aStream;
    aStream.RegisterTrackListener(this);
  }

  void ConnectMediaStreamTrack(MediaStreamTrack& aTrack)
  {
    mMediaStreamTracks.AppendElement(&aTrack);
    aTrack.AddPrincipalChangeObserver(this);
    RefPtr<MediaInputPort> inputPort =
      aTrack.ForwardTrackContentsTo(mTrackUnionStream);
    MOZ_ASSERT(inputPort);
    mInputPorts.AppendElement(inputPort.forget());
    MOZ_ASSERT(mInputPorts[mInputPorts.Length()-1]);
  }

  bool PrincipalSubsumes(nsIPrincipal* aPrincipal)
  {
    if (!mRecorder->GetOwner())
      return false;
    nsCOMPtr<nsIDocument> doc = mRecorder->GetOwner()->GetExtantDoc();
    if (!doc) {
      return false;
    }
    if (!aPrincipal) {
      return false;
    }
    bool subsumes;
    if (NS_FAILED(doc->NodePrincipal()->Subsumes(aPrincipal, &subsumes))) {
      return false;
    }
    return subsumes;
  }

  bool MediaStreamTracksPrincipalSubsumes()
  {
    MOZ_ASSERT(mRecorder->mDOMStream);
    nsCOMPtr<nsIPrincipal> principal = nullptr;
    for (RefPtr<MediaStreamTrack>& track : mMediaStreamTracks) {
      nsContentUtils::CombineResourcePrincipals(&principal, track->GetPrincipal());
    }
    return PrincipalSubsumes(principal);
  }

  bool AudioNodePrincipalSubsumes()
  {
    MOZ_ASSERT(mRecorder->mAudioNode != nullptr);
    nsIDocument* doc = mRecorder->mAudioNode->GetOwner()
                     ? mRecorder->mAudioNode->GetOwner()->GetExtantDoc()
                     : nullptr;
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
    return PrincipalSubsumes(principal);
  }

  bool CheckPermission(const char* aType)
  {
    if (!mRecorder || !mRecorder->GetOwner()) {
      return false;
    }

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
    pm->TestExactPermissionFromPrincipal(doc->NodePrincipal(), aType, &perm);
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
    if (mRecorder->mMimeType.EqualsLiteral(AUDIO_3GPP) && CheckPermission("audio-capture:3gpp")) {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(AUDIO_3GPP),
                                             mRecorder->GetAudioBitrate(),
                                             mRecorder->GetVideoBitrate(),
                                             mRecorder->GetBitrate(),
                                             aTrackTypes);
    } else if (mRecorder->mMimeType.EqualsLiteral(AUDIO_3GPP2) && CheckPermission("audio-capture:3gpp2")) {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(AUDIO_3GPP2),
                                             mRecorder->GetAudioBitrate(),
                                             mRecorder->GetVideoBitrate(),
                                             mRecorder->GetBitrate(),
                                             aTrackTypes);
    } else {
      mEncoder = MediaEncoder::CreateEncoder(NS_LITERAL_STRING(""),
                                             mRecorder->GetAudioBitrate(),
                                             mRecorder->GetVideoBitrate(),
                                             mRecorder->GetBitrate(),
                                             aTrackTypes);
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
    // Try to use direct listeners if possible
    DOMMediaStream* domStream = mRecorder->Stream();
    if (domStream && domStream->GetInputStream()) {
      mInputStream = domStream->GetInputStream()->AsSourceStream();
      if (mInputStream) {
        mInputStream->AddDirectListener(mEncoder);
        mEncoder->SetDirectConnect(true);
      }
    }

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
    NS_DispatchToMainThread(
      new DispatchStartEventRunnable(this, NS_LITERAL_STRING("start")));

    if (NS_FAILED(rv)) {
      NS_DispatchToMainThread(NewRunnableMethod<nsresult>(mRecorder,
                                                          &MediaRecorder::NotifyError, rv));
    }
    if (NS_FAILED(NS_DispatchToMainThread(new EncoderErrorNotifierRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread EncoderErrorNotifierRunnable failed");
    }
    if (rv != NS_ERROR_DOM_SECURITY_ERR) {
      // Don't push a blob if there was a security error.
      if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this)))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
      }
    }
    if (NS_FAILED(NS_DispatchToMainThread(new DestroyRunnable(this)))) {
      MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
    }
    mNeedSessionEndTask = false;
  }
  void CleanupStreams()
  {
    if (mInputStream) {
      if (mEncoder) {
        mInputStream->RemoveDirectListener(mEncoder);
      }
      mInputStream = nullptr;
    }
    for (RefPtr<MediaInputPort>& inputPort : mInputPorts) {
      MOZ_ASSERT(inputPort);
      inputPort->Destroy();
    }
    mInputPorts.Clear();

    if (mTrackUnionStream) {
      if (mEncoder) {
        mTrackUnionStream->RemoveListener(mEncoder);
      }
      mTrackUnionStream->Destroy();
      mTrackUnionStream = nullptr;
    }

    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    for (RefPtr<MediaStreamTrack>& track : mMediaStreamTracks) {
      track->RemovePrincipalChangeObserver(this);
    }
    mMediaStreamTracks.Clear();
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
      nsContentUtils::UnregisterShutdownObserver(this);
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
  RefPtr<MediaRecorder> mRecorder;

  // Receive track data from source and dispatch to Encoder.
  // Pause/ Resume controller.
  RefPtr<ProcessedMediaStream> mTrackUnionStream;
  RefPtr<SourceMediaStream> mInputStream;
  nsTArray<RefPtr<MediaInputPort>> mInputPorts;

  // Stream currently recorded.
  RefPtr<DOMMediaStream> mMediaStream;

  // Tracks currently recorded. This should be a subset of mMediaStream's track
  // set.
  nsTArray<RefPtr<MediaStreamTrack>> mMediaStreamTracks;

  // Runnable thread for read data from MediaEncode.
  nsCOMPtr<nsIThread> mReadThread;
  // MediaEncoder pipeline.
  RefPtr<MediaEncoder> mEncoder;
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
  // Indicate the session had fire start event. Encoding thread only.
  bool mIsStartEventFired;
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
                             nsPIDOMWindowInner* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
  mDOMStream = &aSourceMediaStream;

  RegisterActivityObserver();
}

MediaRecorder::MediaRecorder(AudioNode& aSrcAudioNode,
                             uint32_t aSrcOutput,
                             nsPIDOMWindowInner* aOwnerWindow)
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
    AudioNodeStream::Flags flags =
      AudioNodeStream::EXTERNAL_OUTPUT |
      AudioNodeStream::NEED_MAIN_THREAD_FINISHED;
    mPipeStream = AudioNodeStream::Create(ctx, engine, flags);
    AudioNodeStream* ns = aSrcAudioNode.GetStream();
    if (ns) {
      mInputPort =
        mPipeStream->AllocateInputPort(aSrcAudioNode.GetStream(),
                                       TRACK_ANY, TRACK_ANY,
                                       0, aSrcOutput);
    }
  }
  mAudioNode = &aSrcAudioNode;

  RegisterActivityObserver();
}

void
MediaRecorder::RegisterActivityObserver()
{
  if (nsPIDOMWindowInner* window = GetOwner()) {
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
  if (nsPIDOMWindowInner* window = GetOwner()) {
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

  nsTArray<RefPtr<MediaStreamTrack>> tracks;
  if (mDOMStream) {
    mDOMStream->GetTracks(tracks);
  }
  if (!tracks.IsEmpty()) {
    // If there are tracks already available that we're not allowed
    // to record, we should throw a security error.
    bool subsumes = false;
    nsPIDOMWindowInner* window;
    nsIDocument* doc;
    if (!(window = GetOwner()) ||
        !(doc = window->GetExtantDoc()) ||
        NS_FAILED(doc->NodePrincipal()->Subsumes(
                    mDOMStream->GetPrincipal(), &subsumes)) ||
        !subsumes) {
      aResult.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
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
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!IsTypeSupported(aInitDict.mMimeType)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  RefPtr<MediaRecorder> object = new MediaRecorder(aStream, ownerWindow);
  object->SetOptions(aInitDict);
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
    aRv.ThrowTypeError<MSG_DOES_NOT_IMPLEMENT_INTERFACE>(argStr, typeStr);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
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

  if (!IsTypeSupported(aInitDict.mMimeType)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  RefPtr<MediaRecorder> object = new MediaRecorder(aSrcAudioNode,
                                                     aSrcOutput,
                                                     ownerWindow);
  object->SetOptions(aInitDict);
  return object.forget();
}

void
MediaRecorder::SetOptions(const MediaRecorderOptions& aInitDict)
{
  SetMimeType(aInitDict.mMimeType);
  mAudioBitsPerSecond = aInitDict.mAudioBitsPerSecond.WasPassed() ?
                        aInitDict.mAudioBitsPerSecond.Value() : 0;
  mVideoBitsPerSecond = aInitDict.mVideoBitsPerSecond.WasPassed() ?
                        aInitDict.mVideoBitsPerSecond.Value() : 0;
  mBitsPerSecond = aInitDict.mBitsPerSecond.WasPassed() ?
                   aInitDict.mBitsPerSecond.Value() : 0;
  // We're not handling dynamic changes yet. Eventually we'll handle
  // setting audio, video and/or total -- and anything that isn't set,
  // we'll derive. Calculated versions require querying bitrates after
  // the encoder is Init()ed. This happens only after data is
  // available and thus requires dynamic changes.
  //
  // Until dynamic changes are supported, I prefer to be safe and err
  // slightly high
  if (aInitDict.mBitsPerSecond.WasPassed() && !aInitDict.mVideoBitsPerSecond.WasPassed()) {
    mVideoBitsPerSecond = mBitsPerSecond;
  }
}

static char const *const gWebMAudioEncoderCodecs[2] = {
  "opus",
  // no VP9 yet
  nullptr,
};
static char const *const gWebMVideoEncoderCodecs[4] = {
  "opus",
  "vp8",
  "vp8.0",
  // no VP9 yet
  nullptr,
};
static char const *const gOggAudioEncoderCodecs[2] = {
  "opus",
  // we could support vorbis here too, but don't
  nullptr,
};

template <class String>
static bool
CodecListContains(char const *const * aCodecs, const String& aCodec)
{
  for (int32_t i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i]))
      return true;
  }
  return false;
}

/* static */
bool
MediaRecorder::IsTypeSupported(GlobalObject& aGlobal, const nsAString& aMIMEType)
{
  return IsTypeSupported(aMIMEType);
}

/* static */
bool
MediaRecorder::IsTypeSupported(const nsAString& aMIMEType)
{
  char const* const* codeclist = nullptr;

  if (aMIMEType.IsEmpty()) {
    return true;
  }

  nsContentTypeParser parser(aMIMEType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return false;
  }

  // effectively a 'switch (mimeType) {'
  if (mimeType.EqualsLiteral(AUDIO_OGG)) {
    if (MediaDecoder::IsOggEnabled() && MediaDecoder::IsOpusEnabled()) {
      codeclist = gOggAudioEncoderCodecs;
    }
  }
#ifdef MOZ_WEBM_ENCODER
  else if (mimeType.EqualsLiteral(VIDEO_WEBM) &&
           MediaEncoder::IsWebMEncoderEnabled()) {
    codeclist = gWebMVideoEncoderCodecs;
  }
#endif
#ifdef MOZ_OMX_ENCODER
    // We're working on MP4 encoder support for desktop
  else if (mimeType.EqualsLiteral(VIDEO_MP4) ||
           mimeType.EqualsLiteral(AUDIO_3GPP) ||
           mimeType.EqualsLiteral(AUDIO_3GPP2)) {
    if (MediaEncoder::IsOMXEncoderEnabled()) {
      // XXX check codecs for MP4/3GPP
      return true;
    }
  }
#endif

  // codecs don't matter if we don't support the container
  if (!codeclist) {
    return false;
  }
  // now filter on codecs, and if needed rescind support
  nsAutoString codecstring;
  rv = parser.GetParameter("codecs", codecstring);

  nsTArray<nsString> codecs;
  if (!ParseCodecsString(codecstring, codecs)) {
    return false;
  }
  for (const nsString& codec : codecs) {
    if (!CodecListContains(codeclist, codec)) {
      // Totally unsupported codec
      return false;
    }
  }

  return true;
}

nsresult
MediaRecorder::CreateAndDispatchBlobEvent(already_AddRefed<nsIDOMBlob>&& aBlob)
{
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  nsCOMPtr<nsIDOMBlob> blob = aBlob;
  init.mData = static_cast<Blob*>(blob.get());

  RefPtr<BlobEvent> event =
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

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aStr, false, false);
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

  RefPtr<RecordErrorEvent> event =
    RecordErrorEvent::Constructor(this, NS_LITERAL_STRING("error"), init);
  event->SetTrusted(true);

  rv = DispatchDOMEvent(nullptr, event, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch the error event!!!");
    return;
  }
  return;
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
  nsPIDOMWindowInner* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  nsIDocument* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  LOG(LogLevel::Debug, ("MediaRecorder %p document IsActive %d isVisible %d\n",
                     this, doc->IsActive(), doc->IsVisible()));
  if (!doc->IsActive() || !doc->IsVisible()) {
    // Stop the session.
    ErrorResult result;
    Stop(result);
    result.SuppressException();
  }
}

MediaStream*
MediaRecorder::GetSourceMediaStream()
{
  if (mDOMStream != nullptr) {
    return mDOMStream->GetPlaybackStream();
  }
  MOZ_ASSERT(mAudioNode != nullptr);
  return mPipeStream ? mPipeStream.get() : mAudioNode->GetStream();
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

} // namespace dom
} // namespace mozilla
