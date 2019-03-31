/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "ImageContainer.h"
#include "ImageTypes.h"
#include "Layers.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/File.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIFile.h"
#include "nsIFilePicker.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "SineWaveGenerator.h"
#include "Tracing.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "nsISupportsUtils.h"
#endif

#ifdef MOZ_WEBRTC
#  include "YuvStamper.h"
#endif

#define DEFAULT_AUDIO_TIMER_MS 10
namespace mozilla {

using namespace mozilla::gfx;

static nsString DefaultVideoName() {
  // For the purpose of testing we allow to change the name of the fake device
  // by pref.
  nsAutoString cameraNameFromPref;
  nsresult rv;
  // Here it is preferred a "hard" block, provided by the combination of Await &
  // InvokeAsync, instead of "soft" block, provided by sync dispatch which
  // allows the waiting thread to spin its event loop. The latter would allow
  // miltiple enumeration requests being processed out-of-order.
  media::Await(
      do_AddRef(SystemGroup::EventTargetFor(TaskCategory::Other)),
      InvokeAsync(
          SystemGroup::EventTargetFor(TaskCategory::Other), __func__, [&]() {
            rv = Preferences::GetString("media.getusermedia.fake-camera-name",
                                        cameraNameFromPref);
            return GenericPromise::CreateAndResolve(true, __func__);
          }));

  if (NS_SUCCEEDED(rv)) {
    return std::move(cameraNameFromPref);
  }
  return NS_LITERAL_STRING(u"Default Video Device");
}

/**
 * Default video source.
 */

MediaEngineDefaultVideoSource::MediaEngineDefaultVideoSource()
    : mTimer(nullptr), mName(DefaultVideoName()) {}

MediaEngineDefaultVideoSource::~MediaEngineDefaultVideoSource() = default;

nsString MediaEngineDefaultVideoSource::GetName() const { return mName; }

nsCString MediaEngineDefaultVideoSource::GetUUID() const {
  return NS_LITERAL_CSTRING("1041FCBD-3F12-4F7B-9E9B-1EC556DD5676");
}

nsString MediaEngineDefaultVideoSource::GetGroupId() const {
  return NS_LITERAL_STRING(u"Default Video Group");
}

uint32_t MediaEngineDefaultVideoSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const {
  AssertIsOnOwningThread();

  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance =
        MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break;  // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult MediaEngineDefaultVideoSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kReleased);

  FlattenedConstraints c(aConstraints);

  // Mock failure for automated tests.
  if (c.mDeviceId.mIdeal.find(NS_LITERAL_STRING("bad device")) !=
      c.mDeviceId.mIdeal.end()) {
    return NS_ERROR_FAILURE;
  }

  // emulator debug is very, very slow; reduce load on it with smaller/slower
  // fake video
  mOpts = aPrefs;
  mOpts.mWidth =
      c.mWidth.Get(aPrefs.mWidth ? aPrefs.mWidth :
#ifdef DEBUG
                                 MediaEnginePrefs::DEFAULT_43_VIDEO_WIDTH / 2
#else
                                 MediaEnginePrefs::DEFAULT_43_VIDEO_WIDTH
#endif
      );
  mOpts.mHeight =
      c.mHeight.Get(aPrefs.mHeight ? aPrefs.mHeight :
#ifdef DEBUG
                                   MediaEnginePrefs::DEFAULT_43_VIDEO_HEIGHT / 2
#else
                                   MediaEnginePrefs::DEFAULT_43_VIDEO_HEIGHT
#endif
      );
  mOpts.mWidth = std::max(160, std::min(mOpts.mWidth, 4096)) & ~1;
  mOpts.mHeight = std::max(90, std::min(mOpts.mHeight, 2160)) & ~1;

  mState = kAllocated;
  return NS_OK;
}

nsresult MediaEngineDefaultVideoSource::Deallocate() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(!mImage);
  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  if (mStream && IsTrackIDExplicit(mTrackID)) {
    mStream->EndTrack(mTrackID);
    mStream = nullptr;
    mTrackID = TRACK_NONE;
    mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  }
  mState = kReleased;
  mImageContainer = nullptr;

