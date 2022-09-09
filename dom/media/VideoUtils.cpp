/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoUtils.h"

#include <functional>
#include <stdint.h>

#include "CubebUtils.h"
#include "ImageContainer.h"
#include "MediaContainerType.h"
#include "MediaResource.h"
#include "TimeUnits.h"
#include "VorbisUtils.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentTypeParser.h"
#include "nsIConsoleService.h"
#include "nsINetworkLinkService.h"
#include "nsIRandomGenerator.h"
#include "nsMathUtils.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "AudioStream.h"

namespace mozilla {

using gfx::ColorRange;
using gfx::CICP::ColourPrimaries;
using gfx::CICP::MatrixCoefficients;
using gfx::CICP::TransferCharacteristics;
using layers::PlanarYCbCrImage;
using media::TimeUnit;

CheckedInt64 SaferMultDiv(int64_t aValue, uint64_t aMul, uint64_t aDiv) {
  if (aMul > INT64_MAX || aDiv > INT64_MAX) {
    return CheckedInt64(INT64_MAX) + 1;  // Return an invalid checked int.
  }
  int64_t mul = aMul;
  int64_t div = aDiv;
  int64_t major = aValue / div;
  int64_t remainder = aValue % div;
  return CheckedInt64(remainder) * mul / div + CheckedInt64(major) * mul;
}

// Converts from number of audio frames to microseconds, given the specified
// audio rate.
CheckedInt64 FramesToUsecs(int64_t aFrames, uint32_t aRate) {
  return SaferMultDiv(aFrames, USECS_PER_S, aRate);
}

TimeUnit FramesToTimeUnit(int64_t aFrames, uint32_t aRate) {
  if (MOZ_UNLIKELY(!aRate)) {
    return TimeUnit::Invalid();
  }
  int64_t major = aFrames / aRate;
  int64_t remainder = aFrames % aRate;
  return TimeUnit::FromMicroseconds(major) * USECS_PER_S +
         (TimeUnit::FromMicroseconds(remainder) * USECS_PER_S) / aRate;
}

// Converts from microseconds to number of audio frames, given the specified
// audio rate.
CheckedInt64 UsecsToFrames(int64_t aUsecs, uint32_t aRate) {
  return SaferMultDiv(aUsecs, aRate, USECS_PER_S);
}

// Format TimeUnit as number of frames at given rate.
CheckedInt64 TimeUnitToFrames(const TimeUnit& aTime, uint32_t aRate) {
  return aTime.IsValid() ? UsecsToFrames(aTime.ToMicroseconds(), aRate)
                         : CheckedInt64(INT64_MAX) + 1;
}

nsresult SecondsToUsecs(double aSeconds, int64_t& aOutUsecs) {
  if (aSeconds * double(USECS_PER_S) > double(INT64_MAX)) {
    return NS_ERROR_FAILURE;
  }
  aOutUsecs = int64_t(aSeconds * double(USECS_PER_S));
  return NS_OK;
}

static int32_t ConditionDimension(float aValue) {
  // This will exclude NaNs and too-big values.
  if (aValue > 1.0 && aValue <= float(INT32_MAX)) {
    return int32_t(NS_round(aValue));
  }
  return 0;
}

void ScaleDisplayByAspectRatio(gfx::IntSize& aDisplay, float aAspectRatio) {
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
  if (r > 1.0) {
    r = 1.0;
  }
  return int64_t(double(durationUs) * r);
}

media::TimeIntervals GetEstimatedBufferedTimeRanges(
    mozilla::MediaResource* aStream, int64_t aDurationUsecs) {
  media::TimeIntervals buffered;
  // Nothing to cache if the media takes 0us to play.
  if (aDurationUsecs <= 0 || !aStream) {
    return buffered;
  }

  // Special case completely cached files.  This also handles local files.
  if (aStream->IsDataCachedToEndOfResource(0)) {
    buffered += media::TimeInterval(TimeUnit::Zero(),
                                    TimeUnit::FromMicroseconds(aDurationUsecs));
    return buffered;
  }

  int64_t totalBytes = aStream->GetLength();

  // If we can't determine the total size, pretend that we have nothing
  // buffered. This will put us in a state of eternally-low-on-undecoded-data
  // which is not great, but about the best we can do.
  if (totalBytes <= 0) {
    return buffered;
  }

  int64_t startOffset = aStream->GetNextCachedData(0);
  while (startOffset >= 0) {
    int64_t endOffset = aStream->GetCachedDataEnd(startOffset);
    // Bytes [startOffset..endOffset] are cached.
    NS_ASSERTION(startOffset >= 0, "Integer underflow in GetBuffered");
    NS_ASSERTION(endOffset >= 0, "Integer underflow in GetBuffered");

    int64_t startUs = BytesToTime(startOffset, totalBytes, aDurationUsecs);
    int64_t endUs = BytesToTime(endOffset, totalBytes, aDurationUsecs);
    if (startUs != endUs) {
      buffered += media::TimeInterval(TimeUnit::FromMicroseconds(startUs),
                                      TimeUnit::FromMicroseconds(endUs));
    }
    startOffset = aStream->GetNextCachedData(endOffset);
  }
  return buffered;
}

void DownmixStereoToMono(mozilla::AudioDataValue* aBuffer, uint32_t aFrames) {
  MOZ_ASSERT(aBuffer);
  const int channels = 2;
  for (uint32_t fIdx = 0; fIdx < aFrames; ++fIdx) {
#ifdef MOZ_SAMPLE_TYPE_FLOAT32
    float sample = 0.0;
#else
    int sample = 0;
#endif
    // The sample of the buffer would be interleaved.
    sample = (aBuffer[fIdx * channels] + aBuffer[fIdx * channels + 1]) * 0.5f;
    aBuffer[fIdx * channels] = aBuffer[fIdx * channels + 1] = sample;
  }
}

uint32_t DecideAudioPlaybackChannels(const AudioInfo& info) {
  if (StaticPrefs::accessibility_monoaudio_enable()) {
    return 1;
  }

  if (StaticPrefs::media_forcestereo_enabled()) {
    return 2;
  }

  return info.mChannels;
}

uint32_t DecideAudioPlaybackSampleRate(const AudioInfo& aInfo) {
  bool resampling = StaticPrefs::media_resampling_enabled();

  uint32_t rate = 0;

  if (resampling) {
    rate = 48000;
  } else if (aInfo.mRate >= 44100) {
    // The original rate is of good quality and we want to minimize unecessary
    // resampling, so we let cubeb decide how to resample (if needed).
    rate = aInfo.mRate;
  } else {
    // We will resample all data to match cubeb's preferred sampling rate.
    rate = AudioStream::GetPreferredRate();
    if (rate > 384000) {
      // bogus rate, fall back to something else;
      rate = 48000;
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(rate, "output rate can't be 0.");

  return rate;
}

bool IsDefaultPlaybackDeviceMono() {
  return CubebUtils::MaxNumberOfChannels() == 1;
}

bool IsVideoContentType(const nsCString& aContentType) {
  constexpr auto video = "video"_ns;
  return FindInReadable(video, aContentType);
}

bool IsValidVideoRegion(const gfx::IntSize& aFrame,
                        const gfx::IntRect& aPicture,
                        const gfx::IntSize& aDisplay) {
  return aFrame.width > 0 && aFrame.width <= PlanarYCbCrImage::MAX_DIMENSION &&
         aFrame.height > 0 &&
         aFrame.height <= PlanarYCbCrImage::MAX_DIMENSION &&
         aFrame.width * aFrame.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
         aPicture.width > 0 &&
         aPicture.width <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.x < PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.x + aPicture.width < PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.height > 0 &&
         aPicture.height <= PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.y < PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.y + aPicture.height < PlanarYCbCrImage::MAX_DIMENSION &&
         aPicture.width * aPicture.height <=
             MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT &&
         aDisplay.width > 0 &&
         aDisplay.width <= PlanarYCbCrImage::MAX_DIMENSION &&
         aDisplay.height > 0 &&
         aDisplay.height <= PlanarYCbCrImage::MAX_DIMENSION &&
         aDisplay.width * aDisplay.height <= MAX_VIDEO_WIDTH * MAX_VIDEO_HEIGHT;
}

already_AddRefed<SharedThreadPool> GetMediaThreadPool(MediaThreadType aType) {
  const char* name;
  uint32_t threads = 4;
  switch (aType) {
    case MediaThreadType::PLATFORM_DECODER:
      name = "MediaPDecoder";
      break;
    case MediaThreadType::WEBRTC_CALL_THREAD:
      name = "WebrtcCallThread";
      threads = 1;
      break;
    case MediaThreadType::WEBRTC_WORKER:
      name = "WebrtcWorker";
      break;
    case MediaThreadType::MDSM:
      name = "MediaDecoderStateMachine";
      threads = 1;
      break;
    case MediaThreadType::PLATFORM_ENCODER:
      name = "MediaPEncoder";
      break;
    default:
      MOZ_FALLTHROUGH_ASSERT("Unexpected MediaThreadType");
    case MediaThreadType::SUPERVISOR:
      name = "MediaSupervisor";
      break;
  }

  RefPtr<SharedThreadPool> pool =
      SharedThreadPool::Get(nsDependentCString(name), threads);

  // Ensure a larger stack for platform decoder threads
  if (aType == MediaThreadType::PLATFORM_DECODER) {
    const uint32_t minStackSize = 512 * 1024;
    uint32_t stackSize;
    MOZ_ALWAYS_SUCCEEDS(pool->GetThreadStackSize(&stackSize));
    if (stackSize < minStackSize) {
      MOZ_ALWAYS_SUCCEEDS(pool->SetThreadStackSize(minStackSize));
    }
  }

  return pool.forget();
}

bool ExtractVPXCodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                            uint8_t& aLevel, uint8_t& aBitDepth) {
  uint8_t dummyChromaSubsampling = 1;
  VideoColorSpace dummyColorspace;
  return ExtractVPXCodecDetails(aCodec, aProfile, aLevel, aBitDepth,
                                dummyChromaSubsampling, dummyColorspace);
}

bool ExtractVPXCodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                            uint8_t& aLevel, uint8_t& aBitDepth,
                            uint8_t& aChromaSubsampling,
                            VideoColorSpace& aColorSpace) {
  // Assign default value.
  aChromaSubsampling = 1;
  auto splitter = aCodec.Split(u'.');
  auto fieldsItr = splitter.begin();
  auto fourCC = *fieldsItr;

  if (!fourCC.EqualsLiteral("vp09") && !fourCC.EqualsLiteral("vp08")) {
    // Invalid 4CC
    return false;
  }
  ++fieldsItr;
  uint8_t primary, transfer, matrix, range;
  uint8_t* fields[] = {&aProfile, &aLevel,   &aBitDepth, &aChromaSubsampling,
                       &primary,  &transfer, &matrix,    &range};
  int fieldsCount = 0;
  nsresult rv;
  for (; fieldsItr != splitter.end(); ++fieldsItr, ++fieldsCount) {
    if (fieldsCount > 7) {
      // No more than 8 fields are expected.
      return false;
    }
    *(fields[fieldsCount]) =
        static_cast<uint8_t>((*fieldsItr).ToInteger(&rv, 10));
    // We got invalid field value, parsing error.
    NS_ENSURE_SUCCESS(rv, false);
  }
  // Mandatory Fields
  // <sample entry 4CC>.<profile>.<level>.<bitDepth>.
  // Optional Fields
  // <chromaSubsampling>.<colourPrimaries>.<transferCharacteristics>.
  // <matrixCoefficients>.<videoFullRangeFlag>
  // First three fields are mandatory(we have parsed 4CC).
  if (fieldsCount < 3) {
    // Invalid number of fields.
    return false;
  }
  // Start to validate the parsing value.

  // profile should be 0,1,2 or 3.
  // See https://www.webmproject.org/vp9/profiles/
  if (aProfile > 3) {
    // Invalid profile.
    return false;
  }

  // level, See https://www.webmproject.org/vp9/mp4/#semantics_1
  switch (aLevel) {
    case 10:
    case 11:
    case 20:
    case 21:
    case 30:
    case 31:
    case 40:
    case 41:
    case 50:
    case 51:
    case 52:
    case 60:
    case 61:
    case 62:
      break;
    default:
      // Invalid level.
      return false;
  }

  if (aBitDepth != 8 && aBitDepth != 10 && aBitDepth != 12) {
    // Invalid bitDepth:
    return false;
  }

  if (fieldsCount == 3) {
    // No more options.
    return true;
  }

  // chromaSubsampling should be 0,1,2,3...4~7 are reserved.
  if (aChromaSubsampling > 3) {
    return false;
  }

  if (fieldsCount == 4) {
    // No more options.
    return true;
  }

  // It is an integer that is defined by the "Colour primaries"
  // section of ISO/IEC 23001-8:2016 Table 2.
  // We treat reserved value as false case.
  if (primary == 0 || primary == 3 || primary > 22) {
    // reserved value.
    return false;
  }
  if (primary > 12 && primary < 22) {
    // 13~21 are reserved values.
    return false;
  }
  aColorSpace.mPrimaries = static_cast<ColourPrimaries>(primary);

  if (fieldsCount == 5) {
    // No more options.
    return true;
  }

  // It is an integer that is defined by the
  // "Transfer characteristics" section of ISO/IEC 23001-8:2016 Table 3.
  // We treat reserved value as false case.
  if (transfer == 0 || transfer == 3 || transfer > 18) {
    // reserved value.
    return false;
  }
  aColorSpace.mTransfer = static_cast<TransferCharacteristics>(transfer);

  if (fieldsCount == 6) {
    // No more options.
    return true;
  }

  // It is an integer that is defined by the
  // "Matrix coefficients" section of ISO/IEC 23001-8:2016 Table 4.
  // We treat reserved value as false case.
  if (matrix == 3 || matrix > 11) {
    return false;
  }
  aColorSpace.mMatrix = static_cast<MatrixCoefficients>(matrix);

  // If matrixCoefficients is 0 (RGB), then chroma subsampling MUST be 3
  // (4:4:4).
  if (aColorSpace.mMatrix == MatrixCoefficients::MC_IDENTITY &&
      aChromaSubsampling != 3) {
    return false;
  }

  if (fieldsCount == 7) {
    // No more options.
    return true;
  }

  // videoFullRangeFlag indicates the black level and range of the luma and
  // chroma signals. 0 = legal range (e.g. 16-235 for 8 bit sample depth);
  // 1 = full range (e.g. 0-255 for 8-bit sample depth).
  aColorSpace.mRange = static_cast<ColorRange>(range);
  return range <= 1;
}

bool ExtractH264CodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                             uint8_t& aConstraint, uint8_t& aLevel) {
  // H.264 codecs parameters have a type defined as avcN.PPCCLL, where
  // N = avc type. avc3 is avcc with SPS & PPS implicit (within stream)
  // PP = profile_idc, CC = constraint_set flags, LL = level_idc.
  // We ignore the constraint_set flags, as it's not clear from any
  // documentation what constraints the platform decoders support.
  // See
  // http://blog.pearce.org.nz/2013/11/what-does-h264avc1-codecs-parameters.html
  // for more details.
  if (aCodec.Length() != strlen("avc1.PPCCLL")) {
    return false;
  }

  // Verify the codec starts with "avc1." or "avc3.".
  const nsAString& sample = Substring(aCodec, 0, 5);
  if (!sample.EqualsASCII("avc1.") && !sample.EqualsASCII("avc3.")) {
    return false;
  }

  // Extract the profile_idc, constraint_flags and level_idc.
  nsresult rv = NS_OK;
  aProfile = Substring(aCodec, 5, 2).ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, false);

  // Constraint flags are stored on the 6 most significant bits, first two bits
  // are reserved_zero_2bits.
  aConstraint = Substring(aCodec, 7, 2).ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, false);

  aLevel = Substring(aCodec, 9, 2).ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, false);

  if (aLevel == 9) {
    aLevel = H264_LEVEL_1_b;
  } else if (aLevel <= 5) {
    aLevel *= 10;
  }

  return true;
}

