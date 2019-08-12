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
#include "nsDocShell.h"
#include "nsError.h"
#include "mozilla/dom/Document.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

mozilla::LazyLogModule gMediaRecorderLog("MediaRecorder");
#define LOG(type, msg) MOZ_LOG(gMediaRecorderLog, type, msg)

namespace mozilla {

namespace dom {

using namespace mozilla::media;

/* static */ StaticRefPtr<nsIAsyncShutdownBlocker>
    gMediaRecorderShutdownBlocker;
static nsTHashtable<nsRefPtrHashKey<MediaRecorder::Session>> gSessions;

/**
 * MediaRecorderReporter measures memory being used by the Media Recorder.
 *
 * It is a singleton reporter and the single class object lives as long as at
 * least one Recorder is registered. In MediaRecorder, the reporter is
 * unregistered when it is destroyed.
 */
class MediaRecorderReporter final : public nsIMemoryReporter {
 public:
  static void AddMediaRecorder(MediaRecorder* aRecorder) {
    if (!sUniqueInstance) {
      sUniqueInstance = MakeAndAddRef<MediaRecorderReporter>();
      RegisterWeakAsyncMemoryReporter(sUniqueInstance);
    }
    sUniqueInstance->mRecorders.AppendElement(aRecorder);
  }

  static void RemoveMediaRecorder(MediaRecorder* aRecorder) {
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
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    nsTArray<RefPtr<MediaRecorder::SizeOfPromise>> promises;
    for (const RefPtr<MediaRecorder>& recorder : mRecorders) {
      promises.AppendElement(recorder->SizeOfExcludingThis(MallocSizeOf));
    }

    nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
    nsCOMPtr<nsISupports> data = aData;
    MediaRecorder::SizeOfPromise::All(GetCurrentThreadSerialEventTarget(),
                                      promises)
        ->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
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
                  NS_LITERAL_CSTRING("Memory used by media recorder."), data);

              manager->EndReport();
            },
            [](size_t) { MOZ_CRASH("Unexpected reject"); });

    return NS_OK;
  }

 private:
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  virtual ~MediaRecorderReporter() {
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
 * In original design, all recording context is stored in MediaRecorder, which
 * causes a problem if someone calls MediaRecorder::Stop and
 * MediaRecorder::Start quickly. To prevent blocking main thread, media encoding
 * is executed in a second thread, named as Read Thread. For the same reason, we
 * do not wait Read Thread shutdown in MediaRecorder::Stop. If someone call
 * MediaRecorder::Start before Read Thread shutdown, the same recording context
 * in MediaRecorder might be access by two Reading Threads, which cause a
 * problem. In the new design, we put recording context into Session object,
 * including Read Thread.  Each Session has its own recording context and Read
 * Thread, problem is been resolved.
 *
 * Life cycle of a Session object.
 * 1) Initialization Stage (in main thread)
 *    Setup media streams in MSG, and bind MediaEncoder with Source Stream when
 * mStream is available. Resource allocation, such as encoded data cache buffer
 * and MediaEncoder. Create read thread. Automatically switch to Extract stage
 * in the end of this stage. 2) Extract Stage (in Read Thread) Pull encoded A/V
 * frames from MediaEncoder, dispatch to OnDataAvailable handler. Unless a
 * client calls Session::Stop, Session object keeps stay in this stage. 3)
 * Destroy Stage (in main thread) Switch from Extract stage to Destroy stage by
 * calling Session::Stop. Release session resource and remove associated streams
 * from MSG.
 *
 * Lifetime of MediaRecorder and Session objects.
 * 1) MediaRecorder creates a Session in MediaRecorder::Start function and holds
 *    a reference to Session. Then the Session registers itself to a
 *    ShutdownBlocker and also holds a reference to MediaRecorder.
 *    Therefore, the reference dependency in gecko is:
 *    ShutdownBlocker -> Session <-> MediaRecorder, note that there is a cycle
 *    reference between Session and MediaRecorder.
 * 2) A Session is destroyed after MediaRecorder::Stop has been called _and_ all
 * encoded media data has been passed to OnDataAvailable handler. 3)
 * MediaRecorder::Stop is called by user or the document is going to inactive or
 * invisible.
 */
