/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaRecorder.h"

#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"
#include "MediaStreamGraphImpl.h"
#include "VideoUtils.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaRecorderErrorEvent.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "nsAutoPtr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentTypeParser.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

#ifdef LOG
#undef LOG
#endif

mozilla::LazyLogModule gMediaRecorderLog("MediaRecorder");
#define LOG(type, msg) MOZ_LOG(gMediaRecorderLog, type, msg)

namespace mozilla {

namespace dom {

using namespace mozilla::media;

/* static */ StaticRefPtr<nsIAsyncShutdownBlocker> gMediaRecorderShutdownBlocker;
static nsTHashtable<nsRefPtrHashKey<MediaRecorder::Session>> gSessions;

/**
 * MediaRecorderReporter measures memory being used by the Media Recorder.
 *
 * It is a singleton reporter and the single class object lives as long as at
 * least one Recorder is registered. In MediaRecorder, the reporter is unregistered
 * when it is destroyed.
 */
class MediaRecorderReporter final : public nsIMemoryReporter
{
public:
  static void AddMediaRecorder(MediaRecorder *aRecorder)
  {
    if (!sUniqueInstance) {
      sUniqueInstance = MakeAndAddRef<MediaRecorderReporter>();
      RegisterWeakAsyncMemoryReporter(sUniqueInstance);
    }
    sUniqueInstance->mRecorders.AppendElement(aRecorder);
  }

  static void RemoveMediaRecorder(MediaRecorder *aRecorder)
  {
    if (!sUniqueInstance) {
      return;
    }

    sUniqueInstance->mRecorders.RemoveElement(aRecorder);
    if (sUniqueInstance->mRecorders.IsEmpty()) {
      UnregisterWeakMemoryReporter(sUniqueInstance);
      sUniqueInstance = nullptr;
    }
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  MediaRecorderReporter() = default;

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport,
                 nsISupports* aData, bool aAnonymize) override
  {
    nsTArray<RefPtr<MediaRecorder::SizeOfPromise>> promises;
    for (const RefPtr<MediaRecorder>& recorder: mRecorders) {
      promises.AppendElement(recorder->SizeOfExcludingThis(MallocSizeOf));
    }

    nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
    nsCOMPtr<nsISupports> data = aData;
    MediaRecorder::SizeOfPromise::All(GetCurrentThreadSerialEventTarget(), promises)
      ->Then(GetCurrentThreadSerialEventTarget(), __func__,
          [handleReport, data](const nsTArray<size_t>& sizes) {
            nsCOMPtr<nsIMemoryReporterManager> manager =
              do_GetService("@mozilla.org/memory-reporter-manager;1");
            if (!manager) {
              return;
            }

            size_t sum = 0;
            for (const size_t& size : sizes) {
              sum += size;
            }

            handleReport->Callback(
              EmptyCString(), NS_LITERAL_CSTRING("explicit/media/recorder"),
              KIND_HEAP, UNITS_BYTES, sum,
              NS_LITERAL_CSTRING("Memory used by media recorder."),
              data);

            manager->EndReport();
          },
          [](size_t) { MOZ_CRASH("Unexpected reject"); });

    return NS_OK;
  }

private:
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  virtual ~MediaRecorderReporter()
  {
    MOZ_ASSERT(mRecorders.IsEmpty(), "All recorders must have been removed");
  }

  static StaticRefPtr<MediaRecorderReporter> sUniqueInstance;

  nsTArray<RefPtr<MediaRecorder>> mRecorders;
};
NS_IMPL_ISUPPORTS(MediaRecorderReporter, nsIMemoryReporter);

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaRecorder)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaRecorder,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSecurityDomException)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUnknownDomException)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaRecorder,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSecurityDomException)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUnknownDomException)
  tmp->UnRegisterActivityObserver();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaRecorder)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaRecorder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaRecorder, DOMEventTargetHelper)

/**
 * Session is an object to represent a single recording event.
 * In original design, all recording context is stored in MediaRecorder, which causes
 * a problem if someone calls MediaRecorder::Stop and MediaRecorder::Start quickly.
 * To prevent blocking main thread, media encoding is executed in a second thread,
 * named as Read Thread. For the same reason, we do not wait Read Thread shutdown in
 * MediaRecorder::Stop. If someone call MediaRecorder::Start before Read Thread shutdown,
 * the same recording context in MediaRecorder might be access by two Reading Threads,
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
 *    a reference to Session. Then the Session registers itself to a
 *    ShutdownBlocker and also holds a reference to MediaRecorder.
 *    Therefore, the reference dependency in gecko is:
 *    ShutdownBlocker -> Session <-> MediaRecorder, note that there is a cycle
 *    reference between Session and MediaRecorder.
 * 2) A Session is destroyed in DestroyRunnable after MediaRecorder::Stop being called
 *    _and_ all encoded media data been passed to OnDataAvailable handler.
 * 3) MediaRecorder::Stop is called by user or the document is going to
 *    inactive or invisible.
 */