  return NS_OK;
}

static void AllocateSolidColorFrame(layers::PlanarYCbCrData& aData, int aWidth,
                                    int aHeight, int aY, int aCb, int aCr) {
  MOZ_ASSERT(!(aWidth & 1));
  MOZ_ASSERT(!(aHeight & 1));
  // Allocate a single frame with a solid color
  int yLen = aWidth * aHeight;
  int cbLen = yLen >> 2;
  int crLen = cbLen;
  uint8_t* frame = (uint8_t*)malloc(yLen + cbLen + crLen);
  memset(frame, aY, yLen);
  memset(frame + yLen, aCb, cbLen);
  memset(frame + yLen + cbLen, aCr, crLen);

  aData.mYChannel = frame;
  aData.mYSize = IntSize(aWidth, aHeight);
  aData.mYStride = aWidth;
  aData.mCbCrStride = aWidth >> 1;
  aData.mCbChannel = frame + yLen;
  aData.mCrChannel = aData.mCbChannel + cbLen;
  aData.mCbCrSize = IntSize(aWidth >> 1, aHeight >> 1);
  aData.mPicX = 0;
  aData.mPicY = 0;
  aData.mPicSize = IntSize(aWidth, aHeight);
  aData.mStereoMode = StereoMode::MONO;
}

static void ReleaseFrame(layers::PlanarYCbCrData& aData) {
  free(aData.mYChannel);
}

void MediaEngineDefaultVideoSource::SetTrack(
    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
    const PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated);
  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);

  mStream = aStream;
  mTrackID = aTrackID;
  mPrincipalHandle = aPrincipal;
  aStream->AddTrack(aTrackID, new VideoSegment(),
                    SourceMediaStream::ADDTRACK_QUEUED);
}

nsresult MediaEngineDefaultVideoSource::Start() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  MOZ_ASSERT(mStream, "SetTrack() must happen before Start()");
  MOZ_ASSERT(IsTrackIDExplicit(mTrackID),
             "SetTrack() must happen before Start()");

  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  if (!mImageContainer) {
    mImageContainer = layers::LayerManager::CreateImageContainer(
        layers::ImageContainer::ASYNCHRONOUS);
  }

  // Start timer for subsequent frames
  uint32_t interval;
#if defined(MOZ_WIDGET_ANDROID) && defined(DEBUG)
  // emulator debug is very, very slow and has problems dealing with realtime
  // audio inputs
  interval = 10 * (1000 / mOpts.mFPS);
#else
  interval = 1000 / mOpts.mFPS;
#endif
  mTimer->InitWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) {
        RefPtr<MediaEngineDefaultVideoSource> source =
            static_cast<MediaEngineDefaultVideoSource*>(aClosure);
        source->GenerateFrame();
      },
      this, interval, nsITimer::TYPE_REPEATING_SLACK,
      "MediaEngineDefaultVideoSource::GenerateFrame");

  mState = kStarted;
  return NS_OK;
}

nsresult MediaEngineDefaultVideoSource::Stop() {
  AssertIsOnOwningThread();

  if (mState == kStopped || mState == kAllocated) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kStarted);
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(mStream);
  MOZ_ASSERT(IsTrackIDExplicit(mTrackID));

  mTimer->Cancel();
  mTimer = nullptr;

  mState = kStopped;

  return NS_OK;
}

nsresult MediaEngineDefaultVideoSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const char** aOutBadConstraint) {
  return NS_OK;
}