class MediaRecorder::Session : public PrincipalChangeObserver<MediaStreamTrack>,
                               public DOMMediaStream::TrackListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Session)

  class StoreEncodedBufferRunnable final : public Runnable {
    RefPtr<Session> mSession;
    nsTArray<nsTArray<uint8_t>> mBuffer;

   public:
    StoreEncodedBufferRunnable(Session* aSession,
                               nsTArray<nsTArray<uint8_t>>&& aBuffer)
        : Runnable("StoreEncodedBufferRunnable"),
          mSession(aSession),
          mBuffer(std::move(aBuffer)) {}

    NS_IMETHOD
    Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      mSession->MaybeCreateMutableBlobStorage();
      for (uint32_t i = 0; i < mBuffer.Length(); i++) {
        if (mBuffer[i].IsEmpty()) {
          continue;
        }

        nsresult rv = mSession->mMutableBlobStorage->Append(
            mBuffer[i].Elements(), mBuffer[i].Length());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mSession->DoSessionEndTask(rv);
          break;
        }
      }

      return NS_OK;
    }
  };

  // Fire a named event, run in main thread task.
  class DispatchEventRunnable : public Runnable {
   public:
    explicit DispatchEventRunnable(Session* aSession,
                                   const nsAString& aEventName)
        : Runnable("dom::MediaRecorder::Session::DispatchEventRunnable"),
          mSession(aSession),
          mEventName(aEventName) {}

    NS_IMETHOD Run() override {
      LOG(LogLevel::Debug,
          ("Session.DispatchEventRunnable s=(%p) e=(%s)", mSession.get(),
           NS_ConvertUTF16toUTF8(mEventName).get()));
      MOZ_ASSERT(NS_IsMainThread());

      NS_ENSURE_TRUE(mSession->mRecorder, NS_OK);
      mSession->mRecorder->DispatchSimpleEvent(mEventName);

      return NS_OK;
    }

   private:
    RefPtr<Session> mSession;
    nsString mEventName;
  };

  class EncoderListener : public MediaEncoderListener {
   public:
    EncoderListener(TaskQueue* aEncoderThread, Session* aSession)
        : mEncoderThread(aEncoderThread), mSession(aSession) {}

    void Forget() {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      mSession = nullptr;
    }

    void Initialized() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderInitialized();
      }
    }

    void DataAvailable() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderDataAvailable();
      }
    }

    void Error() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderError();
      }
    }

    void Shutdown() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderShutdown();
      }
    }

   protected:
    RefPtr<TaskQueue> mEncoderThread;
    RefPtr<Session> mSession;
  };

 public:
  Session(MediaRecorder* aRecorder, uint32_t aTimeSlice)
      : mRecorder(aRecorder),
        mMediaStreamReady(false),
        mMainThread(mRecorder->GetOwner()->EventTargetFor(TaskCategory::Other)),
        mTimeSlice(aTimeSlice),
        mStartTime(TimeStamp::Now()),
        mRunningState(RunningState::Idling) {
    MOZ_ASSERT(NS_IsMainThread());

    aRecorder->GetMimeType(mMimeType);
    mMaxMemory = Preferences::GetUint("media.recorder.max_memory",
                                      MAX_ALLOW_MEMORY_BUFFER);
    mLastBlobTimeStamp = mStartTime;
    Telemetry::ScalarAdd(Telemetry::ScalarID::MEDIARECORDER_RECORDING_COUNT, 1);
  }

  void PrincipalChanged(MediaStreamTrack* aTrack) override {
    NS_ASSERTION(mMediaStreamTracks.Contains(aTrack),
                 "Principal changed for unrecorded track");
    if (!MediaStreamTracksPrincipalSubsumes()) {
      DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override {
    LOG(LogLevel::Warning,
        ("Session.NotifyTrackAdded %p Raising error due to track set change",
         this));
    if (mMediaStreamReady) {
      DoSessionEndTask(NS_ERROR_ABORT);
    }

    NS_DispatchToMainThread(
        NewRunnableMethod("MediaRecorder::Session::MediaStreamReady", this,
                          &Session::MediaStreamReady));
    return;
  }

  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) override {
    if (!mMediaStreamReady) {
      // We haven't chosen the track set to record yet.
      return;
    }

    if (aTrack->Ended()) {
      // TrackEncoder will pickup tracks that end itself.
      return;
    }

    MOZ_ASSERT(mEncoder);
    if (mEncoder) {
      mEncoder->RemoveMediaStreamTrack(aTrack);
    }

    LOG(LogLevel::Warning,
        ("Session.NotifyTrackRemoved %p Raising error due to track set change",
         this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void Start() {
    LOG(LogLevel::Debug, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    DOMMediaStream* domStream = mRecorder->Stream();
    if (domStream) {
      // The callback reports back when tracks are available and can be
      // attached to MediaEncoder. This allows `recorder.start()` before any
      // tracks are available. We have supported this historically and have
      // mochitests assuming this behavior.
      mMediaStream = domStream;
      mMediaStream->RegisterTrackListener(this);
      nsTArray<RefPtr<MediaStreamTrack>> tracks(2);
      mMediaStream->GetTracks(tracks);
      for (const auto& track : tracks) {
        // Notify of existing tracks, as the stream doesn't do this by itself.
        NotifyTrackAdded(track);
      }
      return;
    }

    if (mRecorder->mAudioNode) {
      // Check that we may access the audio node's content.
      if (!AudioNodePrincipalSubsumes()) {
        LOG(LogLevel::Warning,
            ("Session.Start AudioNode principal check failed"));
        DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }

      TrackRate trackRate =
          mRecorder->mAudioNode->Context()->Graph()->GraphRate();

      // Web Audio node has only audio.
      InitEncoder(ContainerWriter::CREATE_AUDIO_TRACK, trackRate);
      return;
    }

    MOZ_ASSERT(false, "Unknown source");
  }

  void Stop() {
    LOG(LogLevel::Debug, ("Session.Stop %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (mEncoder) {
      mEncoder->Stop();
    }

    // Remove main thread state added in Start().
    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    {
      auto tracks(std::move(mMediaStreamTracks));
      for (RefPtr<MediaStreamTrack>& track : tracks) {
        track->RemovePrincipalChangeObserver(this);
      }
    }

    if (mRunningState.isOk() &&
        mRunningState.unwrap() == RunningState::Idling) {
      LOG(LogLevel::Debug, ("Session.Stop Explicit end task %p", this));
      // End the Session directly if there is no encoder.
      DoSessionEndTask(NS_OK);
    } else if (mRunningState.isOk() &&
               (mRunningState.unwrap() == RunningState::Starting ||
                mRunningState.unwrap() == RunningState::Running)) {
      mRunningState = RunningState::Stopping;
    }
  }

  nsresult Pause() {
    LOG(LogLevel::Debug, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mEncoder) {
      return NS_ERROR_FAILURE;
    }

    mEncoder->Suspend();
    NS_DispatchToMainThread(
        new DispatchEventRunnable(this, NS_LITERAL_STRING("pause")));
    return NS_OK;
  }

  nsresult Resume() {
    LOG(LogLevel::Debug, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mEncoder) {
      return NS_ERROR_FAILURE;
    }

    mEncoder->Resume();
    NS_DispatchToMainThread(
        new DispatchEventRunnable(this, NS_LITERAL_STRING("resume")));
    return NS_OK;
  }

  void RequestData() {
    LOG(LogLevel::Debug, ("Session.RequestData"));
    MOZ_ASSERT(NS_IsMainThread());

    GatherBlob()->Then(
        mMainThread, __func__,
        [this, self = RefPtr<Session>(this)](
            const BlobPromise::ResolveOrRejectValue& aResult) {
          if (aResult.IsReject()) {
            LOG(LogLevel::Warning, ("GatherBlob failed for RequestData()"));
            DoSessionEndTask(aResult.RejectValue());
            return;
          }

          nsresult rv =
              mRecorder->CreateAndDispatchBlobEvent(aResult.ResolveValue());
          if (NS_FAILED(rv)) {
            DoSessionEndTask(NS_OK);
          }
        });
  }

  void MaybeCreateMutableBlobStorage() {
    if (!mMutableBlobStorage) {
      mMutableBlobStorage = new MutableBlobStorage(
          MutableBlobStorage::eCouldBeInTemporaryFile, nullptr, mMaxMemory);
    }
  }

  static const bool IsExclusive = true;
  using BlobPromise =
      MozPromise<nsMainThreadPtrHandle<Blob>, nsresult, IsExclusive>;
  class BlobStorer : public MutableBlobStorageCallback {
    MozPromiseHolder<BlobPromise> mHolder;

    virtual ~BlobStorer() = default;

   public:
    BlobStorer() = default;

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlobStorer, override)

    void BlobStoreCompleted(MutableBlobStorage*, Blob* aBlob,
                            nsresult aRv) override {
      MOZ_ASSERT(NS_IsMainThread());
      if (NS_FAILED(aRv)) {
        mHolder.Reject(aRv, __func__);
      } else {
        mHolder.Resolve(nsMainThreadPtrHandle<Blob>(
                            MakeAndAddRef<nsMainThreadPtrHolder<Blob>>(
                                "BlobStorer::ResolveBlob", aBlob)),
                        __func__);
      }
    }

    RefPtr<BlobPromise> Promise() { return mHolder.Ensure(__func__); }
  };

  // Stops gathering data into the current blob and resolves when the current
  // blob is available. Future data will be stored in a new blob.
  RefPtr<BlobPromise> GatherBlob() {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<BlobStorer> storer = MakeAndAddRef<BlobStorer>();
    MaybeCreateMutableBlobStorage();
    mMutableBlobStorage->GetBlobWhenReady(
        mRecorder->GetOwner(), NS_ConvertUTF16toUTF8(mMimeType), storer);
    mMutableBlobStorage = nullptr;

    return storer->Promise();
  }

  RefPtr<SizeOfPromise> SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) {
    MOZ_ASSERT(NS_IsMainThread());
    size_t encodedBufferSize =
        mMutableBlobStorage ? mMutableBlobStorage->SizeOfCurrentMemoryBuffer()
                            : 0;

    if (!mEncoder) {
      return SizeOfPromise::CreateAndResolve(encodedBufferSize, __func__);
    }

    auto& encoder = mEncoder;
    return InvokeAsync(
        mEncoderThread, __func__,
        [encoder, encodedBufferSize, aMallocSizeOf]() {
          return SizeOfPromise::CreateAndResolve(
              encodedBufferSize + encoder->SizeOfExcludingThis(aMallocSizeOf),
              __func__);
        });
  }

 private:
  virtual ~Session() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mShutdownPromise);
    LOG(LogLevel::Debug, ("Session.~Session (%p)", this));
  }

  // Pull encoded media data from MediaEncoder and put into MutableBlobStorage.
  // If the bool aForceFlush is true, we will force a dispatch of a blob to
  // main thread.
  void Extract(bool aForceFlush) {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    LOG(LogLevel::Debug, ("Session.Extract %p", this));

    AUTO_PROFILER_LABEL("MediaRecorder::Session::Extract", OTHER);

    // Pull encoded media data from MediaEncoder
    nsTArray<nsTArray<uint8_t>> encodedBuf;
    nsresult rv = mEncoder->GetEncodedData(&encodedBuf);
    if (NS_FAILED(rv)) {
      MOZ_RELEASE_ASSERT(encodedBuf.IsEmpty());
      // Even if we failed to encode more data, it might be time to push a blob
      // with already encoded data.
    }

    // Append pulled data into cache buffer.
    NS_DispatchToMainThread(
        new StoreEncodedBufferRunnable(this, std::move(encodedBuf)));

    // Whether push encoded data back to onDataAvailable automatically or we
    // need a flush.
    bool pushBlob = aForceFlush;
    if (!pushBlob && mTimeSlice > 0 &&
        (TimeStamp::Now() - mLastBlobTimeStamp).ToMilliseconds() > mTimeSlice) {
      pushBlob = true;
    }
    if (pushBlob) {
      mLastBlobTimeStamp = TimeStamp::Now();
      InvokeAsync(mMainThread, this, __func__, &Session::GatherBlob)
          ->Then(mMainThread, __func__,
                 [this, self = RefPtr<Session>(this)](
                     const BlobPromise::ResolveOrRejectValue& aResult) {
                   if (aResult.IsReject()) {
                     LOG(LogLevel::Warning,
                         ("GatherBlob failed for pushing blob"));
                     DoSessionEndTask(aResult.RejectValue());
                     return;
                   }

                   nsresult rv = mRecorder->CreateAndDispatchBlobEvent(
                       aResult.ResolveValue());
                   if (NS_FAILED(rv)) {
                     DoSessionEndTask(NS_OK);
                   }
                 });
    }
  }

  void MediaStreamReady() {
    if (!mMediaStream) {
      // Already shut down. This can happen because MediaStreamReady is async.
      return;
    }

    if (mMediaStreamReady) {
      return;
    }

    if (!mRunningState.isOk() ||
        mRunningState.unwrap() != RunningState::Idling) {
      return;
    }

    nsTArray<RefPtr<mozilla::dom::MediaStreamTrack>> tracks;
    mMediaStream->GetTracks(tracks);
    uint8_t trackTypes = 0;
    int32_t audioTracks = 0;
    int32_t videoTracks = 0;
    for (auto& track : tracks) {
      if (track->Ended()) {
        continue;
      }

      ConnectMediaStreamTrack(*track);

      if (track->AsAudioStreamTrack()) {
        ++audioTracks;
        trackTypes |= ContainerWriter::CREATE_AUDIO_TRACK;
      } else if (track->AsVideoStreamTrack()) {
        ++videoTracks;
        trackTypes |= ContainerWriter::CREATE_VIDEO_TRACK;
      } else {
        MOZ_CRASH("Unexpected track type");
      }
    }

    if (trackTypes == 0) {
      MOZ_ASSERT(audioTracks == 0);
      MOZ_ASSERT(videoTracks == 0);
      return;
    }

    mMediaStreamReady = true;

    if (audioTracks > 1 || videoTracks > 1) {
      // When MediaRecorder supports multiple tracks, we should set up a single
      // MediaInputPort from the input stream, and let main thread check
      // track principals async later.
      nsPIDOMWindowInner* window = mRecorder->GetOwner();
      Document* document = window ? window->GetExtantDoc() : nullptr;
      nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                      NS_LITERAL_CSTRING("Media"), document,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "MediaRecorderMultiTracksNotSupported");
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    // Check that we may access the tracks' content.
    if (!MediaStreamTracksPrincipalSubsumes()) {
      LOG(LogLevel::Warning, ("Session.MediaTracksReady MediaStreamTracks "
                              "principal check failed"));
      DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    LOG(LogLevel::Debug,
        ("Session.MediaTracksReady track type = (%d)", trackTypes));
    InitEncoder(trackTypes, tracks[0]->Graph()->GraphRate());
  }

  void ConnectMediaStreamTrack(MediaStreamTrack& aTrack) {
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

  bool PrincipalSubsumes(nsIPrincipal* aPrincipal) {
    if (!mRecorder->GetOwner()) return false;
    nsCOMPtr<Document> doc = mRecorder->GetOwner()->GetExtantDoc();
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

  bool MediaStreamTracksPrincipalSubsumes() {
    MOZ_ASSERT(mRecorder->mDOMStream);
    nsCOMPtr<nsIPrincipal> principal = nullptr;
    for (RefPtr<MediaStreamTrack>& track : mMediaStreamTracks) {
      nsContentUtils::CombineResourcePrincipals(&principal,
                                                track->GetPrincipal());
    }
    return PrincipalSubsumes(principal);
  }

  bool AudioNodePrincipalSubsumes() {
    MOZ_ASSERT(mRecorder->mAudioNode);
    Document* doc = mRecorder->mAudioNode->GetOwner()
                        ? mRecorder->mAudioNode->GetOwner()->GetExtantDoc()
                        : nullptr;
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
    return PrincipalSubsumes(principal);
  }

  void InitEncoder(uint8_t aTrackTypes, TrackRate aTrackRate) {
    LOG(LogLevel::Debug, ("Session.InitEncoder %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mRunningState.isOk() ||
        mRunningState.unwrap() != RunningState::Idling) {
      MOZ_ASSERT_UNREACHABLE("Double-init");
      return;
    }

    // Create a TaskQueue to read encode media data from MediaEncoder.
    MOZ_RELEASE_ASSERT(!mEncoderThread);
    RefPtr<SharedThreadPool> pool =
        GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER);
    if (!pool) {
      LOG(LogLevel::Debug, ("Session.InitEncoder %p Failed to create "
                            "MediaRecorderReadThread thread pool",
                            this));
      DoSessionEndTask(NS_ERROR_FAILURE);
      return;
    }

    mEncoderThread =
        MakeAndAddRef<TaskQueue>(pool.forget(), "MediaRecorderReadThread");

    if (!gMediaRecorderShutdownBlocker) {
      // Add a shutdown blocker so mEncoderThread can be shutdown async.
      class Blocker : public ShutdownBlocker {
       public:
        Blocker()
            : ShutdownBlocker(
                  NS_LITERAL_STRING("MediaRecorder::Session: shutdown")) {}

        NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override {
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
          ShutdownPromise::All(GetCurrentThreadSerialEventTarget(), promises)
              ->Then(
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
      nsresult rv = barrier->AddBlocker(
          gMediaRecorderShutdownBlocker, NS_LITERAL_STRING(__FILE__), __LINE__,
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
        LOG(LogLevel::Info, ("Session.InitEncoder Bitrates higher than total "
                             "cap. Recalculating."));
        double factor =
            bitrate / static_cast<double>(audioBitrate + videoBitrate);
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
    // At this stage, the API doesn't allow UA to choose the output mimeType
    // format.

    mEncoder =
        MediaEncoder::CreateEncoder(mEncoderThread, mMimeType, audioBitrate,
                                    videoBitrate, aTrackTypes, aTrackRate);

    if (!mEncoder) {
      LOG(LogLevel::Error, ("Session.InitEncoder !mEncoder %p", this));
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    mEncoderListener = MakeAndAddRef<EncoderListener>(mEncoderThread, this);
    nsresult rv =
        mEncoderThread->Dispatch(NewRunnableMethod<RefPtr<EncoderListener>>(
            "mozilla::MediaEncoder::RegisterListener", mEncoder,
            &MediaEncoder::RegisterListener, mEncoderListener));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;

    if (mRecorder->mAudioNode) {
      mEncoder->ConnectAudioNode(mRecorder->mAudioNode,
                                 mRecorder->mAudioNodeOutput);
    }

    for (auto& track : mMediaStreamTracks) {
      mEncoder->ConnectMediaStreamTrack(track);
    }

    // If user defines timeslice interval for video blobs we have to set
    // appropriate video keyframe interval defined in milliseconds.
    mEncoder->SetVideoKeyFrameInterval(mTimeSlice);

    // Set mRunningState to Running so that DoSessionEndTask will
    // take the responsibility to end the session.
    mRunningState = RunningState::Starting;
  }

  // This is the task that will stop recording per spec:
  // - Stop gathering data (this is inherently async)
  // - Set state to "inactive"
  // - Fire an error event, if NS_FAILED(rv)
  // - Discard blob data if rv is NS_ERROR_DOM_SECURITY_ERR
  // - Fire a Blob event
  // - Fire an event named stop
  void DoSessionEndTask(nsresult rv) {
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

    bool needsStartEvent = false;
    if (mRunningState.isOk() &&
        (mRunningState.unwrap() == RunningState::Idling ||
         mRunningState.unwrap() == RunningState::Starting)) {
      needsStartEvent = true;
    }

    if (rv == NS_OK) {
      mRunningState = RunningState::Stopped;
    } else {
      mRunningState = Err(rv);
    }

    GatherBlob()
        ->Then(mMainThread, __func__,
               [this, self = RefPtr<Session>(this), rv, needsStartEvent](
                   const BlobPromise::ResolveOrRejectValue& aResult) {
                 if (mRecorder->mSessions.LastElement() == this) {
                   // Set state to inactive, but only if the recorder is not
                   // controlled by another session already.
                   mRecorder->ForceInactive();
                 }

                 if (needsStartEvent) {
                   mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("start"));
                 }

                 // If there was an error, Fire the appropriate one
                 if (NS_FAILED(rv)) {
                   mRecorder->NotifyError(rv);
                 }

                 // Fire a blob event named dataavailable
                 RefPtr<Blob> blob;
                 if (rv == NS_ERROR_DOM_SECURITY_ERR || aResult.IsReject()) {
                   // In case of SecurityError, the blob data must be discarded.
                   // We create a new empty one and throw the blob with its data
                   // away.
                   // In case we failed to gather blob data, we create an empty
                   // memory blob instead.
                   blob = Blob::CreateEmptyBlob(mRecorder->GetParentObject(),
                                                mMimeType);
                 } else {
                   blob = aResult.ResolveValue();
                 }
                 if (NS_FAILED(mRecorder->CreateAndDispatchBlobEvent(blob))) {
                   // Failed to dispatch blob event. That's unexpected. It's
                   // probably all right to fire an error event if we haven't
                   // already.
                   if (NS_SUCCEEDED(rv)) {
                     mRecorder->NotifyError(NS_ERROR_FAILURE);
                   }
                 }

                 // Dispatch stop event and clear MIME type.
                 mMimeType = NS_LITERAL_STRING("");
                 mRecorder->SetMimeType(mMimeType);

                 // Fire an event named stop
                 mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));

                 // And finally, Shutdown and destroy the Session
                 return Shutdown();
               })
        ->Then(mMainThread, __func__, [this, self = RefPtr<Session>(this)] {
          gSessions.RemoveEntry(this);
          if (gSessions.Count() == 0 && gMediaRecorderShutdownBlocker) {
            // All sessions finished before shutdown, no need to keep the
            // blocker.
            RefPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
            barrier->RemoveBlocker(gMediaRecorderShutdownBlocker);
            gMediaRecorderShutdownBlocker = nullptr;
          }
        });
  }

  void MediaEncoderInitialized() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    NS_DispatchToMainThread(NewRunnableFrom([self = RefPtr<Session>(this), this,
                                             mime = mEncoder->MimeType()]() {
      if (mRunningState.isErr()) {
        return NS_OK;
      }
      mMimeType = mime;
      mRecorder->SetMimeType(mime);
      auto state = mRunningState.unwrap();
      if (state == RunningState::Starting || state == RunningState::Stopping) {
        if (state == RunningState::Starting) {
          // We set it to Running in the runnable since we can only assign
          // mRunningState on main thread. We set it before running the start
          // event runnable since that dispatches synchronously (and may cause
          // js calls to methods depending on mRunningState).
          mRunningState = RunningState::Running;
        }
        mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("start"));
      }
      return NS_OK;
    }));

    Extract(false);
  }

  void MediaEncoderDataAvailable() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    Extract(false);
  }

  void MediaEncoderError() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    NS_DispatchToMainThread(NewRunnableMethod<nsresult>(
        "dom::MediaRecorder::Session::DoSessionEndTask", this,
        &Session::DoSessionEndTask, NS_ERROR_FAILURE));
  }

  void MediaEncoderShutdown() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    MOZ_ASSERT(mEncoder->IsShutdown());

    mMainThread->Dispatch(NewRunnableMethod<nsresult>(
        "MediaRecorder::Session::MediaEncoderShutdown->DoSessionEndTask", this,
        &Session::DoSessionEndTask, NS_OK));

    // Clean up.
    mEncoderListener->Forget();
    DebugOnly<bool> unregistered =
        mEncoder->UnregisterListener(mEncoderListener);
    MOZ_ASSERT(unregistered);
  }

  RefPtr<ShutdownPromise> Shutdown() {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(LogLevel::Debug, ("Session Shutdown %p", this));

    if (mShutdownPromise) {
      return mShutdownPromise;
    }

    // This is a coarse calculation and does not reflect the duration of the
    // final recording for reasons such as pauses. However it allows us an
    // idea of how long people are running their recorders for.
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    Telemetry::Accumulate(Telemetry::MEDIA_RECORDER_RECORDING_DURATION,
                          timeDelta.ToSeconds());

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

    // Remove main thread state. This could be needed if Stop() wasn't called.
    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    {
      auto tracks(std::move(mMediaStreamTracks));
      for (RefPtr<MediaStreamTrack>& track : tracks) {
        track->RemovePrincipalChangeObserver(this);
      }
    }

    // Break the cycle reference between Session and MediaRecorder.
    mShutdownPromise = mShutdownPromise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [self]() {
          self->mRecorder->RemoveSession(self);
          return ShutdownPromise::CreateAndResolve(true, __func__);
        },
        []() {
          MOZ_ASSERT_UNREACHABLE("Unexpected reject");
          return ShutdownPromise::CreateAndReject(false, __func__);
        });

    if (mEncoderThread) {
      RefPtr<TaskQueue>& encoderThread = mEncoderThread;
      mShutdownPromise = mShutdownPromise->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [encoderThread]() { return encoderThread->BeginShutdown(); },
          []() {
            MOZ_ASSERT_UNREACHABLE("Unexpected reject");
            return ShutdownPromise::CreateAndReject(false, __func__);
          });
    }

    return mShutdownPromise;
  }

 private:
  enum class RunningState {
    Idling,    // Session has been created
    Starting,  // MediaEncoder started, waiting for data
    Running,   // MediaEncoder has produced data
    Stopping,  // Stop() has been called
    Stopped,   // Session has stopped without any error
  };

  // Our associated MediaRecorder.
  const RefPtr<MediaRecorder> mRecorder;

  // Stream currently recorded.
  RefPtr<DOMMediaStream> mMediaStream;

  // True after we have decided on the track set to use for the recording.
  bool mMediaStreamReady;

  // Tracks currently recorded. This should be a subset of mMediaStream's track
  // set.
  nsTArray<RefPtr<MediaStreamTrack>> mMediaStreamTracks;

  // Main thread used for MozPromise operations.
  const RefPtr<nsISerialEventTarget> mMainThread;
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
  // onDataAvailable handler.
  const uint32_t mTimeSlice;
  // The time this session started, for telemetry.
  const TimeStamp mStartTime;
  // The session's current main thread state. The error type gets set when
  // ending a recording with an error. An NS_OK error is invalid.
  // Main thread only.
  Result<RunningState, nsresult> mRunningState;
};