class MediaRecorder::Session: public PrincipalChangeObserver<MediaStreamTrack>,
                              public DOMMediaStream::TrackListener
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Session)

  // Main thread task.
  // Create a blob event and send back to client.
  class PushBlobRunnable : public Runnable
                         , public MutableBlobStorageCallback
  {
  public:
    NS_DECL_ISUPPORTS_INHERITED

    // aDestroyRunnable can be null. If it's not, it will be dispatched after
    // the PushBlobRunnable::Run().
    PushBlobRunnable(Session* aSession, Runnable* aDestroyRunnable)
      : Runnable("dom::MediaRecorder::Session::PushBlobRunnable")
      , mSession(aSession)
      , mDestroyRunnable(aDestroyRunnable)
    { }

    NS_IMETHOD Run() override
    {
      LOG(LogLevel::Debug, ("Session.PushBlobRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      mSession->GetBlobWhenReady(this);
      return NS_OK;
    }

    void
    BlobStoreCompleted(MutableBlobStorage* aBlobStorage, Blob* aBlob,
                       nsresult aRv) override
    {
      RefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return;
      }

      if (NS_FAILED(aRv)) {
        mSession->DoSessionEndTask(aRv);
        return;
      }

      nsresult rv = recorder->CreateAndDispatchBlobEvent(aBlob);
      if (NS_FAILED(rv)) {
        mSession->DoSessionEndTask(aRv);
      }

      if (mDestroyRunnable &&
          NS_FAILED(NS_DispatchToMainThread(mDestroyRunnable.forget()))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread failed");
      }
    }

  private:
    ~PushBlobRunnable() = default;

    RefPtr<Session> mSession;

    // The generation of the blob is async. In order to avoid dispatching the
    // DestroyRunnable before pushing the blob event, we store the runnable
    // here.
    RefPtr<Runnable> mDestroyRunnable;
  };

  class StoreEncodedBufferRunnable final : public Runnable
  {
    RefPtr<Session> mSession;
    nsTArray<nsTArray<uint8_t>> mBuffer;

  public:
    StoreEncodedBufferRunnable(Session* aSession,
                               nsTArray<nsTArray<uint8_t>>&& aBuffer)
      : Runnable("StoreEncodedBufferRunnable")
      , mSession(aSession)
      , mBuffer(Move(aBuffer))
    {}

    NS_IMETHOD
    Run() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      mSession->MaybeCreateMutableBlobStorage();
      for (uint32_t i = 0; i < mBuffer.Length(); i++) {
        if (mBuffer[i].IsEmpty()) {
          continue;
        }

        nsresult rv = mSession->mMutableBlobStorage->Append(mBuffer[i].Elements(),
                                                            mBuffer[i].Length());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mSession->DoSessionEndTask(rv);
          break;
        }
      }

      return NS_OK;
    }
  };

  // Notify encoder error, run in main thread task. (Bug 1095381)
  class EncoderErrorNotifierRunnable : public Runnable
  {
  public:
    explicit EncoderErrorNotifierRunnable(Session* aSession)
      : Runnable("dom::MediaRecorder::Session::EncoderErrorNotifierRunnable")
      , mSession(aSession)
    { }

    NS_IMETHOD Run() override
    {
      LOG(LogLevel::Debug, ("Session.ErrorNotifyRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      RefPtr<MediaRecorder> recorder = mSession->mRecorder;
      if (!recorder) {
        return NS_OK;
      }

      recorder->NotifyError(NS_ERROR_UNEXPECTED);
      return NS_OK;
    }

  private:
    RefPtr<Session> mSession;
  };

  // Fire start event and set mimeType, run in main thread task.
  class DispatchStartEventRunnable : public Runnable
  {
  public:
    explicit DispatchStartEventRunnable(Session* aSession)
      : Runnable("dom::MediaRecorder::Session::DispatchStartEventRunnable")
      , mSession(aSession)
    { }

    NS_IMETHOD Run() override
    {
      LOG(LogLevel::Debug, ("Session.DispatchStartEventRunnable s=(%p)", mSession.get()));
      MOZ_ASSERT(NS_IsMainThread());

      NS_ENSURE_TRUE(mSession->mRecorder, NS_OK);
      RefPtr<MediaRecorder> recorder = mSession->mRecorder;

      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("start"));

      return NS_OK;
    }

  private:
    RefPtr<Session> mSession;
  };

  // To ensure that MediaRecorder has tracks to record.
  class TracksAvailableCallback : public OnTracksAvailableCallback
  {
  public:
    explicit TracksAvailableCallback(Session *aSession)
     : mSession(aSession) {}

    virtual void NotifyTracksAvailable(DOMMediaStream* aStream)
    {
      mSession->MediaStreamReady(aStream);
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
      : Runnable("dom::MediaRecorder::Session::DestroyRunnable")
      , mSession(aSession)
    {
    }

    explicit DestroyRunnable(already_AddRefed<Session> aSession)
      : Runnable("dom::MediaRecorder::Session::DestroyRunnable")
      , mSession(aSession)
    {
    }

    NS_IMETHOD Run() override
    {
      LOG(LogLevel::Debug, ("Session.DestroyRunnable session refcnt = (%d) s=(%p)",
                            static_cast<int>(mSession->mRefCnt), mSession.get()));
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
      if (mSession->mRunningState.isOk() &&
          mSession->mRunningState.unwrap() != RunningState::Stopping &&
          mSession->mRunningState.unwrap() != RunningState::Stopped) {
        recorder->StopForSessionDestruction();
        if (NS_FAILED(NS_DispatchToMainThread(new DestroyRunnable(mSession.forget())))) {
          MOZ_ASSERT(false, "NS_DispatchToMainThread failed");
        }
        return NS_OK;
      }

      if (mSession->mRunningState.isOk()) {
        mSession->mRunningState = RunningState::Stopped;
      }

      // Dispatch stop event and clear MIME type.
      mSession->mMimeType = NS_LITERAL_STRING("");
      recorder->SetMimeType(mSession->mMimeType);
      recorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));

      RefPtr<Session> session = mSession.forget();
      session->Shutdown()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [session]() {
          gSessions.RemoveEntry(session);
          if (gSessions.Count() == 0 &&
              gMediaRecorderShutdownBlocker) {
            // All sessions finished before shutdown, no need to keep the blocker.
            RefPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
            barrier->RemoveBlocker(gMediaRecorderShutdownBlocker);
            gMediaRecorderShutdownBlocker = nullptr;
          }
        },
        []() { MOZ_CRASH("Not reached"); });
      return NS_OK;
    }

  private:
    // Call mSession::Release automatically while DestroyRunnable be destroy.
    RefPtr<Session> mSession;
  };

  class EncoderListener : public MediaEncoderListener
  {
  public:
    EncoderListener(TaskQueue* aEncoderThread, Session* aSession)
      : mEncoderThread(aEncoderThread)
      , mSession(aSession)
    {}

    void Forget()
    {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      mSession = nullptr;
    }

    void Initialized() override
    {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderInitialized();
      }
    }

    void DataAvailable() override
    {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderDataAvailable();
      }
    }

    void Error() override
    {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderError();
      }
    }

    void Shutdown() override
    {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderShutdown();
      }
    }

  protected:
    RefPtr<TaskQueue> mEncoderThread;
    RefPtr<Session> mSession;
  };

  friend class EncoderErrorNotifierRunnable;
  friend class PushBlobRunnable;
  friend class DestroyRunnable;
  friend class TracksAvailableCallback;