void MediaEngineDefaultVideoSource::GenerateFrame() {
  AssertIsOnOwningThread();

  // Update the target color
  if (mCr <= 16) {
    if (mCb < 240) {
      mCb++;
    } else {
      mCr++;
    }
  } else if (mCb >= 240) {
    if (mCr < 240) {
      mCr++;
    } else {
      mCb--;
    }
  } else if (mCr >= 240) {
    if (mCb > 16) {
      mCb--;
    } else {
      mCr--;
    }
  } else {
    mCr--;
  }

  // Allocate a single solid color image
  RefPtr<layers::PlanarYCbCrImage> ycbcr_image =
      mImageContainer->CreatePlanarYCbCrImage();
  layers::PlanarYCbCrData data;
  AllocateSolidColorFrame(data, mOpts.mWidth, mOpts.mHeight, 0x80, mCb, mCr);

#ifdef MOZ_WEBRTC
  uint64_t timestamp = PR_Now();
  YuvStamper::Encode(mOpts.mWidth, mOpts.mHeight, mOpts.mWidth, data.mYChannel,
                     reinterpret_cast<unsigned char*>(&timestamp),
                     sizeof(timestamp), 0, 0);
#endif

  bool setData = ycbcr_image->CopyData(data);
  MOZ_ASSERT(setData);

  // SetData copies data, so we can free the frame
  ReleaseFrame(data);

  if (!setData) {
    return;
  }

  VideoSegment segment;
  segment.AppendFrame(ycbcr_image.forget(),
                      gfx::IntSize(mOpts.mWidth, mOpts.mHeight),
                      mPrincipalHandle);
  mStream->AppendToTrack(mTrackID, &segment);
}

// This class is created on the media thread, as part of Start(), then entirely
// self-sustained until destruction, just forwarding calls to Pull().
class AudioSourcePullListener : public MediaStreamTrackListener {
 public:
  AudioSourcePullListener(RefPtr<SourceMediaStream> aStream, TrackID aTrackID,
                          const PrincipalHandle& aPrincipalHandle,
                          uint32_t aFrequency)
      : mStream(std::move(aStream)),
        mTrackID(aTrackID),
        mPrincipalHandle(aPrincipalHandle),
        mSineGenerator(
            MakeUnique<SineWaveGenerator>(mStream->GraphRate(), aFrequency)) {
    MOZ_COUNT_CTOR(AudioSourcePullListener);
  }

  ~AudioSourcePullListener() { MOZ_COUNT_DTOR(AudioSourcePullListener); }

  void NotifyPull(MediaStreamGraph* aGraph, StreamTime aEndOfAppendedData,
                  StreamTime aDesiredTime) override;

  const RefPtr<SourceMediaStream> mStream;
  const TrackID mTrackID;
  const PrincipalHandle mPrincipalHandle;
  const UniquePtr<SineWaveGenerator> mSineGenerator;
};

/**
 * Default audio source.
 */

MediaEngineDefaultAudioSource::MediaEngineDefaultAudioSource() = default;

MediaEngineDefaultAudioSource::~MediaEngineDefaultAudioSource() = default;

nsString MediaEngineDefaultAudioSource::GetName() const {
  return NS_LITERAL_STRING(u"Default Audio Device");
}

nsCString MediaEngineDefaultAudioSource::GetUUID() const {
  return NS_LITERAL_CSTRING("B7CBD7C1-53EF-42F9-8353-73F61C70C092");
}

nsString MediaEngineDefaultAudioSource::GetGroupId() const {
  return NS_LITERAL_STRING(u"Default Audio Group");
}

uint32_t MediaEngineDefaultAudioSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const {
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance =
        MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break;  // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult MediaEngineDefaultAudioSource::Allocate(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    const char** aOutBadConstraint) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kReleased);

  // Mock failure for automated tests.
  if (aConstraints.mDeviceId.WasPassed() &&
      aConstraints.mDeviceId.Value().IsString() &&
      aConstraints.mDeviceId.Value().GetAsString().EqualsASCII("bad device")) {
    return NS_ERROR_FAILURE;
  }

  mFrequency = aPrefs.mFreq ? aPrefs.mFreq : 1000;

  mState = kAllocated;
  return NS_OK;
}

nsresult MediaEngineDefaultAudioSource::Deallocate() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  if (mStream && IsTrackIDExplicit(mTrackID)) {
    mStream->EndTrack(mTrackID);
    mStream = nullptr;
    mTrackID = TRACK_NONE;
    mPrincipalHandle = PRINCIPAL_HANDLE_NONE;
  }
  mState = kReleased;
  return NS_OK;
}

