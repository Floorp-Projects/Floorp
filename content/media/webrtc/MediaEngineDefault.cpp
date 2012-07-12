/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaEngineDefault.h"

#include "nsCOMPtr.h"
#include "nsDOMFile.h"
#include "nsILocalFile.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "nsISupportsUtils.h"
#endif

#define WIDTH 320
#define HEIGHT 240
#define FPS 10
#define CHANNELS 1
#define RATE USECS_PER_S

namespace mozilla {

NS_IMPL_THREADSAFE_ISUPPORTS1(MediaEngineDefaultVideoSource, nsITimerCallback)
/**
 * Default video source.
 */
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
MediaEngineDefaultVideoSource::Allocate()
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

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

MediaEngineVideoOptions
MediaEngineDefaultVideoSource::GetOptions()
{
  MediaEngineVideoOptions aOpts;
  aOpts.mWidth = WIDTH;
  aOpts.mHeight = HEIGHT;
  aOpts.mMaxFPS = FPS;
  aOpts.codecType = kVideoCodecI420;
  return aOpts;
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

  mSource = aStream;

  // Allocate a single blank Image
  layers::Image::Format format = layers::Image::PLANAR_YCBCR;
  mImageContainer = layers::LayerManager::CreateImageContainer();

  nsRefPtr<layers::Image> image = mImageContainer->CreateImage(&format, 1);

  int len = ((WIDTH * HEIGHT) * 3 / 2);
  mImage = static_cast<layers::PlanarYCbCrImage*>(image.get());
  PRUint8* frame = (PRUint8*) PR_Malloc(len);
  memset(frame, 0x80, len); // Gray

  const PRUint8 lumaBpp = 8;
  const PRUint8 chromaBpp = 4;

  layers::PlanarYCbCrImage::Data data;
  data.mYChannel = frame;
  data.mYSize = gfxIntSize(WIDTH, HEIGHT);
  data.mYStride = WIDTH * lumaBpp / 8.0;
  data.mCbCrStride = WIDTH * chromaBpp / 8.0;
  data.mCbChannel = frame + HEIGHT * data.mYStride;
  data.mCrChannel = data.mCbChannel + HEIGHT * data.mCbCrStride / 2;
  data.mCbCrSize = gfxIntSize(WIDTH / 2, HEIGHT / 2);
  data.mPicX = 0;
  data.mPicY = 0;
  data.mPicSize = gfxIntSize(WIDTH, HEIGHT);
  data.mStereoMode = layers::STEREO_MODE_MONO;

  // SetData copies data, so we can free the frame
  mImage->SetData(data);
  PR_Free(frame);

  // AddTrack takes ownership of segment
  VideoSegment *segment = new VideoSegment();
  segment->AppendFrame(image.forget(), USECS_PER_S / FPS, gfxIntSize(WIDTH, HEIGHT));
  mSource->AddTrack(aID, RATE, 0, segment);

  // We aren't going to add any more tracks
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  // Remember TrackID so we can end it later
  mTrackID = aID;

  // Start timer for subsequent frames
  mTimer->InitWithCallback(this, 1000 / FPS, nsITimer::TYPE_REPEATING_SLACK);
  mState = kStarted;

  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Stop()
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mTimer->Cancel();
  mTimer = NULL;

  mSource->EndTrack(mTrackID);
  mSource->Finish();

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultVideoSource::Snapshot(PRUint32 aDuration, nsIDOMFile** aFile)
{
  *aFile = nsnull;

#ifndef MOZ_WIDGET_ANDROID
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  if (!AndroidBridge::Bridge()) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString filePath;
  AndroidBridge::Bridge()->ShowFilePickerForMimeType(filePath, NS_LITERAL_STRING("image/*"));

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(filePath, false, getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aFile = new nsDOMFileFile(file));
  return NS_OK;
#endif
}

NS_IMETHODIMP
MediaEngineDefaultVideoSource::Notify(nsITimer* aTimer)
{
  VideoSegment segment;

  nsRefPtr<layers::PlanarYCbCrImage> image = mImage;
  segment.AppendFrame(image.forget(), USECS_PER_S / FPS, gfxIntSize(WIDTH, HEIGHT));
  mSource->AppendToTrack(mTrackID, &segment);

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(MediaEngineDefaultAudioSource, nsITimerCallback)
/**
 * Default audio source.
 */
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
MediaEngineDefaultAudioSource::Allocate()
{
  if (mState != kReleased) {
    return NS_ERROR_FAILURE;
  }

  mState = kAllocated;
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
  segment->Init(CHANNELS);
  mSource->AddTrack(aID, RATE, 0, segment);

  // We aren't going to add any more tracks
  mSource->AdvanceKnownTracksTime(STREAM_TIME_MAX);

  // Remember TrackID so we can finish later
  mTrackID = aID;

  // 1 Audio frame per Video frame
  mTimer->InitWithCallback(this, 1000 / FPS, nsITimer::TYPE_REPEATING_SLACK);
  mState = kStarted;

  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Stop()
{
  if (mState != kStarted) {
    return NS_ERROR_FAILURE;
  }
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }

  mTimer->Cancel();
  mTimer = NULL;

  mSource->EndTrack(mTrackID);
  mSource->Finish();

  mState = kStopped;
  return NS_OK;
}

nsresult
MediaEngineDefaultAudioSource::Snapshot(PRUint32 aDuration, nsIDOMFile** aFile)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MediaEngineDefaultAudioSource::Notify(nsITimer* aTimer)
{
  AudioSegment segment;
  segment.Init(CHANNELS);
  segment.InsertNullDataAtStart(1);

  mSource->AppendToTrack(mTrackID, &segment);

  return NS_OK;
}

void
MediaEngineDefault::EnumerateVideoDevices(nsTArray<nsRefPtr<MediaEngineVideoSource> >* aVSources) {
  aVSources->AppendElement(mVSource);
  return;
}

void
MediaEngineDefault::EnumerateAudioDevices(nsTArray<nsRefPtr<MediaEngineAudioSource> >* aASources) {
  aASources->AppendElement(mASource);
  return;
}

} // namespace mozilla