public:
  Session(MediaRecorder* aRecorder, int32_t aTimeSlice)
    : mRecorder(aRecorder)
    , mTimeSlice(aTimeSlice)
    , mRunningState(RunningState::Idling)
  {
    MOZ_ASSERT(NS_IsMainThread());

    mMaxMemory = Preferences::GetUint("media.recorder.max_memory",
                                      MAX_ALLOW_MEMORY_BUFFER);
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
    if (aTrack->Ended()) {
      // TrackEncoder will pickup tracks that end itself.
      return;
    }

    MOZ_ASSERT(mEncoder);
    if (mEncoder) {
      mEncoder->RemoveMediaStreamTrack(aTrack);
    }

    LOG(LogLevel::Warning, ("Session.NotifyTrackRemoved %p Raising error due to track set change", this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void Start()
  {
    LOG(LogLevel::Debug, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    DOMMediaStream* domStream = mRecorder->Stream();
    if (domStream) {
      // The callback reports back when tracks are available and can be
      // attached to MediaEncoder. This allows `recorder.start()` before any tracks are available.
      // We have supported this historically and have mochitests assuming this behavior.
      TracksAvailableCallback* tracksAvailableCallback = new TracksAvailableCallback(this);
      domStream->OnTracksAvailable(tracksAvailableCallback);
      return;
    }

    if (mRecorder->mAudioNode) {
      // Check that we may access the audio node's content.
      if (!AudioNodePrincipalSubsumes()) {
        LOG(LogLevel::Warning, ("Session.Start AudioNode principal check failed"));
        DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }

      TrackRate trackRate = mRecorder->mAudioNode->Context()->Graph()->GraphRate();

      // Web Audio node has only audio.
      InitEncoder(ContainerWriter::CREATE_AUDIO_TRACK, trackRate);
      return;
    }

    MOZ_ASSERT(false, "Unknown source");
  }

  void Stop()
  {
    LOG(LogLevel::Debug, ("Session.Stop %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (mEncoder) {
      mEncoder->Stop();
    }

    if (mRunningState.isOk() &&
        mRunningState.unwrap() == RunningState::Idling) {
      LOG(LogLevel::Debug, ("Session.Stop Explicit end task %p", this));
      // End the Session directly if there is no ExtractRunnable.
      DoSessionEndTask(NS_OK);
    } else if (mRunningState.isOk() &&
               (mRunningState.unwrap() == RunningState::Starting ||
                mRunningState.unwrap() == RunningState::Running)) {
      mRunningState = RunningState::Stopping;
    }
  }

  nsresult Pause()
  {
    LOG(LogLevel::Debug, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mEncoder) {
      return NS_ERROR_FAILURE;
    }

    mEncoder->Suspend(TimeStamp::Now());
    return NS_OK;
  }

  nsresult Resume()
  {
    LOG(LogLevel::Debug, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mEncoder) {
      return NS_ERROR_FAILURE;
    }

    mEncoder->Resume(TimeStamp::Now());
    return NS_OK;
  }

  nsresult RequestData()
  {
    LOG(LogLevel::Debug, ("Session.RequestData"));
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this, nullptr)))) {
      MOZ_ASSERT(false, "RequestData NS_DispatchToMainThread failed");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  void
  MaybeCreateMutableBlobStorage()
  {
    if (!mMutableBlobStorage) {
      mMutableBlobStorage =
        new MutableBlobStorage(MutableBlobStorage::eCouldBeInTemporaryFile,
                               nullptr, mMaxMemory);
    }
  }

  void
  GetBlobWhenReady(MutableBlobStorageCallback* aCallback)
  {
    MOZ_ASSERT(NS_IsMainThread());

    MaybeCreateMutableBlobStorage();
    mMutableBlobStorage->GetBlobWhenReady(mRecorder->GetParentObject(),
                                          NS_ConvertUTF16toUTF8(mMimeType),
                                          aCallback);
    mMutableBlobStorage = nullptr;
  }

  RefPtr<SizeOfPromise>
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
  {
    MOZ_ASSERT(NS_IsMainThread());
    size_t encodedBufferSize = mMutableBlobStorage
                                 ? mMutableBlobStorage->SizeOfCurrentMemoryBuffer()
                                 : 0;

    if (!mEncoder) {
      return SizeOfPromise::CreateAndResolve(encodedBufferSize, __func__);
    }

    auto& encoder = mEncoder;
    return InvokeAsync(mEncoderThread, __func__,
      [encoder, encodedBufferSize, aMallocSizeOf]() {
        return SizeOfPromise::CreateAndResolve(
          encodedBufferSize + encoder->SizeOfExcludingThis(aMallocSizeOf), __func__);
      });
  }

private:
  // Only DestroyRunnable is allowed to delete Session object on main thread.
  virtual ~Session()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mShutdownPromise);
    LOG(LogLevel::Debug, ("Session.~Session (%p)", this));
  }
  // Pull encoded media data from MediaEncoder and put into MutableBlobStorage.
  // Destroy this session object in the end of this function.
  // If the bool aForceFlush is true, we will force to dispatch a
  // PushBlobRunnable to main thread.
  void Extract(bool aForceFlush, Runnable* aDestroyRunnable)
  {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    LOG(LogLevel::Debug, ("Session.Extract %p", this));

    AUTO_PROFILER_LABEL("MediaRecorder::Session::Extract", OTHER);

    // Pull encoded media data from MediaEncoder
    nsTArray<nsTArray<uint8_t> > encodedBuf;
    nsresult rv = mEncoder->GetEncodedData(&encodedBuf);
    if (NS_FAILED(rv)) {
      MOZ_RELEASE_ASSERT(encodedBuf.IsEmpty());
      // Even if we failed to encode more data, it might be time to push a blob
      // with already encoded data.
    }

    // Append pulled data into cache buffer.
    NS_DispatchToMainThread(new StoreEncodedBufferRunnable(this,
                                                           Move(encodedBuf)));

    // Whether push encoded data back to onDataAvailable automatically or we
    // need a flush.
    bool pushBlob = aForceFlush;
    if (!pushBlob &&
        mTimeSlice > 0 &&
        (TimeStamp::Now()-mLastBlobTimeStamp).ToMilliseconds() > mTimeSlice) {
      pushBlob = true;
    }
    if (pushBlob) {
      if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this, aDestroyRunnable)))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
      } else {
        mLastBlobTimeStamp = TimeStamp::Now();
      }
    } else if (aDestroyRunnable) {
      if (NS_FAILED(NS_DispatchToMainThread(aDestroyRunnable))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
      }
    }
  }

  void MediaStreamReady(DOMMediaStream* aStream) {
    MOZ_RELEASE_ASSERT(aStream);

    if (!mRunningState.isOk() || mRunningState.unwrap() != RunningState::Idling) {
      return;
    }

    mMediaStream = aStream;
    aStream->RegisterTrackListener(this);

    uint8_t trackTypes = 0;
    nsTArray<RefPtr<mozilla::dom::AudioStreamTrack>> audioTracks;
    aStream->GetAudioTracks(audioTracks);
    if (!audioTracks.IsEmpty()) {
      trackTypes |= ContainerWriter::CREATE_AUDIO_TRACK;
    }

    nsTArray<RefPtr<mozilla::dom::VideoStreamTrack>> videoTracks;
    aStream->GetVideoTracks(videoTracks);
    if (!videoTracks.IsEmpty()) {
      trackTypes |= ContainerWriter::CREATE_VIDEO_TRACK;
    }

    nsTArray<RefPtr<mozilla::dom::MediaStreamTrack>> tracks;
    aStream->GetTracks(tracks);
    for (auto& track : tracks) {
      if (track->Ended()) {
        continue;
      }

      ConnectMediaStreamTrack(*track);
    }

    if (audioTracks.Length() > 1 ||
        videoTracks.Length() > 1) {
      // When MediaRecorder supports multiple tracks, we should set up a single
      // MediaInputPort from the input stream, and let main thread check
      // track principals async later.
      nsPIDOMWindowInner* window = mRecorder->GetParentObject();
      nsIDocument* document = window ? window->GetExtantDoc() : nullptr;
      nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                      NS_LITERAL_CSTRING("Media"),
                                      document,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "MediaRecorderMultiTracksNotSupported");
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    NS_ASSERTION(trackTypes != 0, "TracksAvailableCallback without any tracks available");

    // Check that we may access the tracks' content.
    if (!MediaStreamTracksPrincipalSubsumes()) {
      LOG(LogLevel::Warning, ("Session.NotifyTracksAvailable MediaStreamTracks principal check failed"));
      DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    LOG(LogLevel::Debug, ("Session.NotifyTracksAvailable track type = (%d)", trackTypes));
    InitEncoder(trackTypes, aStream->GraphRate());
  }

  void ConnectMediaStreamTrack(MediaStreamTrack& aTrack)
  {
    for (auto& track : mMediaStreamTracks) {
      if (track->AsAudioStreamTrack() && aTrack.AsAudioStreamTrack()) {
        // We only allow one audio track. See bug 1276928.
        return;
      }
      if (track->AsVideoStreamTrack() && aTrack.AsVideoStreamTrack()) {
        // We only allow one video track. See bug 1276928.
        return;
      }
    }
    mMediaStreamTracks.AppendElement(&aTrack);
    aTrack.AddPrincipalChangeObserver(this);
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
    MOZ_ASSERT(mRecorder->mAudioNode);
    nsIDocument* doc = mRecorder->mAudioNode->GetOwner()
                       ? mRecorder->mAudioNode->GetOwner()->GetExtantDoc()
                       : nullptr;
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
    return PrincipalSubsumes(principal);
  }

  void InitEncoder(uint8_t aTrackTypes, TrackRate aTrackRate)
  {
    LOG(LogLevel::Debug, ("Session.InitEncoder %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mRunningState.isOk() || mRunningState.unwrap() != RunningState::Idling) {
      MOZ_ASSERT_UNREACHABLE("Double-init");
      return;
    }

    // Create a TaskQueue to read encode media data from MediaEncoder.
    MOZ_RELEASE_ASSERT(!mEncoderThread);
    RefPtr<SharedThreadPool> pool =
      GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER);
    if (!pool) {
      LOG(LogLevel::Debug, ("Session.InitEncoder %p Failed to create "
                            "MediaRecorderReadThread thread pool", this));
      DoSessionEndTask(NS_ERROR_FAILURE);
      return;
    }

    mEncoderThread =
      MakeAndAddRef<TaskQueue>(pool.forget(), "MediaRecorderReadThread");

    if (!gMediaRecorderShutdownBlocker) {
      // Add a shutdown blocker so mEncoderThread can be shutdown async.
      class Blocker : public ShutdownBlocker
      {
      public:
        Blocker()
          : ShutdownBlocker(NS_LITERAL_STRING(
              "MediaRecorder::Session: shutdown"))
        {}

        NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override
        {
          // Distribute the global async shutdown blocker in a ticket. If there
          // are zero graphs then shutdown is unblocked when we go out of scope.
          RefPtr<ShutdownTicket> ticket =
              MakeAndAddRef<ShutdownTicket>(gMediaRecorderShutdownBlocker);
          gMediaRecorderShutdownBlocker = nullptr;

          nsTArray<RefPtr<ShutdownPromise>> promises(gSessions.Count());
          for (auto iter = gSessions.Iter(); !iter.Done(); iter.Next()) {
            promises.AppendElement(iter.Get()->GetKey()->Shutdown());
          }
          gSessions.Clear();
          ShutdownPromise::All(GetCurrentThreadSerialEventTarget(), promises)->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [ticket]() mutable {
              MOZ_ASSERT(gSessions.Count() == 0);
              // Unblock shutdown
              ticket = nullptr;
            },
            []() { MOZ_CRASH("Not reached"); });
          return NS_OK;
        }
      };

      gMediaRecorderShutdownBlocker = MakeAndAddRef<Blocker>();
      RefPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
      nsresult rv = barrier->AddBlocker(gMediaRecorderShutdownBlocker,
                                        NS_LITERAL_STRING(__FILE__), __LINE__,
                                        NS_LITERAL_STRING("MediaRecorder::Session: shutdown"));
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    }

    gSessions.PutEntry(this);

    uint32_t audioBitrate = mRecorder->GetAudioBitrate();
    uint32_t videoBitrate = mRecorder->GetVideoBitrate();
    uint32_t bitrate = mRecorder->GetBitrate();
    if (bitrate > 0) {
      // There's a total cap set. We have to make sure the type-specific limits
      // are within range.
      if ((aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK) &&
          (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK) &&
          audioBitrate + videoBitrate > bitrate) {
        LOG(LogLevel::Info, ("Session.InitEncoder Bitrates higher than total cap. Recalculating."));
        double factor = bitrate / static_cast<double>(audioBitrate + videoBitrate);
        audioBitrate = static_cast<uint32_t>(audioBitrate * factor);
        videoBitrate = static_cast<uint32_t>(videoBitrate * factor);
      } else if ((aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK) &&
                 !(aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK)) {
        audioBitrate = std::min(audioBitrate, bitrate);
        videoBitrate = 0;
      } else if (!(aTrackTypes & ContainerWriter::CREATE_AUDIO_TRACK) &&
                 (aTrackTypes & ContainerWriter::CREATE_VIDEO_TRACK)) {
        audioBitrate = 0;
        videoBitrate = std::min(videoBitrate, bitrate);
      }
      MOZ_ASSERT(audioBitrate + videoBitrate <= bitrate);
    }

    // Allocate encoder and bind with union stream.
    // At this stage, the API doesn't allow UA to choose the output mimeType format.

    mEncoder = MediaEncoder::CreateEncoder(mEncoderThread,
                                           NS_LITERAL_STRING(""),
                                           audioBitrate, videoBitrate,
                                           aTrackTypes, aTrackRate);

    if (!mEncoder) {
      LOG(LogLevel::Error, ("Session.InitEncoder !mEncoder %p", this));
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    mEncoderListener = MakeAndAddRef<EncoderListener>(mEncoderThread, this);
    nsresult rv =
      mEncoderThread->Dispatch(
        NewRunnableMethod<RefPtr<EncoderListener>>(
          "mozilla::MediaEncoder::RegisterListener",
          mEncoder, &MediaEncoder::RegisterListener, mEncoderListener));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));

    if (mRecorder->mAudioNode) {
      mEncoder->ConnectAudioNode(mRecorder->mAudioNode,
                                 mRecorder->mAudioNodeOutput);
    }

    for (auto& track : mMediaStreamTracks) {
      mEncoder->ConnectMediaStreamTrack(track);
    }

    // Set mRunningState to Running so that ExtractRunnable/DestroyRunnable will
    // take the responsibility to end the session.
    mRunningState = RunningState::Starting;
  }

  // application should get blob and onstop event
  void DoSessionEndTask(nsresult rv)
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mRunningState.isErr()) {
      // We have already ended with an error.
      return;
    }

    if (mRunningState.isOk() &&
        mRunningState.unwrap() == RunningState::Stopped) {
      // We have already ended gracefully.
      return;
    }

    if (mRunningState.isOk() &&
        (mRunningState.unwrap() == RunningState::Idling ||
         mRunningState.unwrap() == RunningState::Starting)) {
      NS_DispatchToMainThread(new DispatchStartEventRunnable(this));
    }

    if (rv == NS_OK) {
      mRunningState = RunningState::Stopped;
    } else {
      mRunningState = Err(rv);
    }

    if (NS_FAILED(rv)) {
      mRecorder->ForceInactive();
      NS_DispatchToMainThread(
        NewRunnableMethod<nsresult>("dom::MediaRecorder::NotifyError",
                                    mRecorder,
                                    &MediaRecorder::NotifyError,
                                    rv));
    }

    RefPtr<Runnable> destroyRunnable = new DestroyRunnable(this);

    if (rv != NS_ERROR_DOM_SECURITY_ERR) {
      // Don't push a blob if there was a security error.
      if (NS_FAILED(NS_DispatchToMainThread(new PushBlobRunnable(this, destroyRunnable)))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread PushBlobRunnable failed");
      }
    } else {
      if (NS_FAILED(NS_DispatchToMainThread(destroyRunnable))) {
        MOZ_ASSERT(false, "NS_DispatchToMainThread DestroyRunnable failed");
      }
    }
  }

  void MediaEncoderInitialized()
  {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    // Pull encoded metadata from MediaEncoder
    nsTArray<nsTArray<uint8_t> > encodedBuf;
    nsString mime;
    nsresult rv = mEncoder->GetEncodedMetadata(&encodedBuf, mime);

    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false);
      return;
    }

    // Append pulled data into cache buffer.
    NS_DispatchToMainThread(new StoreEncodedBufferRunnable(this,
                                                           Move(encodedBuf)));

    RefPtr<Session> self = this;
    NS_DispatchToMainThread(NewRunnableFrom([self, mime]() {
      if (!self->mRecorder) {
        MOZ_ASSERT_UNREACHABLE("Recorder should be live");
        return NS_OK;
      }
      if (self->mRunningState.isOk()) {
        auto state = self->mRunningState.unwrap();
        if (state == RunningState::Starting || state == RunningState::Stopping) {
          if (state == RunningState::Starting) {
            // We set it to Running in the runnable since we can only assign
            // mRunningState on main thread. We set it before running the start
            // event runnable since that dispatches synchronously (and may cause
            // js calls to methods depending on mRunningState).
            self->mRunningState = RunningState::Running;
          }
          self->mMimeType = mime;
          self->mRecorder->SetMimeType(self->mMimeType);
          auto startEvent = MakeRefPtr<DispatchStartEventRunnable>(self);
          startEvent->Run();
        }
      }
      return NS_OK;
    }));
  }

  void MediaEncoderDataAvailable()
  {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    Extract(false, nullptr);
  }

  void MediaEncoderError()
  {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    NS_DispatchToMainThread(
      NewRunnableMethod<nsresult>(
        "dom::MediaRecorder::Session::DoSessionEndTask",
        this, &Session::DoSessionEndTask, NS_ERROR_FAILURE));
  }

  void MediaEncoderShutdown()
  {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    MOZ_ASSERT(mEncoder->IsShutdown());

    // For the stop event. Let's the creation of the blob to dispatch this runnable.
    RefPtr<Runnable> destroyRunnable = new DestroyRunnable(this);

    // Forces the last blob even if it's not time for it yet.
    Extract(true, destroyRunnable);

    // Clean up.
    mEncoderListener->Forget();
    DebugOnly<bool> unregistered =
      mEncoder->UnregisterListener(mEncoderListener);
    MOZ_ASSERT(unregistered);
  }

  RefPtr<ShutdownPromise> Shutdown()
  {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(LogLevel::Debug, ("Session Shutdown %p", this));

    if (mShutdownPromise) {
      return mShutdownPromise;
    }

    mShutdownPromise = ShutdownPromise::CreateAndResolve(true, __func__);
    RefPtr<Session> self = this;

    if (mEncoder) {
      auto& encoder = mEncoder;
      encoder->Cancel();

      MOZ_RELEASE_ASSERT(mEncoderListener);
      auto& encoderListener = mEncoderListener;
      mShutdownPromise = mShutdownPromise->Then(
        mEncoderThread, __func__,
        [encoder, encoderListener]() {
          encoder->UnregisterListener(encoderListener);
          encoderListener->Forget();
          return ShutdownPromise::CreateAndResolve(true, __func__);
        },
        []() {
          MOZ_ASSERT_UNREACHABLE("Unexpected reject");
          return ShutdownPromise::CreateAndReject(false, __func__);
        });
    }

    // Remove main thread state.
    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    {
      auto tracks(Move(mMediaStreamTracks));
      for (RefPtr<MediaStreamTrack>& track : tracks) {
        track->RemovePrincipalChangeObserver(this);
      }
    }

    // Break the cycle reference between Session and MediaRecorder.
    if (mRecorder) {
      mShutdownPromise = mShutdownPromise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self]() {
          self->mRecorder->RemoveSession(self);
          self->mRecorder = nullptr;
          return ShutdownPromise::CreateAndResolve(true, __func__);
        },
        []() {
          MOZ_ASSERT_UNREACHABLE("Unexpected reject");
          return ShutdownPromise::CreateAndReject(false, __func__);
        });
    }

    if (mEncoderThread) {
      RefPtr<TaskQueue>& encoderThread = mEncoderThread;
      mShutdownPromise = mShutdownPromise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [encoderThread]() {
          return encoderThread->BeginShutdown();
        },
        []() {
          MOZ_ASSERT_UNREACHABLE("Unexpected reject");
          return ShutdownPromise::CreateAndReject(false, __func__);
        });
    }

    return mShutdownPromise;
  }

