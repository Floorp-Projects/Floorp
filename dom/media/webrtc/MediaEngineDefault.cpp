/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "ImageContainer.h"
#include "ImageTypes.h"
#include "Layers.h"
#include "MediaStreamGraph.h"
#include "MediaTrackConstraints.h"
#include "mozilla/dom/File.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIFile.h"
#include "nsIFilePicker.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "Tracing.h"

#ifdef MOZ_WIDGET_ANDROID
#include "nsISupportsUtils.h"
#endif

#ifdef MOZ_WEBRTC
#include "YuvStamper.h"
#endif

#define DEFAULT_AUDIO_TIMER_MS 10
namespace mozilla {

using namespace mozilla::gfx;

/**
 * Default video source.
 */

MediaEngineDefaultVideoSource::MediaEngineDefaultVideoSource()
  : mTimer(nullptr)
  , mMutex("MediaEngineDefaultVideoSource::mMutex")
{}

MediaEngineDefaultVideoSource::~MediaEngineDefaultVideoSource()
{}

nsString
MediaEngineDefaultVideoSource::GetName() const
{
  return NS_LITERAL_STRING(u"Default Video Device");
}

nsCString
MediaEngineDefaultVideoSource::GetUUID() const
{
  return NS_LITERAL_CSTRING("1041FCBD-3F12-4F7B-9E9B-1EC556DD5676");
}

uint32_t
MediaEngineDefaultVideoSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  AssertIsOnOwningThread();

  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance = MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break; // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult
MediaEngineDefaultVideoSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                        const MediaEnginePrefs &aPrefs,
                                        const nsString& aDeviceId,
                                        const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                        AllocationHandle** aOutHandle,
                                        const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kReleased);

  FlattenedConstraints c(aConstraints);

  // Mock failure for automated tests.
  if (c.mDeviceId.mIdeal.find(NS_LITERAL_STRING("bad device")) !=
      c.mDeviceId.mIdeal.end()) {
    return NS_ERROR_FAILURE;
  }

  // emulator debug is very, very slow; reduce load on it with smaller/slower fake video
  mOpts = aPrefs;
  mOpts.mWidth = c.mWidth.Get(aPrefs.mWidth ? aPrefs.mWidth :
#ifdef DEBUG
                              MediaEnginePrefs::DEFAULT_43_VIDEO_WIDTH/2
#else
                              MediaEnginePrefs::DEFAULT_43_VIDEO_WIDTH
#endif
                              );
  mOpts.mHeight = c.mHeight.Get(aPrefs.mHeight ? aPrefs.mHeight :
#ifdef DEBUG
                                MediaEnginePrefs::DEFAULT_43_VIDEO_HEIGHT/2
#else
                                MediaEnginePrefs::DEFAULT_43_VIDEO_HEIGHT
#endif
                                );
  mOpts.mWidth = std::max(160, std::min(mOpts.mWidth, 4096)) & ~1;
  mOpts.mHeight = std::max(90, std::min(mOpts.mHeight, 2160)) & ~1;
  *aOutHandle = nullptr;

  MutexAutoLock lock(mMutex);
  mState = kAllocated;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Deallocate(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(!aHandle);
  MOZ_ASSERT(!mImage);
  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  MutexAutoLock lock(mMutex);
  if (mStream && IsTrackIDExplicit(mTrackID)) {
    mStream->EndTrack(mTrackID);
    mStream = nullptr;
    mTrackID = TRACK_NONE;
  }
  mState = kReleased;
  mImageContainer = nullptr;

  return NS_OK;
}

static void AllocateSolidColorFrame(layers::PlanarYCbCrData& aData,
                                    int aWidth, int aHeight,
                                    int aY, int aCb, int aCr)
{
  MOZ_ASSERT(!(aWidth&1));
  MOZ_ASSERT(!(aHeight&1));
  // Allocate a single frame with a solid color
  int yLen = aWidth*aHeight;
  int cbLen = yLen>>2;
  int crLen = cbLen;
  uint8_t* frame = (uint8_t*) malloc(yLen+cbLen+crLen);
  memset(frame, aY, yLen);
  memset(frame+yLen, aCb, cbLen);
  memset(frame+yLen+cbLen, aCr, crLen);

  aData.mYChannel = frame;
  aData.mYSize = IntSize(aWidth, aHeight);
  aData.mYStride = aWidth;
  aData.mCbCrStride = aWidth>>1;
  aData.mCbChannel = frame + yLen;
  aData.mCrChannel = aData.mCbChannel + cbLen;
  aData.mCbCrSize = IntSize(aWidth>>1, aHeight>>1);
  aData.mPicX = 0;
  aData.mPicY = 0;
  aData.mPicSize = IntSize(aWidth, aHeight);
  aData.mStereoMode = StereoMode::MONO;
}

