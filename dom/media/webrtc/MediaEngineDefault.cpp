/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "nsCOMPtr.h"
#include "mozilla/dom/File.h"
#include "mozilla/UniquePtr.h"
#include "nsIFile.h"
#include "Layers.h"
#include "ImageContainer.h"
#include "ImageTypes.h"
#include "nsContentUtils.h"
#include "MediaStreamGraph.h"

#include "nsIFilePicker.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#ifdef MOZ_WIDGET_ANDROID
#include "nsISupportsUtils.h"
#endif

#ifdef MOZ_WEBRTC
#include "YuvStamper.h"
#endif

#define AUDIO_RATE mozilla::MediaEngine::DEFAULT_SAMPLE_RATE
#define DEFAULT_AUDIO_TIMER_MS 10
namespace mozilla {

using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS(MediaEngineDefaultVideoSource, nsITimerCallback, nsINamed)
/**
 * Default video source.
 */

MediaEngineDefaultVideoSource::MediaEngineDefaultVideoSource()
#ifdef MOZ_WEBRTC
  : MediaEngineCameraVideoSource("FakeVideo.Monitor")
#else
  : MediaEngineVideoSource()
#endif
  , mTimer(nullptr)
#ifndef MOZ_WEBRTC
  , mMonitor("Fake video")
#endif
  , mCb(16), mCr(16)
{
  mImageContainer =
    layers::LayerManager::CreateImageContainer(layers::ImageContainer::ASYNCHRONOUS);
}

MediaEngineDefaultVideoSource::~MediaEngineDefaultVideoSource()
{}

void
MediaEngineDefaultVideoSource::GetName(nsAString& aName) const
{
  aName.AssignLiteral(u"Default Video Device");
}

void
MediaEngineDefaultVideoSource::GetUUID(nsACString& aUUID) const
{
  aUUID.AssignLiteral("1041FCBD-3F12-4F7B-9E9B-1EC556DD5676");
}

uint32_t
MediaEngineDefaultVideoSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, aDeviceId);
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
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

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
                              MediaEngine::DEFAULT_43_VIDEO_WIDTH/2
#else
                              MediaEngine::DEFAULT_43_VIDEO_WIDTH
#endif
                              );
  mOpts.mHeight = c.mHeight.Get(aPrefs.mHeight ? aPrefs.mHeight :
#ifdef DEBUG
                                MediaEngine::DEFAULT_43_VIDEO_HEIGHT/2
#else
                                MediaEngine::DEFAULT_43_VIDEO_HEIGHT
#endif
                                );
  mState = kAllocated;
  *aOutHandle = nullptr;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Deallocate(AllocationHandle* aHandle)
{
  MOZ_ASSERT(!aHandle);
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  mState = kReleased;
  mImage = nullptr;
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
MediaEngineDefaultVideoSource::Start(SourceMediaStream* aStream, TrackID aID,
                                     const PrincipalHandle& aPrincipalHandle)
{
  if (mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  aStream->AddTrack(aID, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);

  // Remember TrackID so we can end it later
  mTrackID = aID;

  // Start timer for subsequent frames
#if defined(MOZ_WIDGET_ANDROID) && defined(DEBUG)
// emulator debug is very, very slow and has problems dealing with realtime audio inputs
  mTimer->InitWithCallback(this, (1000 / mOpts.mFPS)*10, nsITimer::TYPE_REPEATING_SLACK);
#else
  mTimer->InitWithCallback(this, 1000 / mOpts.mFPS, nsITimer::TYPE_REPEATING_SLACK);
#endif
  mState = kStarted;

  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mTimer->Cancel();
  mTimer = nullptr;

  aSource->EndTrack(aID);

  mState = kStopped;
  mImage = nullptr;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Restart(
    AllocationHandle* aHandle,
    const dom::MediaTrackConstraints& aConstraints,
    const MediaEnginePrefs &aPrefs,
    const nsString& aDeviceId,
    const char** aOutBadConstraint)
{
  return NS_OK;
}

NS_IMETHODIMP
MediaEngineDefaultVideoSource::Notify(nsITimer* aTimer)
{
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
    return NS_ERROR_FAILURE;
  }

  MonitorAutoLock lock(mMonitor);

  // implicitly releases last image
  mImage = ycbcr_image.forget();

  return NS_OK;
}

NS_IMETHODIMP
MediaEngineDefaultVideoSource::GetName(nsACString& aName)
{
  aName.AssignLiteral("MediaEngineDefaultVideoSource");
  return NS_OK;
}

void
MediaEngineDefaultVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                          SourceMediaStream *aSource,
                                          TrackID aID,
                                          StreamTime aDesiredTime,
                                          const PrincipalHandle& aPrincipalHandle)
{
  // AddTrack takes ownership of segment
  VideoSegment segment;
  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted) {
    return;
  }

  // Note: we're not giving up mImage here
  RefPtr<layers::Image> image = mImage;
  StreamTime delta = aDesiredTime - aSource->GetEndOfAppendedData(aID);

  if (delta > 0) {
    // nullptr images are allowed
    IntSize size(image ? mOpts.mWidth : 0, image ? mOpts.mHeight : 0);
    segment.AppendFrame(image.forget(), delta, size, aPrincipalHandle);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    aSource->AppendToTrack(aID, &segment);
  }
}

