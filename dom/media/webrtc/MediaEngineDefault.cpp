/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "nsCOMPtr.h"
#include "mozilla/dom/File.h"
#include "nsILocalFile.h"
#include "Layers.h"
#include "ImageContainer.h"
#include "ImageTypes.h"
#include "prmem.h"
#include "nsContentUtils.h"

#include "nsIFilePicker.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "nsISupportsUtils.h"
#endif

#ifdef MOZ_WEBRTC
#include "YuvStamper.h"
#endif

#define AUDIO_RATE mozilla::MediaEngine::DEFAULT_SAMPLE_RATE
#define AUDIO_FRAME_LENGTH ((AUDIO_RATE * MediaEngine::DEFAULT_AUDIO_TIMER_MS) / 1000)
namespace mozilla {

using namespace mozilla::gfx;

// Enable the testing flag fakeTracks and fake in MediaStreamConstraints, will
// return you a MediaStream with additional fake video tracks and audio tracks.
static const int kFakeVideoTrackCount = 2;
static const int kFakeAudioTrackCount = 3;

NS_IMPL_ISUPPORTS(MediaEngineDefaultVideoSource, nsITimerCallback)
/**
 * Default video source.
 */

MediaEngineDefaultVideoSource::MediaEngineDefaultVideoSource()
  : MediaEngineVideoSource(kReleased)
  , mTimer(nullptr)
  , mMonitor("Fake video")
  , mCb(16), mCr(16)
{
  mImageContainer = layers::LayerManager::CreateImageContainer();
}

MediaEngineDefaultVideoSource::~MediaEngineDefaultVideoSource()
{}

void
MediaEngineDefaultVideoSource::GetName(nsAString& aName)
{
  aName.AssignLiteral(MOZ_UTF16("Default Video Device"));
  return;
}

void
MediaEngineDefaultVideoSource::GetUUID(nsACString& aUUID)
{
  aUUID.AssignLiteral("1041FCBD-3F12-4F7B-9E9B-1EC556DD5676");
  return;
}

uint32_t
MediaEngineDefaultVideoSource::GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId)
{
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const dom::MediaTrackConstraintSet* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, false, aDeviceId);
    break; // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult
MediaEngineDefaultVideoSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                        const MediaEnginePrefs &aPrefs,
                                        const nsString& aDeviceId)
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mOpts = aPrefs;
  mOpts.mWidth = mOpts.mWidth ? mOpts.mWidth : MediaEngine::DEFAULT_43_VIDEO_WIDTH;
  mOpts.mHeight = mOpts.mHeight ? mOpts.mHeight : MediaEngine::DEFAULT_43_VIDEO_HEIGHT;
  mState = kAllocated;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Deallocate()
{
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  mState = kReleased;
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
  uint8_t* frame = (uint8_t*) PR_Malloc(yLen+cbLen+crLen);
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
  PR_Free(aData.mYChannel);
}

nsresult
MediaEngineDefaultVideoSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  if (mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  aStream->AddTrack(aID, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);

  if (mHasFakeTracks) {
    for (int i = 0; i < kFakeVideoTrackCount; ++i) {
      aStream->AddTrack(kTrackCount + i, 0, new VideoSegment(), SourceMediaStream::ADDTRACK_QUEUED);
    }
  }

  // Remember TrackID so we can end it later
  mTrackID = aID;

  // Start timer for subsequent frames
#if defined(MOZ_WIDGET_GONK) && defined(DEBUG)
// B2G emulator debug is very, very slow and has problems dealing with realtime audio inputs
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
  if (mHasFakeTracks) {
    for (int i = 0; i < kFakeVideoTrackCount; ++i) {
      aSource->EndTrack(kTrackCount + i);
    }
  }

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Restart(const dom::MediaTrackConstraints& aConstraints,
                                       const MediaEnginePrefs &aPrefs,
                                       const nsString& aDeviceId)
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
  RefPtr<layers::Image> image = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  RefPtr<layers::PlanarYCbCrImage> ycbcr_image =
      static_cast<layers::PlanarYCbCrImage*>(image.get());
  layers::PlanarYCbCrData data;
  AllocateSolidColorFrame(data, mOpts.mWidth, mOpts.mHeight, 0x80, mCb, mCr);

#ifdef MOZ_WEBRTC
  uint64_t timestamp = PR_Now();
  YuvStamper::Encode(mOpts.mWidth, mOpts.mHeight, mOpts.mWidth,
		     data.mYChannel,
		     reinterpret_cast<unsigned char*>(&timestamp), sizeof(timestamp),
		     0, 0);
#endif

  bool setData = ycbcr_image->SetData(data);
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

void
MediaEngineDefaultVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                          SourceMediaStream *aSource,
                                          TrackID aID,
                                          StreamTime aDesiredTime)
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
    segment.AppendFrame(image.forget(), delta, size);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    aSource->AppendToTrack(aID, &segment);
    // Generate null data for fake tracks.
    if (mHasFakeTracks) {
      for (int i = 0; i < kFakeVideoTrackCount; ++i) {
        VideoSegment nullSegment;
        nullSegment.AppendNullData(delta);
        aSource->AppendToTrack(kTrackCount + i, &nullSegment);
      }
    }
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
    mAudioBuffer = new int16_t[mTotalLength];
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
      memcpy(aBuffer, mAudioBuffer + mReadLength, processSamples * bytesPerSample);
      aBuffer += processSamples;
      mReadLength += processSamples;
      remaining -= processSamples;
      if (mReadLength == mTotalLength) {
        mReadLength = 0;
      }
    }
  }