static void ReleaseFrame(layers::PlanarYCbCrData& aData)
{
  free(aData.mYChannel);
}

nsresult
MediaEngineDefaultVideoSource::SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                                        const RefPtr<SourceMediaStream>& aStream,
                                        TrackID aTrackID,
                                        const PrincipalHandle& aPrincipal)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated);
  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);

  {
    MutexAutoLock lock(mMutex);
    mStream = aStream;
    mTrackID = aTrackID;
  }
  aStream->AddTrack(aTrackID, 0, new VideoSegment(),
                    SourceMediaStream::ADDTRACK_QUEUED);
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Start(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  MOZ_ASSERT(mStream, "SetTrack() must happen before Start()");
  MOZ_ASSERT(IsTrackIDExplicit(mTrackID), "SetTrack() must happen before Start()");

  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  if (!mImageContainer) {
    mImageContainer =
      layers::LayerManager::CreateImageContainer(layers::ImageContainer::ASYNCHRONOUS);
  }

  // Start timer for subsequent frames
  uint32_t interval;
#if defined(MOZ_WIDGET_ANDROID) && defined(DEBUG)
// emulator debug is very, very slow and has problems dealing with realtime audio inputs
  interval = 10 * (1000 / mOpts.mFPS);
#else
  interval = 1000 / mOpts.mFPS;
#endif
  mTimer->InitWithNamedFuncCallback([](nsITimer* aTimer, void* aClosure) {
      RefPtr<MediaEngineDefaultVideoSource> source =
        static_cast<MediaEngineDefaultVideoSource*>(aClosure);
      source->GenerateFrame();
    }, this, interval, nsITimer::TYPE_REPEATING_SLACK,
    "MediaEngineDefaultVideoSource::GenerateFrame");

  MutexAutoLock lock(mMutex);
  mState = kStarted;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Stop(const RefPtr<const AllocationHandle>& aHandle)
{
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

  MutexAutoLock lock(mMutex);

  mImage = nullptr;
  mState = kStopped;

  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Reconfigure(
    const RefPtr<AllocationHandle>& aHandle,
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  return NS_OK;
}

void
MediaEngineDefaultVideoSource::GenerateFrame()
{
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
  RefPtr<layers::PlanarYCbCrImage> ycbcr_image = mImageContainer->CreatePlanarYCbCrImage();
  layers::PlanarYCbCrData data;
  AllocateSolidColorFrame(data, mOpts.mWidth, mOpts.mHeight, 0x80, mCb, mCr);

#ifdef MOZ_WEBRTC
  uint64_t timestamp = PR_Now();
  YuvStamper::Encode(mOpts.mWidth, mOpts.mHeight, mOpts.mWidth,
		     data.mYChannel,
		     reinterpret_cast<unsigned char*>(&timestamp), sizeof(timestamp),
		     0, 0);
#endif

  bool setData = ycbcr_image->CopyData(data);
  MOZ_ASSERT(setData);

  // SetData copies data, so we can free the frame
  ReleaseFrame(data);

  if (!setData) {
    return;
  }

  MutexAutoLock lock(mMutex);
  mImage = std::move(ycbcr_image);
}

void
MediaEngineDefaultVideoSource::Pull(const RefPtr<const AllocationHandle>& aHandle,
                                    const RefPtr<SourceMediaStream>& aStream,
                                    TrackID aTrackID,
                                    StreamTime aDesiredTime,
                                    const PrincipalHandle& aPrincipalHandle)
{
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i",
                               aStream.get(), aTrackID);
  // AppendFrame takes ownership of `segment`
  VideoSegment segment;

  RefPtr<layers::Image> image;
  {
    MutexAutoLock lock(mMutex);
    // Started - append real image
    // Stopped - append null
    // Released - Track is ended, safe to ignore
    //            Can happen because NotifyPull comes from a stream listener
    if (mState == kReleased) {
      return;
    }
    MOZ_ASSERT(mState != kAllocated);
    if (mState == kStarted) {
      MOZ_ASSERT(mStream == aStream);
      MOZ_ASSERT(mTrackID == aTrackID);
      image = mImage;
    }
  }

  StreamTime delta = aDesiredTime - aStream->GetEndOfAppendedData(aTrackID);
  if (delta > 0) {
    // nullptr images are allowed
    IntSize size(mOpts.mWidth, mOpts.mHeight);
    segment.AppendFrame(image.forget(), delta, size, aPrincipalHandle);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    aStream->AppendToTrack(aTrackID, &segment);
  }
}

/**
 * Default audio source.
 */

MediaEngineDefaultAudioSource::MediaEngineDefaultAudioSource()
  : mMutex("MediaEngineDefaultAudioSource::mMutex")
{}

MediaEngineDefaultAudioSource::~MediaEngineDefaultAudioSource()
{}

nsString
MediaEngineDefaultAudioSource::GetName() const
{
  return NS_LITERAL_STRING(u"Default Audio Device");
}

nsCString
MediaEngineDefaultAudioSource::GetUUID() const
{
  return NS_LITERAL_CSTRING("B7CBD7C1-53EF-42F9-8353-73F61C70C092");
}

uint32_t
MediaEngineDefaultAudioSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance = MediaConstraintsHelper::GetMinimumFitnessDistance(*cs, aDeviceId);
    break; // distance is read from first entry only
  }
