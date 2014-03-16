/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "nsCOMPtr.h"
#include "nsDOMFile.h"
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

#define VIDEO_RATE USECS_PER_S
#define AUDIO_RATE 16000
#define AUDIO_FRAME_LENGTH ((AUDIO_RATE * MediaEngine::DEFAULT_AUDIO_TIMER_MS) / 1000)
namespace mozilla {

using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS1(MediaEngineDefaultVideoSource, nsITimerCallback)
/**
 * Default video source.
 */

MediaEngineDefaultVideoSource::MediaEngineDefaultVideoSource()
  : mTimer(nullptr), mMonitor("Fake video")
{
  mImageContainer = layers::LayerManager::CreateImageContainer();
  mState = kReleased;
}

MediaEngineDefaultVideoSource::~MediaEngineDefaultVideoSource()
{}

void
MediaEngineDefaultVideoSource::GetName(nsAString& aName)
{
  aName.Assign(NS_LITERAL_STRING("Default Video Device"));
  return;
}

void
MediaEngineDefaultVideoSource::GetUUID(nsAString& aUUID)
{
  aUUID.Assign(NS_LITERAL_STRING("1041FCBD-3F12-4F7B-9E9B-1EC556DD5676"));
  return;
}

nsresult
MediaEngineDefaultVideoSource::Allocate(const MediaEnginePrefs &aPrefs)
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mOpts = aPrefs;
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

  aStream->AddTrack(aID, VIDEO_RATE, 0, new VideoSegment());
  aStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  // Remember TrackID so we can end it later
  mTrackID = aID;

  // Start timer for subsequent frames
  mTimer->InitWithCallback(this, 1000 / mOpts.mFPS, nsITimer::TYPE_REPEATING_SLACK);
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
  aSource->Finish();

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Snapshot(uint32_t aDuration, nsIDOMFile** aFile)
{
  *aFile = nullptr;

#ifndef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  nsAutoString filePath;
  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES, "Browse", title);
  int16_t mode = static_cast<int16_t>(nsIFilePicker::modeOpen);

  nsresult rv = filePicker->Init(nullptr, title, mode);
  NS_ENSURE_SUCCESS(rv, rv);
  filePicker->AppendFilters(nsIFilePicker::filterImages);