// generate 1k sine wave per second
class SineWaveGenerator
{
public:
  static const int bytesPerSample = 2;
  static const int millisecondsPerSecond = PR_MSEC_PER_SEC;

  explicit SineWaveGenerator(uint32_t aSampleRate, uint32_t aFrequency) :
    mTotalLength(aSampleRate / aFrequency),
    mReadLength(0) {
    // If we allow arbitrary frequencies, there's no guarantee we won't get rounded here
    // We could include an error term and adjust for it in generation; not worth the trouble
    //MOZ_ASSERT(mTotalLength * aFrequency == aSampleRate);
    mAudioBuffer = MakeUnique<int16_t[]>(mTotalLength);
    for (int i = 0; i < mTotalLength; i++) {
      // Set volume to -20db. It's from 32768.0 * 10^(-20/20) = 3276.8
      mAudioBuffer[i] = (3276.8f * sin(2 * M_PI * i / mTotalLength));
    }
  }

  // NOTE: only safely called from a single thread (MSG callback)
  void generate(int16_t* aBuffer, int16_t aLengthInSamples) {
    int16_t remaining = aLengthInSamples;

    while (remaining) {
      int16_t processSamples = 0;

      if (mTotalLength - mReadLength >= remaining) {
        processSamples = remaining;
      } else {
        processSamples = mTotalLength - mReadLength;
      }
      memcpy(aBuffer, &mAudioBuffer[mReadLength], processSamples * bytesPerSample);
      aBuffer += processSamples;
      mReadLength += processSamples;
      remaining -= processSamples;
      if (mReadLength == mTotalLength) {
        mReadLength = 0;
      }
    }
  }

private:
  UniquePtr<int16_t[]> mAudioBuffer;
  int16_t mTotalLength;
  int16_t mReadLength;
};

/**
 * Default audio source.
 */

NS_IMPL_ISUPPORTS0(MediaEngineDefaultAudioSource)

MediaEngineDefaultAudioSource::MediaEngineDefaultAudioSource()
  : MediaEngineAudioSource(kReleased)
  , mLastNotify(0)
{}

MediaEngineDefaultAudioSource::~MediaEngineDefaultAudioSource()
{}

void
MediaEngineDefaultAudioSource::GetName(nsAString& aName) const
{
  aName.AssignLiteral(u"Default Audio Device");
}

void
MediaEngineDefaultAudioSource::GetUUID(nsACString& aUUID) const
{
  aUUID.AssignLiteral("B7CBD7C1-53EF-42F9-8353-73F61C70C092");
}