bool ExtractAV1CodecDetails(const nsAString& aCodec, uint8_t& aProfile,
                            uint8_t& aLevel, uint8_t& aTier, uint8_t& aBitDepth,
                            bool& aMonochrome, bool& aSubsamplingX,
                            bool& aSubsamplingY, uint8_t& aChromaSamplePosition,
                            VideoColorSpace& aColorSpace) {
  auto fourCC = Substring(aCodec, 0, 4);

  if (!fourCC.EqualsLiteral("av01")) {
    // Invalid 4CC
    return false;
  }

  // Format is:
  // av01.N.NN[MH].NN.B.BBN.NN.NN.NN.B
  // where
  //   N = decimal digit
  //   [] = single character
  //   B = binary digit
  // Field order:
  // <sample entry 4CC>.<profile>.<level><tier>.<bitDepth>
  // [.<monochrome>.<chromaSubsampling>
  // .<colorPrimaries>.<transferCharacteristics>.<matrixCoefficients>
  // .<videoFullRangeFlag>]
  //
  // If any optional field is found, all the rest must be included.
  //
  // Parsing stops but does not fail upon encountering unexpected characters
  // at the end of an otherwise well-formed string.
  //
  // See https://aomediacodec.github.io/av1-isobmff/#codecsparam

  struct AV1Field {
    uint8_t* field;
    size_t length;
  };
  uint8_t monochrome;
  uint8_t subsampling;
  uint8_t primary;
  uint8_t transfer;
  uint8_t matrix;
  uint8_t range;
  AV1Field fields[] = {{&aProfile, 1},
                       {&aLevel, 2},
                       // parsing loop skips tier
                       {&aBitDepth, 2},
                       {&monochrome, 1},
                       {&subsampling, 3},
                       {&primary, 2},
                       {&transfer, 2},
                       {&matrix, 2},
                       {&range, 1}};

  auto splitter = aCodec.Split(u'.');
  auto iter = splitter.begin();
  ++iter;
  size_t fieldCount = 0;
  while (iter != splitter.end()) {
    // Exit if there are too many fields.
    if (fieldCount >= 9) {
      return false;
    }

    AV1Field& field = fields[fieldCount];
    auto fieldStr = *iter;

    if (field.field == &aLevel) {
      // Parse tier and remove it from the level field.
      if (fieldStr.Length() < 3) {
        return false;
      }
      auto tier = fieldStr[2];
      switch (tier) {
        case 'M':
          aTier = 0;
          break;
        case 'H':
          aTier = 1;
          break;
        default:
          return false;
      }
      fieldStr.SetLength(2);
    }

    if (fieldStr.Length() < field.length) {
      return false;
    }

    // Manually parse values since nsString.ToInteger silently stops parsing
    // upon encountering unknown characters.
    uint8_t value = 0;
    for (size_t i = 0; i < field.length; i++) {
      uint8_t oldValue = value;
      char16_t character = fieldStr[i];
      if ('0' <= character && character <= '9') {
        value = (value * 10) + (character - '0');
      } else {
        return false;
      }
      if (value < oldValue) {
        // Overflow is possible on the 3-digit subsampling field.
        return false;
      }
    }

    *field.field = value;

    ++fieldCount;
    ++iter;

    // Field had extra characters, exit early.
    if (fieldStr.Length() > field.length) {
      // Disallow numbers as unexpected characters.
      char16_t character = fieldStr[field.length];
      if ('0' <= character && character <= '9') {
        return false;
      }
      break;
    }
  }

  // Spec requires profile, level/tier, bitdepth, or for all possible fields to
  // be present.
  if (fieldCount != 3 && fieldCount != 9) {
    return false;
  }

  // Valid profiles are: Main (0), High (1), Professional (2).
  // Levels range from 0 to 23, or 31 to remove level restrictions.
  if (aProfile > 2 || (aLevel > 23 && aLevel != 31)) {
    return false;
  }

  if (fieldCount == 3) {
    // If only required fields are included, set to the spec defaults for the
    // rest and continue validating.
    aMonochrome = false;
    aSubsamplingX = true;
    aSubsamplingY = true;
    aChromaSamplePosition = 0;
    aColorSpace.mPrimaries = ColourPrimaries::CP_BT709;
    aColorSpace.mTransfer = TransferCharacteristics::TC_BT709;
    aColorSpace.mMatrix = MatrixCoefficients::MC_BT709;
    aColorSpace.mRange = ColorRange::LIMITED;
  } else {
    // Extract the individual values for the remaining fields, and check for
    // valid values for each.

    // Monochrome is a boolean.
    if (monochrome > 1) {
      return false;
    }
    aMonochrome = !!monochrome;

    // Extract individual digits of the subsampling field.
    // Subsampling is two binary digits for x and y
    // and one enumerated sample position field of
    // Unknown (0), Vertical (1), Colocated (2).
    uint8_t subsamplingX = (subsampling / 100) % 10;
    uint8_t subsamplingY = (subsampling / 10) % 10;
    if (subsamplingX > 1 || subsamplingY > 1) {
      return false;
    }
    aSubsamplingX = !!subsamplingX;
    aSubsamplingY = !!subsamplingY;
    aChromaSamplePosition = subsampling % 10;
    if (aChromaSamplePosition > 2) {
      return false;
    }

    // We can validate the color space values using CICP enums, as the values
    // are standardized in Rec. ITU-T H.273.
    aColorSpace.mPrimaries = static_cast<ColourPrimaries>(primary);
    aColorSpace.mTransfer = static_cast<TransferCharacteristics>(transfer);
    aColorSpace.mMatrix = static_cast<MatrixCoefficients>(matrix);
    if (gfx::CICP::IsReserved(aColorSpace.mPrimaries) ||
        gfx::CICP::IsReserved(aColorSpace.mTransfer) ||
        gfx::CICP::IsReserved(aColorSpace.mMatrix)) {
      return false;
    }
    // Range is a boolean, true meaning full and false meaning limited range.
    if (range > 1) {
      return false;
    }
    aColorSpace.mRange = static_cast<ColorRange>(range);
  }

  // Begin validating all parameter values:

  // Only Levels 8 and above (4.0 and greater) can specify Tier.
  // See: 5.5.1. General sequence header OBU syntax,
  // if ( seq_level_idx[ i ] > 7 ) seq_tier[ i ] = f(1)
  // https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=42
  // Also: Annex A, A.3. Levels, columns MainMbps and HighMbps
  // at https://aomediacodec.github.io/av1-spec/av1-spec.pdf#page=652
  if (aLevel < 8 && aTier > 0) {
    return false;
  }

  // Supported bit depths are 8, 10 and 12.
  if (aBitDepth != 8 && aBitDepth != 10 && aBitDepth != 12) {
    return false;
  }
  // Profiles 0 and 1 only support 8-bit and 10-bit.
  if (aProfile < 2 && aBitDepth == 12) {
    return false;
  }

  // x && y subsampling is used to specify monochrome 4:0:0 as well
  bool is420or400 = aSubsamplingX && aSubsamplingY;
  bool is422 = aSubsamplingX && !aSubsamplingY;
  bool is444 = !aSubsamplingX && !aSubsamplingY;

  // Profile 0 only supports 4:2:0.
  if (aProfile == 0 && !is420or400) {
    return false;
  }
  // Profile 1 only supports 4:4:4.
  if (aProfile == 1 && !is444) {
    return false;
  }
  // Profile 2 only allows 4:2:2 at 10 bits and below.
  if (aProfile == 2 && aBitDepth < 12 && !is422) {
    return false;
  }
  // Chroma sample position can only be specified with 4:2:0.
  if (aChromaSamplePosition != 0 && !is420or400) {
    return false;
  }

  // When video is monochrome, subsampling must be 4:0:0.
  if (aMonochrome && (aChromaSamplePosition != 0 || !is420or400)) {
    return false;
  }
  // Monochrome can only be signaled when profile is 0 or 2.
  // Note: This check is redundant with the above subsampling check,
  // as profile 1 only supports 4:4:4.
  if (aMonochrome && aProfile != 0 && aProfile != 2) {
    return false;
  }

  // Identity matrix requires 4:4:4 subsampling.
  if (aColorSpace.mMatrix == MatrixCoefficients::MC_IDENTITY &&
      (aSubsamplingX || aSubsamplingY ||
       aColorSpace.mRange != gfx::ColorRange::FULL)) {
    return false;
  }

  return true;
}