private:
  nsAutoArrayPtr<int16_t> mAudioBuffer;
  int16_t mTotalLength;
  int16_t mReadLength;
};

/**
 * Default audio source.
 */
NS_IMPL_ISUPPORTS(MediaEngineDefaultAudioSource, nsITimerCallback)

MediaEngineDefaultAudioSource::MediaEngineDefaultAudioSource()
  : MediaEngineAudioSource(kReleased)
  , mTimer(nullptr)
{
}

MediaEngineDefaultAudioSource::~MediaEngineDefaultAudioSource()
{}

void
MediaEngineDefaultAudioSource::GetName(nsAString& aName)
{
  aName.AssignLiteral(MOZ_UTF16("Default Audio Device"));
  return;
}

void
MediaEngineDefaultAudioSource::GetUUID(nsACString& aUUID)
{
  aUUID.AssignLiteral("B7CBD7C1-53EF-42F9-8353-73F61C70C092");
  return;
}

uint32_t
MediaEngineDefaultAudioSource::GetBestFitnessDistance(
    const nsTArray<const dom::MediaTrackConstraintSet*>& aConstraintSets,
    const nsString& aDeviceId)
{
  uint32_t distance = 0;
#ifdef MOZ_WEBRTC
  for (const dom::MediaTrackConstraintSet* cs : aConstraintSets) {
    distance = GetMinimumFitnessDistance(*cs, false, aDeviceId);
    break; // distance is read from first entry only
  }
#endif
  return distance;
}

nsresult
MediaEngineDefaultAudioSource::Allocate(const dom::MediaTrackConstraints &aConstraints,
                                        const MediaEnginePrefs &aPrefs,
                                        const nsString& aDeviceId)
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mState = kAllocated;
  // generate sine wave (default 1KHz)
  mSineGenerator = new SineWaveGenerator(AUDIO_RATE,
                                         static_cast<uint32_t>(aPrefs.mFreq ? aPrefs.mFreq : 1000));
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Deallocate()
{
  if (mState != kStopped && mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }
  mState = kReleased;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Start(SourceMediaStream* aStream, TrackID aID)
{
  if (mState != kAllocated) {
    return NS_ERROR_FAILURE;
  }

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mSource = aStream;

  // AddTrack will take ownership of segment
  AudioSegment* segment = new AudioSegment();
  mSource->AddAudioTrack(aID, AUDIO_RATE, 0, segment, SourceMediaStream::ADDTRACK_QUEUED);

  if (mHasFakeTracks) {
    for (int i = 0; i < kFakeAudioTrackCount; ++i) {
      mSource->AddAudioTrack(kTrackCount + kFakeVideoTrackCount+i,
                             AUDIO_RATE, 0, new AudioSegment(), SourceMediaStream::ADDTRACK_QUEUED);
    }
  }

  // Remember TrackID so we can finish later
  mTrackID = aID;

  // 1 Audio frame per 10ms
#if defined(MOZ_WIDGET_GONK) && defined(DEBUG)
// B2G emulator debug is very, very slow and has problems dealing with realtime audio inputs
  mTimer->InitWithCallback(this, MediaEngine::DEFAULT_AUDIO_TIMER_MS*10,
                           nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
#else
  mTimer->InitWithCallback(this, MediaEngine::DEFAULT_AUDIO_TIMER_MS,
                           nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
#endif
  mState = kStarted;

  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Stop(SourceMediaStream *aSource, TrackID aID)
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
  if (mHasFakeTracks) {
    for (int i = 0; i < kFakeAudioTrackCount; ++i) {
      aSource->EndTrack(kTrackCount + kFakeVideoTrackCount+i);
    }
  }

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Restart(const dom::MediaTrackConstraints& aConstraints,
                                       const MediaEnginePrefs &aPrefs,
                                       const nsString& aDeviceId)
{
  return NS_OK;
}

NS_IMETHODIMP
MediaEngineDefaultAudioSource::Notify(nsITimer* aTimer)
{
  AudioSegment segment;
  RefPtr<SharedBuffer> buffer = SharedBuffer::Create(AUDIO_FRAME_LENGTH * sizeof(int16_t));
  int16_t* dest = static_cast<int16_t*>(buffer->Data());

  mSineGenerator->generate(dest, AUDIO_FRAME_LENGTH);
  nsAutoTArray<const int16_t*,1> channels;
  channels.AppendElement(dest);
  segment.AppendFrames(buffer.forget(), channels, AUDIO_FRAME_LENGTH);
  mSource->AppendToTrack(mTrackID, &segment);

  // Generate null data for fake tracks.
  if (mHasFakeTracks) {
    for (int i = 0; i < kFakeAudioTrackCount; ++i) {
      AudioSegment nullSegment;
      nullSegment.AppendNullData(AUDIO_FRAME_LENGTH);
      mSource->AppendToTrack(kTrackCount + kFakeVideoTrackCount+i, &nullSegment);
    }
  }
  return NS_OK;
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
  newSource->SetHasFakeTracks(mHasFakeTracks);
  mVSources.AppendElement(newSource);
  aVSources->AppendElement(newSource);

  return;
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
    newSource->SetHasFakeTracks(mHasFakeTracks);
    mASources.AppendElement(newSource);
    aASources->AppendElement(newSource);
  }
  return;
}

} // namespace mozilla