MediaRecorder::~MediaRecorder() {
  LOG(LogLevel::Debug, ("~MediaRecorder (%p)", this));
  UnRegisterActivityObserver();
}

MediaRecorder::MediaRecorder(DOMMediaStream& aSourceMediaStream,
                             nsPIDOMWindowInner* aOwnerWindow)
    : DOMEventTargetHelper(aOwnerWindow),
      mAudioNodeOutput(0),
      mState(RecordingState::Inactive),
      mAudioBitsPerSecond(0),
      mVideoBitsPerSecond(0),
      mBitsPerSecond(0) {
  MOZ_ASSERT(aOwnerWindow);
  mDOMStream = &aSourceMediaStream;

  RegisterActivityObserver();
}

MediaRecorder::MediaRecorder(AudioNode& aSrcAudioNode, uint32_t aSrcOutput,
                             nsPIDOMWindowInner* aOwnerWindow)
    : DOMEventTargetHelper(aOwnerWindow),
      mAudioNodeOutput(aSrcOutput),
      mState(RecordingState::Inactive),
      mAudioBitsPerSecond(0),
      mVideoBitsPerSecond(0),
      mBitsPerSecond(0) {
  MOZ_ASSERT(aOwnerWindow);

  mAudioNode = &aSrcAudioNode;

  RegisterActivityObserver();
}