nsresult GenerateRandomName(nsCString& aOutSalt, uint32_t aLength) {
  nsresult rv;
  nsCOMPtr<nsIRandomGenerator> rg =
      do_GetService("@mozilla.org/security/random-generator;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // For each three bytes of random data we will get four bytes of ASCII.
  const uint32_t requiredBytesLength =
      static_cast<uint32_t>((aLength + 3) / 4 * 3);

  uint8_t* buffer;
  rv = rg->GenerateRandomBytes(requiredBytesLength, &buffer);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString temp;
  nsDependentCSubstring randomData(reinterpret_cast<const char*>(buffer),
                                   requiredBytesLength);
  rv = Base64Encode(randomData, temp);
  free(buffer);
  buffer = nullptr;
  if (NS_FAILED(rv)) {
    return rv;
  }

  aOutSalt = std::move(temp);
  return NS_OK;
}

nsresult GenerateRandomPathName(nsCString& aOutSalt, uint32_t aLength) {
  nsresult rv = GenerateRandomName(aOutSalt, aLength);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Base64 characters are alphanumeric (a-zA-Z0-9) and '+' and '/', so we need
  // to replace illegal characters -- notably '/'
  aOutSalt.ReplaceChar(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS, '_');
  return NS_OK;
}

already_AddRefed<TaskQueue> CreateMediaDecodeTaskQueue(const char* aName) {
  RefPtr<TaskQueue> queue = TaskQueue::Create(
      GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER), aName);
  return queue.forget();
}