private:
  enum class RunningState {
    Idling, // Session has been created
    Starting, // MediaEncoder started, waiting for data
    Running, // MediaEncoder has produced data
    Stopping, // Stop() has been called
    Stopped, // Session has stopped without any error
  };

  // Hold reference to MediaRecorder that ensure MediaRecorder is alive
  // if there is an active session. Access ONLY on main thread.
  RefPtr<MediaRecorder> mRecorder;

  // Stream currently recorded.
  RefPtr<DOMMediaStream> mMediaStream;

  // Tracks currently recorded. This should be a subset of mMediaStream's track
  // set.
  nsTArray<RefPtr<MediaStreamTrack>> mMediaStreamTracks;

  // Runnable thread for reading data from MediaEncoder.
  RefPtr<TaskQueue> mEncoderThread;
  // MediaEncoder pipeline.
  RefPtr<MediaEncoder> mEncoder;
  // Listener through which MediaEncoder signals us.
  RefPtr<EncoderListener> mEncoderListener;
  // Set in Shutdown() and resolved when shutdown is complete.
  RefPtr<ShutdownPromise> mShutdownPromise;
  // A buffer to cache encoded media data.
  RefPtr<MutableBlobStorage> mMutableBlobStorage;
  // Max memory to use for the MutableBlobStorage.
  uint64_t mMaxMemory;
  // Current session mimeType
  nsString mMimeType;
  // Timestamp of the last fired dataavailable event.
  TimeStamp mLastBlobTimeStamp;
  // The interval of passing encoded data from MutableBlobStorage to
  // onDataAvailable handler. "mTimeSlice < 0" means Session object does not
  // push encoded data to onDataAvailable, instead, it passive wait the client
  // side pull encoded data by calling requestData API.
  const int32_t mTimeSlice;
  // The session's current main thread state. The error type gets setwhen ending
  // a recording with an error. An NS_OK error is invalid.
  // Main thread only.
  Result<RunningState, nsresult> mRunningState;
};