  // XXX - This API should be made async
  PRInt16 dialogReturn;
  rv = filePicker->Show(&dialogReturn);
  NS_ENSURE_SUCCESS(rv, rv);
  if (dialogReturn == nsIFilePicker::returnCancel) {
    *aFile = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIFile> localFile;
  filePicker->GetFile(getter_AddRefs(localFile));

  if (!localFile) {
    *aFile = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMFile> domFile = new nsDOMFileFile(localFile);
  domFile.forget(aFile);
  return NS_OK;
#endif
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
  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(ImageFormat::PLANAR_YCBCR);
  nsRefPtr<layers::PlanarYCbCrImage> ycbcr_image =
      static_cast<layers::PlanarYCbCrImage*>(image.get());
  layers::PlanarYCbCrData data;
  AllocateSolidColorFrame(data, mOpts.mWidth, mOpts.mHeight, 0x80, mCb, mCr);
  ycbcr_image->SetData(data);
  // SetData copies data, so we can free the frame
  ReleaseFrame(data);

  MonitorAutoLock lock(mMonitor);

  // implicitly releases last image
  mImage = ycbcr_image.forget();

  return NS_OK;
}

void
MediaEngineDefaultVideoSource::NotifyPull(MediaStreamGraph* aGraph,
                                          SourceMediaStream *aSource,
                                          TrackID aID,
                                          StreamTime aDesiredTime,
                                          TrackTicks &aLastEndTime)
{
  // AddTrack takes ownership of segment
  VideoSegment segment;
  MonitorAutoLock lock(mMonitor);
  if (mState != kStarted) {
    return;
  }

  // Note: we're not giving up mImage here
  nsRefPtr<layers::Image> image = mImage;
  TrackTicks target = TimeToTicksRoundUp(USECS_PER_S, aDesiredTime);
  TrackTicks delta = target - aLastEndTime;

  if (delta > 0) {
    // nullptr images are allowed
    IntSize size(image ? mOpts.mWidth : 0, image ? mOpts.mHeight : 0);
    segment.AppendFrame(image.forget(), delta, size);
    // This can fail if either a) we haven't added the track yet, or b)
    // we've removed or finished the track.
    if (aSource->AppendToTrack(aID, &segment)) {
      aLastEndTime = target;
    }
  }
}

// generate 1k sine wave per second
class SineWaveGenerator : public RefCounted<SineWaveGenerator>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(SineWaveGenerator)
  static const int bytesPerSample = 2;
  static const int millisecondsPerSecond = 1000;
  static const int frequency = 1000;

  SineWaveGenerator(int aSampleRate) :
    mTotalLength(aSampleRate / frequency),
    mReadLength(0) {
    MOZ_ASSERT(mTotalLength * frequency == aSampleRate);
    mAudioBuffer = new int16_t[mTotalLength];
    for(int i = 0; i < mTotalLength; i++) {
      // Set volume to -20db. It's from 32768.0 * 10^(-20/20) = 3276.8
      mAudioBuffer[i] = (3276.8f * sin(2 * M_PI * i / mTotalLength));
    }
  }

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
NS_IMPL_ISUPPORTS1(MediaEngineDefaultAudioSource, nsITimerCallback)

MediaEngineDefaultAudioSource::MediaEngineDefaultAudioSource()
  : mTimer(nullptr)
{
  mState = kReleased;
}

MediaEngineDefaultAudioSource::~MediaEngineDefaultAudioSource()
{}

void
MediaEngineDefaultAudioSource::GetName(nsAString& aName)
{
  aName.Assign(NS_LITERAL_STRING("Default Audio Device"));
  return;
}

void
MediaEngineDefaultAudioSource::GetUUID(nsAString& aUUID)
{
  aUUID.Assign(NS_LITERAL_STRING("B7CBD7C1-53EF-42F9-8353-73F61C70C092"));
  return;
}

nsresult
MediaEngineDefaultAudioSource::Allocate(const MediaEnginePrefs &aPrefs)
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mState = kAllocated;
  // generate 1Khz sine wave
  mSineGenerator = new SineWaveGenerator(AUDIO_RATE);
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
  mSource->AddTrack(aID, AUDIO_RATE, 0, segment);

  // We aren't going to add any more tracks
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  // Remember TrackID so we can finish later
  mTrackID = aID;

  // 1 Audio frame per 10ms
  mTimer->InitWithCallback(this, MediaEngine::DEFAULT_AUDIO_TIMER_MS,
                           nsITimer::TYPE_REPEATING_PRECISE);
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
  aSource->Finish();

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Snapshot(uint32_t aDuration, nsIDOMFile** aFile)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MediaEngineDefaultAudioSource::Notify(nsITimer* aTimer)
{
  AudioSegment segment;
  nsRefPtr<SharedBuffer> buffer = SharedBuffer::Create(AUDIO_FRAME_LENGTH * sizeof(int16_t));
  int16_t* dest = static_cast<int16_t*>(buffer->Data());

  mSineGenerator->generate(dest, AUDIO_FRAME_LENGTH);
  nsAutoTArray<const int16_t*,1> channels;
  channels.AppendElement(dest);
  segment.AppendFrames(buffer.forget(), channels, AUDIO_FRAME_LENGTH);
  mSource->AppendToTrack(mTrackID, &segment);

  return NS_OK;
}

void
MediaEngineDefault::EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >* aVSources) {
  MutexAutoLock lock(mMutex);

  // We once had code here to find a VideoSource with the same settings and re-use that.
  // This no longer is possible since the resolution is being set in Allocate().

  nsRefPtr<MediaEngineVideoSource> newSource = new MediaEngineDefaultVideoSource();
  mVSources.AppendElement(newSource);
  aVSources->AppendElement(newSource);

  return;
}

void
MediaEngineDefault::EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >* aASources) {
  MutexAutoLock lock(mMutex);
  int32_t len = mASources.Length();

  for (int32_t i = 0; i < len; i++) {
    nsRefPtr<MediaEngineAudioSource> source = mASources.ElementAt(i);
    if (source->IsAvailable()) {
      aASources->AppendElement(source);
    }
  }

  // All streams are currently busy, just make a new one.
  if (aASources->Length() == 0) {
    nsRefPtr<MediaEngineAudioSource> newSource =
      new MediaEngineDefaultAudioSource();
    mASources.AppendElement(newSource);
    aASources->AppendElement(newSource);
  }
  return;
}

} // namespace mozilla