#endif
  return distance;
}

bool
MediaEngineDefaultAudioSource::IsAvailable() const
{
  AssertIsOnOwningThread();

  return mState == kReleased;
}

nsresult
MediaEngineDefaultAudioSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                        const MediaEnginePrefs &aPrefs,
                                        const nsString& aDeviceId,
                                        const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                        AllocationHandle** aOutHandle,
                                        const char** aOutBadConstraint)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kReleased);

  // Mock failure for automated tests.
  if (aConstraints.mDeviceId.IsString() &&
      aConstraints.mDeviceId.GetAsString().EqualsASCII("bad device")) {
    return NS_ERROR_FAILURE;
  }

  mFreq = aPrefs.mFreq ? aPrefs.mFreq : 1000;
  *aOutHandle = nullptr;

  MutexAutoLock lock(mMutex);
  mState = kAllocated;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Deallocate(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(!aHandle);
  MOZ_ASSERT(mState == kStopped || mState == kAllocated);

  MutexAutoLock lock(mMutex);
  if (mStream && IsTrackIDExplicit(mTrackID)) {
    mStream->EndTrack(mTrackID);
    mStream = nullptr;
    mTrackID = TRACK_NONE;
  }
  mState = kReleased;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::SetTrack(const RefPtr<const AllocationHandle>& aHandle,
                                        const RefPtr<SourceMediaStream>& aStream,
                                        TrackID aTrackID,
                                        const PrincipalHandle& aPrincipal)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated);
  MOZ_ASSERT(!mStream);
  MOZ_ASSERT(mTrackID == TRACK_NONE);

  // AddAudioTrack will take ownership of segment
  mStream = aStream;
  mTrackID = aTrackID;
  aStream->AddAudioTrack(aTrackID,
                         aStream->GraphRate(),
                         0,
                         new AudioSegment(),
                         SourceMediaStream::ADDTRACK_QUEUED);
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Start(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(mState == kAllocated || mState == kStopped);
  MOZ_ASSERT(mStream, "SetTrack() must happen before Start()");
  MOZ_ASSERT(IsTrackIDExplicit(mTrackID), "SetTrack() must happen before Start()");

  if (!mSineGenerator) {
    // generate sine wave (default 1KHz)
    mSineGenerator = new SineWaveGenerator(mStream->GraphRate(), mFreq);
  }

  MutexAutoLock lock(mMutex);
  mState = kStarted;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Stop(const RefPtr<const AllocationHandle>& aHandle)
{
  AssertIsOnOwningThread();

  if (mState == kStopped || mState == kAllocated) {
    return NS_OK;
  }

  MOZ_ASSERT(mState == kStarted);

  MutexAutoLock lock(mMutex);
  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Reconfigure(
    const RefPtr<AllocationHandle>& aHandle,
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  return NS_OK;
}

void
MediaEngineDefaultAudioSource::AppendToSegment(AudioSegment& aSegment,
                                               TrackTicks aSamples,
                                               const PrincipalHandle& aPrincipalHandle)
{
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(aSamples * sizeof(int16_t));
  int16_t* dest = static_cast<int16_t*>(buffer->Data());

  mSineGenerator->generate(dest, aSamples);
  AutoTArray<const int16_t*,1> channels;
  channels.AppendElement(dest);
  aSegment.AppendFrames(buffer.forget(), channels, aSamples, aPrincipalHandle);
}

void
MediaEngineDefaultAudioSource::Pull(const RefPtr<const AllocationHandle>& aHandle,
                                    const RefPtr<SourceMediaStream>& aStream,
                                    TrackID aTrackID,
                                    StreamTime aDesiredTime,
                                    const PrincipalHandle& aPrincipalHandle)
{
  TRACE_AUDIO_CALLBACK_COMMENT("SourceMediaStream %p track %i",
                               aStream.get(), aTrackID);
  AudioSegment segment;
  // avoid accumulating rounding errors
  TrackTicks desired = aStream->TimeToTicksRoundUp(aStream->GraphRate(), aDesiredTime);
  TrackTicks delta = desired - mLastNotify;
  mLastNotify += delta;
  AppendToSegment(segment, delta, aPrincipalHandle);
  aStream->AppendToTrack(aTrackID, &segment);
}

void
MediaEngineDefault::EnumerateDevices(uint64_t aWindowId,
                                     dom::MediaSourceEnum aMediaSource,
                                     MediaSinkEnum aMediaSink,
                                     nsTArray<RefPtr<MediaDevice>>* aDevices)
{
  AssertIsOnOwningThread();

  switch (aMediaSource) {
    case dom::MediaSourceEnum::Camera: {
      // Only supports camera video sources. See Bug 1038241.

      // We once had code here to find a VideoSource with the same settings and
      // re-use that. This is no longer possible since the resolution gets set
      // in Allocate().

      nsTArray<RefPtr<MediaEngineSource>>*
        devicesForThisWindow = mVSources.LookupOrAdd(aWindowId);
      auto newSource = MakeRefPtr<MediaEngineDefaultVideoSource>();
      devicesForThisWindow->AppendElement(newSource);
      aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                                newSource,
                                newSource->GetName(),
                                NS_ConvertUTF8toUTF16(newSource->GetUUID())));
      return;
    }
    case dom::MediaSourceEnum::Microphone: {
      nsTArray<RefPtr<MediaEngineDefaultAudioSource>>*
        devicesForThisWindow = mASources.LookupOrAdd(aWindowId);
      for (const RefPtr<MediaEngineDefaultAudioSource>& source : *devicesForThisWindow) {
        if (source->IsAvailable()) {
          aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                                    source,
                                    source->GetName(),
                                    NS_ConvertUTF8toUTF16(source->GetUUID())));
        }
      }

      if (aDevices->IsEmpty()) {
        // All streams are currently busy, just make a new one.
        auto newSource = MakeRefPtr<MediaEngineDefaultAudioSource>();
        devicesForThisWindow->AppendElement(newSource);
        aDevices->AppendElement(MakeRefPtr<MediaDevice>(
                                  newSource,
                                  newSource->GetName(),
                                  NS_ConvertUTF8toUTF16(newSource->GetUUID())));
      }
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

void
MediaEngineDefault::ReleaseResourcesForWindow(uint64_t aWindowId)
{
  nsTArray<RefPtr<MediaEngineDefaultAudioSource>>* audioDevicesForThisWindow =
   mASources.Get(aWindowId);

  if (audioDevicesForThisWindow) {
    for (const RefPtr<MediaEngineDefaultAudioSource>& source :
         *audioDevicesForThisWindow) {
      source->Shutdown();
    }
  }

  mASources.Remove(aWindowId);

  nsTArray<RefPtr<MediaEngineSource>>* videoDevicesForThisWindow =
    mVSources.Get(aWindowId);

  if (videoDevicesForThisWindow) {
    for (const RefPtr<MediaEngineSource>& source :
         *videoDevicesForThisWindow) {
      source->Shutdown();
    }
  }

  mVSources.Remove(aWindowId);
}

void
MediaEngineDefault::Shutdown()
{
  AssertIsOnOwningThread();

  for (auto iter = mVSources.Iter(); !iter.Done(); iter.Next()) {
    for (const RefPtr<MediaEngineSource>& source : *iter.UserData()) {
      if (source) {
        source->Shutdown();
      }
    }
  }
  for (auto iter = mASources.Iter(); !iter.Done(); iter.Next()) {
    for (const RefPtr<MediaEngineDefaultAudioSource>& source : *iter.UserData()) {
      if (source) {
        source->Shutdown();
      }
    }
  }
  mVSources.Clear();
  mASources.Clear();
};

} // namespace mozilla
