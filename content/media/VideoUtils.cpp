/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoUtils.h"
#include "MediaResource.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsMathUtils.h"
#include "nsSize.h"
#include "VorbisUtils.h"
#include "ImageContainer.h"
#include "SharedThreadPool.h"
#include "mozilla/Preferences.h"

#include <stdint.h>

namespace mozilla {

using layers::PlanarYCbCrImage;

// Converts from number of audio frames to microseconds, given the specified
// audio rate.
CheckedInt64 FramesToUsecs(int64_t aFrames, uint32_t aRate) {
  return (CheckedInt64(aFrames) * USECS_PER_S) / aRate;
}

// Converts from microseconds to number of audio frames, given the specified
// audio rate.
CheckedInt64 UsecsToFrames(int64_t aUsecs, uint32_t aRate) {
  return (CheckedInt64(aUsecs) * aRate) / USECS_PER_S;
}

nsresult SecondsToUsecs(double aSeconds, int64_t& aOutUsecs) {
  if (aSeconds * double(USECS_PER_S) > INT64_MAX) {
    return NS_ERROR_FAILURE;
  }
  aOutUsecs = int64_t(aSeconds * double(USECS_PER_S));
  return NS_OK;
}

static int32_t ConditionDimension(float aValue)
{
  // This will exclude NaNs and too-big values.
  if (aValue > 1.0 && aValue <= INT32_MAX)
    return int32_t(NS_round(aValue));
  return 0;
}

void ScaleDisplayByAspectRatio(nsIntSize& aDisplay, float aAspectRatio)
{
  if (aAspectRatio > 1.0) {
    // Increase the intrinsic width
    aDisplay.width = ConditionDimension(aAspectRatio * aDisplay.width);
  } else {
    // Increase the intrinsic height
    aDisplay.height = ConditionDimension(aDisplay.height / aAspectRatio);
  }
}

static int64_t BytesToTime(int64_t offset, int64_t length, int64_t durationUs) {
  NS_ASSERTION(length > 0, "Must have positive length");
  double r = double(offset) / double(length);
  if (r > 1.0)
    r = 1.0;
  return int64_t(double(durationUs) * r);
}

void GetEstimatedBufferedTimeRanges(mozilla::MediaResource* aStream,
                                    int64_t aDurationUsecs,
                                    mozilla::dom::TimeRanges* aOutBuffered)
{
  // Nothing to cache if the media takes 0us to play.
  if (aDurationUsecs <= 0 || !aStream || !aOutBuffered)
    return;

  // Special case completely cached files.  This also handles local files.
  if (aStream->IsDataCachedToEndOfResource(0)) {
    aOutBuffered->Add(0, double(aDurationUsecs) / USECS_PER_S);
    return;
  }

  int64_t totalBytes = aStream->GetLength();

  // If we can't determine the total size, pretend that we have nothing
  // buffered. This will put us in a state of eternally-low-on-undecoded-data
  // which is not great, but about the best we can do.
  if (totalBytes <= 0)
    return;

  int64_t startOffset = aStream->GetNextCachedData(0);
  while (startOffset >= 0) {
    int64_t endOffset = aStream->GetCachedDataEnd(startOffset);
    // Bytes [startOffset..endOffset] are cached.
    NS_ASSERTION(startOffset >= 0, "Integer underflow in GetBuffered");
    NS_ASSERTION(endOffset >= 0, "Integer underflow in GetBuffered");

    int64_t startUs = BytesToTime(startOffset, totalBytes, aDurationUsecs);
    int64_t endUs = BytesToTime(endOffset, totalBytes, aDurationUsecs);
    if (startUs != endUs) {
      aOutBuffered->Add(double(startUs) / USECS_PER_S,
                        double(endUs) / USECS_PER_S);
    }
    startOffset = aStream->GetNextCachedData(endOffset);
  }
  return;
}

int DownmixAudioToStereo(mozilla::AudioDataValue* buffer,
                         int channels, uint32_t frames)
{
  int outChannels;
  outChannels = 2;
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
  // Downmix matrix. Per-row normalization 1 for rows 3,4 and 2 for rows 5-8.
  static const float dmatrix[6][8][2]= {
      /*3*/{{0.5858f,0},{0.4142f,0.4142f},{0,     0.5858f}},
      /*4*/{{0.4226f,0},{0,      0.4226f},{0.366f,0.2114f},{0.2114f,0.366f}},
      /*5*/{{0.6510f,0},{0.4600f,0.4600f},{0,     0.6510f},{0.5636f,0.3254f},{0.3254f,0.5636f}},
      /*6*/{{0.5290f,0},{0.3741f,0.3741f},{0,     0.5290f},{0.4582f,0.2645f},{0.2645f,0.4582f},{0.3741f,0.3741f}},
      /*7*/{{0.4553f,0},{0.3220f,0.3220f},{0,     0.4553f},{0.3943f,0.2277f},{0.2277f,0.3943f},{0.2788f,0.2788f},{0.3220f,0.3220f}},
      /*8*/{{0.3886f,0},{0.2748f,0.2748f},{0,     0.3886f},{0.3366f,0.1943f},{0.1943f,0.3366f},{0.3366f,0.1943f},{0.1943f,0.3366f},{0.2748f,0.2748f}},
  };
  // Re-write the buffer with downmixed data
  for (uint32_t i = 0; i < frames; i++) {
    float sampL = 0.0;
    float sampR = 0.0;
    for (int j = 0; j < channels; j++) {
      sampL+=buffer[i*channels+j]*dmatrix[channels-3][j][0];
      sampR+=buffer[i*channels+j]*dmatrix[channels-3][j][1];
    }
    buffer[i*outChannels]=sampL;
    buffer[i*outChannels+1]=sampR;
  }
#else
  // Downmix matrix. Per-row normalization 1 for rows 3,4 and 2 for rows 5-8.
  // Coefficients in Q14.
  static const int16_t dmatrix[6][8][2]= {
      /*3*/{{9598, 0},{6786,6786},{0,   9598}},
      /*4*/{{6925, 0},{0,   6925},{5997,3462},{3462,5997}},
      /*5*/{{10663,0},{7540,7540},{0,  10663},{9234,5331},{5331,9234}},
      /*6*/{{8668, 0},{6129,6129},{0,   8668},{7507,4335},{4335,7507},{6129,6129}},
      /*7*/{{7459, 0},{5275,5275},{0,   7459},{6460,3731},{3731,6460},{4568,4568},{5275,5275}},
      /*8*/{{6368, 0},{4502,4502},{0,   6368},{5514,3184},{3184,5514},{5514,3184},{3184,5514},{4502,4502}}
  };
  // Re-write the buffer with downmixed data
  for (uint32_t i = 0; i < frames; i++) {
    int32_t sampL = 0;
    int32_t sampR = 0;
    for (int j = 0; j < channels; j++) {
      sampL+=buffer[i*channels+j]*dmatrix[channels-3][j][0];
      sampR+=buffer[i*channels+j]*dmatrix[channels-3][j][1];
    }
    sampL = (sampL + 8192)>>14;
    buffer[i*outChannels] = static_cast<mozilla::AudioDataValue>(MOZ_CLIP_TO_15(sampL));
    sampR = (sampR + 8192)>>14;
    buffer[i*outChannels+1] = static_cast<mozilla::AudioDataValue>(MOZ_CLIP_TO_15(sampR));
  }
#endif
  return outChannels;
}

bool
IsVideoContentType(const nsCString& aContentType)
{
  NS_NAMED_LITERAL_CSTRING(video, "video");
  if (FindInReadable(video, aContentType)) {
    return true;
  }
  return false;
}

bool
IsValidVideoRegion(const nsIntSize& aFrame, const nsIntRect& aPicture,
                   const nsIntSize& aDisplay)
{
  return
    aFrame.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aFrame.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aFrame.width * aFrame.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aFrame.width * aFrame.height != 0 &&
    aPicture.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.x < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.x + aPicture.width < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.y < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.y + aPicture.height < PlanarYCbCrImage::MAX_DIMENSION &&
    aPicture.width * aPicture.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aPicture.width * aPicture.height != 0 &&
    aDisplay.width <= PlanarYCbCrImage::MAX_DIMENSION &&
    aDisplay.height <= PlanarYCbCrImage::MAX_DIMENSION &&
    aDisplay.width * aDisplay.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
    aDisplay.width * aDisplay.height != 0;
}

TemporaryRef<SharedThreadPool> GetMediaDecodeThreadPool()
{
  return SharedThreadPool::Get(NS_LITERAL_CSTRING("Media Decode"),
                               Preferences::GetUint("media.num-decode-threads", 25));
}

} // end namespace mozilla