void MediaEngineDefaultAudioSource::SetTrack(
    const RefPtr<SourceMediaStream>& aStream, TrackID aTrackID,
    const PrincipalHandle& aPrincipal) {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated);
  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);

  // AddAudioTrack will take ownership of segment
  mStream = aStream;
  mTrackID = aTrackID;
  mPrincipalHandle = aPrincipal;
  aStream->AddAudioTrack(aTrackID, aStream->GraphRate(), new AudioSegment(),
                         SourceMediaStream::ADDTRACK_QUEUED);
}

nsresult MediaEngineDefaultAudioSource::Start() {
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  MOZ_ASSERT(mStream, "SetTrack() must happen before Start()");
  MOZ_ASSERT(IsTrackIDExplicit(mTrackID),
             "SetTrack() must happen before Start()");

  if (!mPullListener) {
    mPullListener = MakeAndAddRef<AudioSourcePullListener>(
        mStream, mTrackID, mPrincipalHandle, mFrequency);
  }

  mState = kStarted;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [stream = mStream, track = mTrackID, listener = mPullListener]() {
        if (stream->IsDestroyed()) {
          return;
        }
        stream->AddTrackListener(listener, track);
        stream->SetPullingEnabled(track, true);
      }));

  return NS_OK;
}

nsresult MediaEngineDefaultAudioSource::Stop() {
  AssertIsOnOwningThread();

  if (mState == kStopped || mState == kAllocated) {
    return NS_OK;
  }
  MOZ_ASSERT(mState == kStarted);
  mState = kStopped;

  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [stream = mStream, track = mTrackID,
                                        listener = std::move(mPullListener)]() {
        if (stream->IsDestroyed()) {
          return;
        }
        stream->RemoveTrackListener(listener, track);
        stream->SetPullingEnabled(track, false);
      }));
  return NS_OK;
}

nsresult MediaEngineDefaultAudioSource::Reconfigure(
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs& aPrefs, const nsString& aDeviceId,
    const char** aOutBadConstraint) {
  return NS_OK;
}

void AudioSourcePullListener::NotifyPull(MediaStreamGraph* aGraph,
                                         StreamTime aEndOfAppendedData,
                                         StreamTime aDesiredTime) {
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i", mStream.get(),
                               mTrackID);
  AudioSegment segment;
  TrackTicks delta = aDesiredTime - aEndOfAppendedData;
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(delta * sizeof(int16_t));
  int16_t* dest = static_cast<int16_t*>(buffer->Data());
  mSineGenerator->generate(dest, delta);
  AutoTArray<const int16_t*, 1> channels;
  channels.AppendElement(dest);
  segment.AppendFrames(buffer.forget(), channels, delta, mPrincipalHandle);
  mStream->AppendToTrack(mTrackID, &segment);
}

void MediaEngineDefault::EnumerateDevices(
    uint64_t aWindowId, dom::MediaSourceEnum aMediaSource,
    MediaSinkEnum aMediaSink, nsTArray<RefPtr<MediaDevice>>* aDevices) {
  AssertIsOnOwningThread();

  switch (aMediaSource) {
    case dom::MediaSourceEnum::Camera: {
      // Only supports camera video sources. See Bug 1038241.
      auto newSource = MakeRefPtr<MediaEngineDefaultVideoSource>();
      aDevices->AppendElement(MakeRefPtr<MediaDevice>(
          newSource, newSource->GetName(),
          NS_ConvertUTF8toUTF16(newSource->GetUUID()), newSource->GetGroupId(),
          NS_LITERAL_STRING("")));
      return;
    }
    case dom::MediaSourceEnum::Microphone: {
      auto newSource = MakeRefPtr<MediaEngineDefaultAudioSource>();
      aDevices->AppendElement(MakeRefPtr<MediaDevice>(
          newSource, newSource->GetName(),
          NS_ConvertUTF8toUTF16(newSource->GetUUID()), newSource->GetGroupId(),
          NS_LITERAL_STRING("")));
      return;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported source type");
      return;
  }

  if (aMediaSink == MediaSinkEnum::Speaker) {
    NS_WARNING("No default implementation for MediaSinkEnum::Speaker");
  }
}

}  // namespace mozilla