NS_IMPL_ISUPPORTS_INHERITED0(MediaRecorder::Session::PushBlobRunnable, Runnable)

MediaRecorder::~MediaRecorder()
{
  LOG(LogLevel::Debug, ("~MediaRecorder (%p)", this));
  UnRegisterActivityObserver();
}

MediaRecorder::MediaRecorder(DOMMediaStream& aSourceMediaStream,
                             nsPIDOMWindowInner* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mAudioNodeOutput(0)
  , mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);
  mDOMStream = &aSourceMediaStream;

  RegisterActivityObserver();
}

MediaRecorder::MediaRecorder(AudioNode& aSrcAudioNode,
                             uint32_t aSrcOutput,
                             nsPIDOMWindowInner* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
  , mAudioNodeOutput(aSrcOutput)
  , mState(RecordingState::Inactive)
{
  MOZ_ASSERT(aOwnerWindow);

  mAudioNode = &aSrcAudioNode;

  RegisterActivityObserver();
}

void
MediaRecorder::RegisterActivityObserver()
{
  if (nsPIDOMWindowInner* window = GetOwner()) {
    mDocument = window->GetExtantDoc();
    if (mDocument) {
      mDocument->RegisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void
MediaRecorder::UnRegisterActivityObserver()
{
  if (mDocument) {
    mDocument->UnregisterActivityObserver(
      NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
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

  InitializeDomExceptions();

  if (mState != RecordingState::Inactive) {
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
  mStartTime = TimeStamp::Now();
  Telemetry::ScalarAdd(Telemetry::ScalarID::MEDIARECORDER_RECORDING_COUNT, 1);
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
  if (mState == RecordingState::Inactive) {
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
MediaRecorder::CreateAndDispatchBlobEvent(Blob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aBlob;

  RefPtr<BlobEvent> event =
    BlobEvent::Constructor(this,
                           NS_LITERAL_STRING("dataavailable"),
                           init);
  event->SetTrusted(true);
  bool dummy;
  return DispatchEvent(event, &dummy);
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

  bool dummy;
  rv = DispatchEvent(event, &dummy);
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
  MediaRecorderErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  // These DOMExceptions have been created earlier so they can contain stack
  // traces. We attach the appropriate one here to be fired. We should have
  // exceptions here, but defensively check.
  switch (aRv) {
    case NS_ERROR_DOM_SECURITY_ERR:
      if (!mSecurityDomException) {
        LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
          "mSecurityDomException was not initialized"));
        mSecurityDomException = DOMException::Create(NS_ERROR_DOM_SECURITY_ERR);
      }
      init.mError = mSecurityDomException.forget();
      break;
    default:
      if (!mUnknownDomException) {
        LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
          "mUnknownDomException was not initialized"));
        mUnknownDomException = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR);
      }
      LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
        "mUnknownDomException being fired for aRv: %X", uint32_t(aRv)));
      init.mError = mUnknownDomException.forget();
  }

  RefPtr<MediaRecorderErrorEvent> event = MediaRecorderErrorEvent::Constructor(
    this, NS_LITERAL_STRING("error"), init);
  event->SetTrusted(true);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to dispatch the error event!!!");
  }
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

void
MediaRecorder::ForceInactive()
{
  LOG(LogLevel::Debug, ("MediaRecorder.ForceInactive %p", this));
  mState = RecordingState::Inactive;
}

void
MediaRecorder::StopForSessionDestruction()
{
  LOG(LogLevel::Debug, ("MediaRecorder.StopForSessionDestruction %p", this));
  MediaRecorderReporter::RemoveMediaRecorder(this);
  // We do not perform a mState != RecordingState::Recording) check here as
  // we may already be inactive due to ForceInactive().
  mState = RecordingState::Inactive;
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->Stop();
  // This is a coarse calculation and does not reflect the duration of the
  // final recording for reasons such as pauses. However it allows us an idea
  // of how long people are running their recorders for.
  TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
  Telemetry::Accumulate(Telemetry::MEDIA_RECORDER_RECORDING_DURATION,
                        timeDelta.ToSeconds());
}

void
MediaRecorder::InitializeDomExceptions()
{
  mSecurityDomException = DOMException::Create(NS_ERROR_DOM_SECURITY_ERR);
  mUnknownDomException = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR);
}

RefPtr<MediaRecorder::SizeOfPromise>
MediaRecorder::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  MOZ_ASSERT(NS_IsMainThread());

  // The return type of a chained MozPromise cannot be changed, so we create a
  // holder for our desired return type and resolve that from All()->Then().
  auto holder = MakeRefPtr<Refcountable<MozPromiseHolder<SizeOfPromise>>>();
  RefPtr<SizeOfPromise> promise = holder->Ensure(__func__);

  nsTArray<RefPtr<SizeOfPromise>> promises(mSessions.Length());
  for (const RefPtr<Session>& session : mSessions) {
    promises.AppendElement(session->SizeOfExcludingThis(aMallocSizeOf));
  }

  SizeOfPromise::All(GetCurrentThreadSerialEventTarget(), promises)->Then(
    GetCurrentThreadSerialEventTarget(), __func__,
    [holder](const nsTArray<size_t>& sizes) {
      size_t total = 0;
      for (const size_t& size : sizes) {
        total += size;
      }
      holder->Resolve(total, __func__);
    },
    []() {
      MOZ_CRASH("Unexpected reject");
    });

  return promise;
}

StaticRefPtr<MediaRecorderReporter> MediaRecorderReporter::sUniqueInstance;

} // namespace dom
} // namespace mozilla