void SimpleTimer::Cancel() {
  if (mTimer) {
#ifdef DEBUG
    nsCOMPtr<nsIEventTarget> target;
    mTimer->GetTarget(getter_AddRefs(target));
    bool onCurrent;
    nsresult rv = target->IsOnCurrentThread(&onCurrent);
    MOZ_ASSERT(NS_SUCCEEDED(rv) && onCurrent);
#endif
    mTimer->Cancel();
    mTimer = nullptr;
  }
  mTask = nullptr;
}

NS_IMETHODIMP
SimpleTimer::Notify(nsITimer* timer) {
  RefPtr<SimpleTimer> deathGrip(this);
  if (mTask) {
    mTask->Run();
    mTask = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
SimpleTimer::GetName(nsACString& aName) {
  aName.AssignLiteral("SimpleTimer");
  return NS_OK;
}

nsresult SimpleTimer::Init(nsIRunnable* aTask, uint32_t aTimeoutMs,
                           nsIEventTarget* aTarget) {
  nsresult rv;

  // Get target thread first, so we don't have to cancel the timer if it fails.
  nsCOMPtr<nsIEventTarget> target;
  if (aTarget) {
    target = aTarget;
  } else {
    target = GetMainThreadEventTarget();
    if (!target) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  rv = NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, aTimeoutMs,
                               nsITimer::TYPE_ONE_SHOT, target);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mTask = aTask;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(SimpleTimer, nsITimerCallback, nsINamed)

already_AddRefed<SimpleTimer> SimpleTimer::Create(nsIRunnable* aTask,
                                                  uint32_t aTimeoutMs,
                                                  nsIEventTarget* aTarget) {
  RefPtr<SimpleTimer> t(new SimpleTimer());
  if (NS_FAILED(t->Init(aTask, aTimeoutMs, aTarget))) {
    return nullptr;
  }
  return t.forget();
}

void LogToBrowserConsole(const nsAString& aMsg) {
  if (!NS_IsMainThread()) {
    nsString msg(aMsg);
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
        "LogToBrowserConsole", [msg]() { LogToBrowserConsole(msg); });
    SchedulerGroup::Dispatch(TaskCategory::Other, task.forget());
    return;
  }
  nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }
  nsAutoString msg(aMsg);
  console->LogStringMessage(msg.get());
}

bool ParseCodecsString(const nsAString& aCodecs,
                       nsTArray<nsString>& aOutCodecs) {
  aOutCodecs.Clear();
  bool expectMoreTokens = false;
  nsCharSeparatedTokenizer tokenizer(aCodecs, ',');
  while (tokenizer.hasMoreTokens()) {
    const nsAString& token = tokenizer.nextToken();
    expectMoreTokens = tokenizer.separatorAfterCurrentToken();
    aOutCodecs.AppendElement(token);
  }
  if (expectMoreTokens) {
    // Last codec name was empty
    return false;
  }
  return true;
}

bool ParseMIMETypeString(const nsAString& aMIMEType,
                         nsString& aOutContainerType,
                         nsTArray<nsString>& aOutCodecs) {
  nsContentTypeParser parser(aMIMEType);
  nsresult rv = parser.GetType(aOutContainerType);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsString codecsStr;
  parser.GetParameter("codecs", codecsStr);
  return ParseCodecsString(codecsStr, aOutCodecs);
}

template <int N>
static bool StartsWith(const nsACString& string, const char (&prefix)[N]) {
  if (N - 1 > string.Length()) {
    return false;
  }
  return memcmp(string.Data(), prefix, N - 1) == 0;
}

bool IsH264CodecString(const nsAString& aCodec) {
  uint8_t profile = 0;
  uint8_t constraint = 0;
  uint8_t level = 0;
  return ExtractH264CodecDetails(aCodec, profile, constraint, level);
}

bool IsAACCodecString(const nsAString& aCodec) {
  return aCodec.EqualsLiteral("mp4a.40.2") ||  // MPEG4 AAC-LC
         aCodec.EqualsLiteral(
             "mp4a.40.02") ||  // MPEG4 AAC-LC(for compatibility)
         aCodec.EqualsLiteral("mp4a.40.5") ||  // MPEG4 HE-AAC
         aCodec.EqualsLiteral(
             "mp4a.40.05") ||                 // MPEG4 HE-AAC(for compatibility)
         aCodec.EqualsLiteral("mp4a.67") ||   // MPEG2 AAC-LC
         aCodec.EqualsLiteral("mp4a.40.29");  // MPEG4 HE-AACv2
}

bool IsVP8CodecString(const nsAString& aCodec) {
  uint8_t profile = 0;
  uint8_t level = 0;
  uint8_t bitDepth = 0;
  return aCodec.EqualsLiteral("vp8") || aCodec.EqualsLiteral("vp8.0") ||
         (StartsWith(NS_ConvertUTF16toUTF8(aCodec), "vp08") &&
          ExtractVPXCodecDetails(aCodec, profile, level, bitDepth));
}

bool IsVP9CodecString(const nsAString& aCodec) {
  uint8_t profile = 0;
  uint8_t level = 0;
  uint8_t bitDepth = 0;
  return aCodec.EqualsLiteral("vp9") || aCodec.EqualsLiteral("vp9.0") ||
         (StartsWith(NS_ConvertUTF16toUTF8(aCodec), "vp09") &&
          ExtractVPXCodecDetails(aCodec, profile, level, bitDepth));
}

bool IsAV1CodecString(const nsAString& aCodec) {
  uint8_t profile, level, tier, bitDepth, chromaPosition;
  bool monochrome, subsamplingX, subsamplingY;
  VideoColorSpace colorSpace;
  return aCodec.EqualsLiteral("av1") ||
         (StartsWith(NS_ConvertUTF16toUTF8(aCodec), "av01") &&
          ExtractAV1CodecDetails(aCodec, profile, level, tier, bitDepth,
                                 monochrome, subsamplingX, subsamplingY,
                                 chromaPosition, colorSpace));
}

UniquePtr<TrackInfo> CreateTrackInfoWithMIMEType(
    const nsACString& aCodecMIMEType) {
  UniquePtr<TrackInfo> trackInfo;
  if (StartsWith(aCodecMIMEType, "audio/")) {
    trackInfo.reset(new AudioInfo());
    trackInfo->mMimeType = aCodecMIMEType;
  } else if (StartsWith(aCodecMIMEType, "video/")) {
    trackInfo.reset(new VideoInfo());
    trackInfo->mMimeType = aCodecMIMEType;
  }
  return trackInfo;
}

UniquePtr<TrackInfo> CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
    const nsACString& aCodecMIMEType,
    const MediaContainerType& aContainerType) {
  UniquePtr<TrackInfo> trackInfo = CreateTrackInfoWithMIMEType(aCodecMIMEType);
  if (trackInfo) {
    VideoInfo* videoInfo = trackInfo->GetAsVideoInfo();
    if (videoInfo) {
      Maybe<int32_t> maybeWidth = aContainerType.ExtendedType().GetWidth();
      if (maybeWidth && *maybeWidth > 0) {
        videoInfo->mImage.width = *maybeWidth;
        videoInfo->mDisplay.width = *maybeWidth;
      }
      Maybe<int32_t> maybeHeight = aContainerType.ExtendedType().GetHeight();
      if (maybeHeight && *maybeHeight > 0) {
        videoInfo->mImage.height = *maybeHeight;
        videoInfo->mDisplay.height = *maybeHeight;
      }
    } else if (trackInfo->GetAsAudioInfo()) {
      AudioInfo* audioInfo = trackInfo->GetAsAudioInfo();
      Maybe<int32_t> maybeChannels =
          aContainerType.ExtendedType().GetChannels();
      if (maybeChannels && *maybeChannels > 0) {
        audioInfo->mChannels = *maybeChannels;
      }
      Maybe<int32_t> maybeSamplerate =
          aContainerType.ExtendedType().GetSamplerate();
      if (maybeSamplerate && *maybeSamplerate > 0) {
        audioInfo->mRate = *maybeSamplerate;
      }
    }
  }
  return trackInfo;
}