uint32_t
MediaEngineDefaultAudioSource::GetBestFitnessDistance(
    const nsTArray<const NormalizedConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId) const
{
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const auto* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, aDeviceId);
    break; // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult
MediaEngineDefaultAudioSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                        const MediaEnginePrefs &aPrefs,
                                        const nsString& aDeviceId,
                                        const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                                        AllocationHandle** aOutHandle,
                                        const char** aOutBadConstraint)
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  // Mock failure for automated tests.
  if (aConstraints.mDeviceId.IsString() &&
      aConstraints.mDeviceId.GetAsString().EqualsASCII("bad device")) {
    return NS_ERROR_FAILURE;
  }

  mState = kAllocated;
  // generate sine wave (default 1KHz)
  mSineGenerator = new SineWaveGenerator(AUDIO_RATE,
                                         static_cast<uint32_t>(aPrefs.mFreq ? aPrefs.mFreq : 1000));
  *aOutHandle = nullptr;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Deallocate(AllocationHandle* aHandle)
{
  MOZ_ASSERT(!aHandle);
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  mState = kReleased;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Start(SourceMediaStream* aStream, TrackID aID,
                                     const PrincipalHandle& aPrincipalHandle)
{
  if (mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  // AddTrack will take ownership of segment
  AudioSegment* segment = new AudioSegment();
  aStream->AddAudioTrack(aID, AUDIO_RATE, 0, segment, SourceMediaStream::ADDTRACK_QUEUED);

  // Remember TrackID so we can finish later
  mTrackID = aID;

  mLastNotify = 0;
  mState = kStarted;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Stop(SourceMediaStream *aSource, TrackID aID)
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
  aSource->EndTrack(aID);

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Restart(AllocationHandle* aHandle,
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
MediaEngineDefaultAudioSource::NotifyPull(MediaStreamGraph* aGraph,
                                          SourceMediaStream *aSource,
                                          TrackID aID,
                                          StreamTime aDesiredTime,
                                          const PrincipalHandle& aPrincipalHandle)
{
  MOZ_ASSERT(aID == mTrackID);
  AudioSegment segment;
  // avoid accumulating rounding errors
  TrackTicks desired = aSource->TimeToTicksRoundUp(AUDIO_RATE, aDesiredTime);
  TrackTicks delta = desired - mLastNotify;
  mLastNotify += delta;
  AppendToSegment(segment, delta, aPrincipalHandle);
  aSource->AppendToTrack(mTrackID, &segment);
}

void
MediaEngineDefault::EnumerateVideoDevices(dom::MediaSourceEnum aMediaSource,
                                          nsTArray<RefPtr<MediaEngineVideoSource> >* aVSources) {
  MutexAutoLock lock(mMutex);

  // only supports camera sources (for now).  See Bug 1038241
  if (aMediaSource != dom::MediaSourceEnum::Camera) {
    return;
  }

  // We once had code here to find a VideoSource with the same settings and re-use that.
  // This no longer is possible since the resolution is being set in Allocate().

  RefPtr<MediaEngineVideoSource> newSource = new MediaEngineDefaultVideoSource();
  mVSources.AppendElement(newSource);
  aVSources->AppendElement(newSource);
}

void
MediaEngineDefault::EnumerateAudioDevices(dom::MediaSourceEnum aMediaSource,
                                          nsTArray<RefPtr<MediaEngineAudioSource> >* aASources) {
  MutexAutoLock lock(mMutex);
  int32_t len = mASources.Length();

  // aMediaSource is ignored for audio devices (for now).

  for (int32_t i = 0; i < len; i++) {
    RefPtr<MediaEngineAudioSource> source = mASources.ElementAt(i);
    if (source->IsAvailable()) {
      aASources->AppendElement(source);
    }
  }

  // All streams are currently busy, just make a new one.
  if (aASources->Length() == 0) {
    RefPtr<MediaEngineAudioSource> newSource =
      new MediaEngineDefaultAudioSource();
    mASources.AppendElement(newSource);
    aASources->AppendElement(newSource);
  }
}

} // namespace mozilla