void MediaRecorder::RegisterActivityObserver() {
  if (nsPIDOMWindowInner* window = GetOwner()) {
    mDocument = window->GetExtantDoc();
    if (mDocument) {
      mDocument->RegisterActivityObserver(
          NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void MediaRecorder::UnRegisterActivityObserver() {
  if (mDocument) {
    mDocument->UnregisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
  }
}

void MediaRecorder::SetMimeType(const nsString& aMimeType) {
  mMimeType = aMimeType;
}

void MediaRecorder::GetMimeType(nsString& aMimeType) { aMimeType = mMimeType; }

void MediaRecorder::Start(const Optional<uint32_t>& aTimeSlice,
                          ErrorResult& aResult) {
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
    RefPtr<nsIPrincipal> streamPrincipal = mDOMStream->GetPrincipal();
    bool subsumes = false;
    nsPIDOMWindowInner* window;
    Document* doc;
    if (!(window = GetOwner()) || !(doc = window->GetExtantDoc()) ||
        NS_FAILED(doc->NodePrincipal()->Subsumes(streamPrincipal, &subsumes)) ||
        !subsumes) {
      aResult.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  uint32_t timeSlice = aTimeSlice.WasPassed() ? aTimeSlice.Value() : 0;
  MediaRecorderReporter::AddMediaRecorder(this);
  mState = RecordingState::Recording;
  // Start a session.
  mSessions.AppendElement();
  mSessions.LastElement() = new Session(this, timeSlice);
  mSessions.LastElement()->Start();
}

void MediaRecorder::Stop(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Stop %p", this));
  MediaRecorderReporter::RemoveMediaRecorder(this);
  if (mState == RecordingState::Inactive) {
    return;
  }
  mState = RecordingState::Inactive;
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->Stop();
}

void MediaRecorder::Pause(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Pause %p", this));
  if (mState == RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mState == RecordingState::Paused) {
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

void MediaRecorder::Resume(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Resume %p", this));
  if (mState == RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mState == RecordingState::Recording) {
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

void MediaRecorder::RequestData(ErrorResult& aResult) {
  if (mState == RecordingState::Inactive) {
    aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->RequestData();
}

JSObject* MediaRecorder::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return MediaRecorder_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<MediaRecorder> MediaRecorder::Constructor(
    const GlobalObject& aGlobal, DOMMediaStream& aStream,
    const MediaRecorderOptions& aInitDict, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
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

/* static */
already_AddRefed<MediaRecorder> MediaRecorder::Constructor(
    const GlobalObject& aGlobal, AudioNode& aSrcAudioNode, uint32_t aSrcOutput,
    const MediaRecorderOptions& aInitDict, ErrorResult& aRv) {
  // Allow recording from audio node only when pref is on.
  if (!Preferences::GetBool("media.recorder.audio_node.enabled", false)) {
    // Pretending that this constructor is not defined.
    NS_NAMED_LITERAL_STRING(argStr, "Argument 1 of MediaRecorder.constructor");
    NS_NAMED_LITERAL_STRING(typeStr, "MediaStream");
    aRv.ThrowTypeError<MSG_DOES_NOT_IMPLEMENT_INTERFACE>(argStr, typeStr);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
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

  RefPtr<MediaRecorder> object =
      new MediaRecorder(aSrcAudioNode, aSrcOutput, ownerWindow);
  object->SetOptions(aInitDict);
  return object.forget();
}

void MediaRecorder::SetOptions(const MediaRecorderOptions& aInitDict) {
  SetMimeType(aInitDict.mMimeType);
  mAudioBitsPerSecond = aInitDict.mAudioBitsPerSecond.WasPassed()
                            ? aInitDict.mAudioBitsPerSecond.Value()
                            : 0;
  mVideoBitsPerSecond = aInitDict.mVideoBitsPerSecond.WasPassed()
                            ? aInitDict.mVideoBitsPerSecond.Value()
                            : 0;
  mBitsPerSecond = aInitDict.mBitsPerSecond.WasPassed()
                       ? aInitDict.mBitsPerSecond.Value()
                       : 0;
  // We're not handling dynamic changes yet. Eventually we'll handle
  // setting audio, video and/or total -- and anything that isn't set,
  // we'll derive. Calculated versions require querying bitrates after
  // the encoder is Init()ed. This happens only after data is
  // available and thus requires dynamic changes.
  //
  // Until dynamic changes are supported, I prefer to be safe and err
  // slightly high
  if (aInitDict.mBitsPerSecond.WasPassed() &&
      !aInitDict.mVideoBitsPerSecond.WasPassed()) {
    mVideoBitsPerSecond = mBitsPerSecond;
  }
}

static char const* const gWebMVideoEncoderCodecs[4] = {
    "opus",
    "vp8",
    "vp8.0",
    // no VP9 yet
    nullptr,
};
static char const* const gWebMAudioEncoderCodecs[4] = {
    "opus",
    nullptr,
};
static char const* const gOggAudioEncoderCodecs[2] = {
    "opus",
    // we could support vorbis here too, but don't
    nullptr,
};

template <class String>
static bool CodecListContains(char const* const* aCodecs,
                              const String& aCodec) {
  for (int32_t i = 0; aCodecs[i]; ++i) {
    if (aCodec.EqualsASCII(aCodecs[i])) return true;
  }
  return false;
}

/* static */
bool MediaRecorder::IsTypeSupported(GlobalObject& aGlobal,
                                    const nsAString& aMIMEType) {
  return IsTypeSupported(aMIMEType);
}

/* static */
bool MediaRecorder::IsTypeSupported(const nsAString& aMIMEType) {
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
  else if ((mimeType.EqualsLiteral(VIDEO_WEBM) ||
            mimeType.EqualsLiteral(AUDIO_WEBM)) &&
           MediaEncoder::IsWebMEncoderEnabled()) {
    if (mimeType.EqualsLiteral(AUDIO_WEBM)) {
      codeclist = gWebMAudioEncoderCodecs;
    } else {
      codeclist = gWebMVideoEncoderCodecs;
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

nsresult MediaRecorder::CreateAndDispatchBlobEvent(Blob* aBlob) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aBlob;

  RefPtr<BlobEvent> event =
      BlobEvent::Constructor(this, NS_LITERAL_STRING("dataavailable"), init);
  event->SetTrusted(true);
  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

void MediaRecorder::DispatchSimpleEvent(const nsAString& aStr) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  rv = DOMEventTargetHelper::DispatchTrustedEvent(aStr);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error,
        ("MediaRecorder.DispatchSimpleEvent: DispatchTrustedEvent failed  %p",
         this));
    NS_ERROR("Failed to dispatch the event!!!");
  }
}

void MediaRecorder::NotifyError(nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckCurrentGlobalCorrectness();
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
                            "mUnknownDomException being fired for aRv: %X",
                            uint32_t(aRv)));
      init.mError = mUnknownDomException.forget();
  }

  RefPtr<MediaRecorderErrorEvent> event = MediaRecorderErrorEvent::Constructor(
      this, NS_LITERAL_STRING("error"), init);
  event->SetTrusted(true);

  IgnoredErrorResult res;
  DispatchEvent(*event, res);
  if (res.Failed()) {
    NS_ERROR("Failed to dispatch the error event!!!");
  }
}

void MediaRecorder::RemoveSession(Session* aSession) {
  LOG(LogLevel::Debug, ("MediaRecorder.RemoveSession (%p)", aSession));
  mSessions.RemoveElement(aSession);
}

void MediaRecorder::NotifyOwnerDocumentActivityChanged() {
  nsPIDOMWindowInner* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  Document* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  bool inFrameSwap = false;
  if (nsDocShell* docShell = static_cast<nsDocShell*>(doc->GetDocShell())) {
    inFrameSwap = docShell->InFrameSwap();
  }

  LOG(LogLevel::Debug, ("MediaRecorder %p NotifyOwnerDocumentActivityChanged "
                        "IsActive=%d, "
                        "IsVisible=%d, "
                        "InFrameSwap=%d",
                        this, doc->IsActive(), doc->IsVisible(), inFrameSwap));
  if (!doc->IsActive() || !(inFrameSwap || doc->IsVisible())) {
    // Stop the session.
    ErrorResult result;
    Stop(result);
    result.SuppressException();
  }
}

void MediaRecorder::ForceInactive() {
  LOG(LogLevel::Debug, ("MediaRecorder.ForceInactive %p", this));
  mState = RecordingState::Inactive;
}

void MediaRecorder::InitializeDomExceptions() {
  mSecurityDomException = DOMException::Create(NS_ERROR_DOM_SECURITY_ERR);
  mUnknownDomException = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR);
}

RefPtr<MediaRecorder::SizeOfPromise> MediaRecorder::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  MOZ_ASSERT(NS_IsMainThread());

  // The return type of a chained MozPromise cannot be changed, so we create a
  // holder for our desired return type and resolve that from All()->Then().
  auto holder = MakeRefPtr<Refcountable<MozPromiseHolder<SizeOfPromise>>>();
  RefPtr<SizeOfPromise> promise = holder->Ensure(__func__);

  nsTArray<RefPtr<SizeOfPromise>> promises(mSessions.Length());
  for (const RefPtr<Session>& session : mSessions) {
    promises.AppendElement(session->SizeOfExcludingThis(aMallocSizeOf));
  }

  SizeOfPromise::All(GetCurrentThreadSerialEventTarget(), promises)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [holder](const nsTArray<size_t>& sizes) {
            size_t total = 0;
            for (const size_t& size : sizes) {
              total += size;
            }
            holder->Resolve(total, __func__);
          },
          []() { MOZ_CRASH("Unexpected reject"); });

  return promise;
}

StaticRefPtr<MediaRecorderReporter> MediaRecorderReporter::sUniqueInstance;

}  // namespace dom
}  // namespace mozilla

#undef LOG