bool OnCellularConnection() {
  uint32_t linkType = nsINetworkLinkService::LINK_TYPE_UNKNOWN;
  if (XRE_IsContentProcess()) {
    mozilla::dom::ContentChild* cpc =
        mozilla::dom::ContentChild::GetSingleton();
    if (!cpc) {
      NS_WARNING("Can't get ContentChild singleton in content process!");
      return false;
    }
    linkType = cpc->NetworkLinkType();
  } else {
    nsresult rv;
    nsCOMPtr<nsINetworkLinkService> nls =
        do_GetService(NS_NETWORK_LINK_SERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("Can't get nsINetworkLinkService.");
      return false;
    }

    rv = nls->GetLinkType(&linkType);
    if (NS_FAILED(rv)) {
      NS_WARNING("Can't get network link type.");
      return false;
    }
  }

  switch (linkType) {
    case nsINetworkLinkService::LINK_TYPE_UNKNOWN:
    case nsINetworkLinkService::LINK_TYPE_ETHERNET:
    case nsINetworkLinkService::LINK_TYPE_USB:
    case nsINetworkLinkService::LINK_TYPE_WIFI:
      return false;
    case nsINetworkLinkService::LINK_TYPE_WIMAX:
    case nsINetworkLinkService::LINK_TYPE_MOBILE:
      return true;
  }

  return false;
}

}  // end namespace mozilla
