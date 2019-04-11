/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaMIMETypes.h"

#include "nsContentTypeParser.h"
#include "mozilla/dom/MediaCapabilitiesBinding.h"

namespace mozilla {

template <int N>
static bool StartsWith(const nsACString& string, const char (&prefix)[N]) {
  if (N - 1 > string.Length()) {
    return false;
  }
  return memcmp(string.Data(), prefix, N - 1) == 0;
}

bool MediaMIMEType::HasApplicationMajorType() const {
  return StartsWith(mMIMEType, "application/");
}

bool MediaMIMEType::HasAudioMajorType() const {
  return StartsWith(mMIMEType, "audio/");
}

bool MediaMIMEType::HasVideoMajorType() const {
  return StartsWith(mMIMEType, "video/");
}

size_t MediaMIMEType::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mMIMEType.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

MediaMIMEType::MediaMIMEType(const nsACString& aType) : mMIMEType(aType) {}

Maybe<MediaMIMEType> MakeMediaMIMEType(const nsAString& aType) {
  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  return Some(MediaMIMEType(mime8));
}

Maybe<MediaMIMEType> MakeMediaMIMEType(const nsACString& aType) {
  return MakeMediaMIMEType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaMIMEType> MakeMediaMIMEType(const char* aType) {
  if (!aType) {
    return Nothing();
  }
  return MakeMediaMIMEType(nsDependentCString(aType));
}

bool MediaCodecs::Contains(const nsAString& aCodec) const {
  for (const auto& myCodec : Range()) {
    if (myCodec == aCodec) {
      return true;
    }
  }
  return false;
}

bool MediaCodecs::ContainsAll(const MediaCodecs& aCodecs) const {
  const auto& codecsToTest = aCodecs.Range();
  for (const auto& codecToTest : codecsToTest) {
    if (!Contains(codecToTest)) {
      return false;
    }
  }
  return true;
}

bool MediaCodecs::ContainsPrefix(const nsAString& aCodecPrefix) const {
  const size_t prefixLength = aCodecPrefix.Length();
  for (const auto& myCodec : Range()) {
    if (myCodec.Length() >= prefixLength &&
        memcmp(myCodec.Data(), aCodecPrefix.Data(), prefixLength) == 0) {
      return true;
    }
  }
  return false;
}

size_t MediaCodecs::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  return mCodecs.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
}

static int32_t GetParameterAsNumber(const nsContentTypeParser& aParser,
                                    const char* aParameter,
                                    const int32_t aErrorReturn) {
  nsAutoString parameterString;
  nsresult rv = aParser.GetParameter(aParameter, parameterString);
  if (NS_FAILED_impl(rv)) {
    return aErrorReturn;
  }
  int32_t number = parameterString.ToInteger(&rv);
  if (MOZ_UNLIKELY(NS_FAILED_impl(rv))) {
    return aErrorReturn;
  }
  return number;
}

MediaExtendedMIMEType::MediaExtendedMIMEType(
    const nsACString& aOriginalString, const nsACString& aMIMEType,
    bool aHaveCodecs, const nsAString& aCodecs, int32_t aWidth, int32_t aHeight,
    double aFramerate, int32_t aBitrate, EOTF aEOTF, int32_t aChannels)
    : mOriginalString(aOriginalString),
      mMIMEType(aMIMEType),
      mHaveCodecs(aHaveCodecs),
      mCodecs(aCodecs),
      mWidth(aWidth),
      mHeight(aHeight),
      mFramerate(aFramerate),
      mEOTF(aEOTF),
      mChannels(aChannels),
      mBitrate(aBitrate) {}

MediaExtendedMIMEType::MediaExtendedMIMEType(
    const nsACString& aOriginalString, const nsACString& aMIMEType,
    bool aHaveCodecs, const nsAString& aCodecs, int32_t aChannels,
    int32_t aSamplerate, int32_t aBitrate)
    : mOriginalString(aOriginalString),
      mMIMEType(aMIMEType),
      mHaveCodecs(aHaveCodecs),
      mCodecs(aCodecs),
      mChannels(aChannels),
      mSamplerate(aSamplerate),
      mBitrate(aBitrate) {}

MediaExtendedMIMEType::MediaExtendedMIMEType(const MediaMIMEType& aType)
    : mOriginalString(aType.AsString()), mMIMEType(aType) {}

MediaExtendedMIMEType::MediaExtendedMIMEType(MediaMIMEType&& aType)
    : mOriginalString(aType.AsString()), mMIMEType(std::move(aType)) {}

/* static */
Maybe<double> MediaExtendedMIMEType::ComputeFractionalString(
    const nsAString& aFrac) {
  nsAutoString frac(aFrac);
  nsresult error;
  double result = frac.ToDouble(&error);
  if (NS_SUCCEEDED(error)) {
    if (result <= 0) {
      return Nothing();
    }
    return Some(result);
  }

  int32_t slashPos = frac.Find(NS_LITERAL_STRING("/"));
  if (slashPos == kNotFound) {
    return Nothing();
  }

  nsAutoString firstPart(Substring(frac, 0, slashPos - 1));
  double first = firstPart.ToDouble(&error);
  if (NS_FAILED(error)) {
    return Nothing();
  }
  nsAutoString secondPart(Substring(frac, slashPos + 1));
  double second = secondPart.ToDouble(&error);
  if (NS_FAILED(error) || second == 0) {
    return Nothing();
  }

  result = first / second;
  if (result <= 0) {
    return Nothing();
  }

  return Some(result);
}

static EOTF GetParameterAsEOTF(const nsContentTypeParser& aParser) {
  nsAutoString eotf;
  nsresult rv = aParser.GetParameter("eotf", eotf);
  if (NS_FAILED_impl(rv)) {
    return EOTF::UNSPECIFIED;
  }
  if (eotf.LowerCaseEqualsASCII("bt709")) {
    return EOTF::BT709;
  }
  return EOTF::NOT_SUPPORTED;
}

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const nsAString& aType) {
  nsContentTypeParser parser(aType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  bool haveCodecs = NS_SUCCEEDED(rv);

  int32_t width = GetParameterAsNumber(parser, "width", -1);
  int32_t height = GetParameterAsNumber(parser, "height", -1);
  double framerate = GetParameterAsNumber(parser, "framerate", -1);
  int32_t bitrate = GetParameterAsNumber(parser, "bitrate", -1);
  EOTF eotf = GetParameterAsEOTF(parser);
  int32_t channels = GetParameterAsNumber(parser, "channels", -1);

  return Some(MediaExtendedMIMEType(NS_ConvertUTF16toUTF8(aType), mime8,
                                    haveCodecs, codecs, width, height,
                                    framerate, bitrate, eotf, channels));
}

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(
    const dom::VideoConfiguration& aConfig) {
  if (aConfig.mContentType.IsEmpty()) {
    return Nothing();
  }
  nsContentTypeParser parser(aConfig.mContentType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  bool haveCodecs = NS_SUCCEEDED(rv);

  auto framerate =
      MediaExtendedMIMEType::ComputeFractionalString(aConfig.mFramerate);
  if (!framerate) {
    return Nothing();
  }

  return Some(MediaExtendedMIMEType(NS_ConvertUTF16toUTF8(aConfig.mContentType),
                                    mime8, haveCodecs, codecs, aConfig.mWidth,
                                    aConfig.mHeight, framerate.ref(),
                                    aConfig.mBitrate, EOTF::UNSPECIFIED));
}

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(
    const dom::AudioConfiguration& aConfig) {
  if (aConfig.mContentType.IsEmpty()) {
    return Nothing();
  }
  nsContentTypeParser parser(aConfig.mContentType);
  nsAutoString mime;
  nsresult rv = parser.GetType(mime);
  if (!NS_SUCCEEDED(rv) || mime.IsEmpty()) {
    return Nothing();
  }

  NS_ConvertUTF16toUTF8 mime8{mime};
  if (!IsMediaMIMEType(mime8)) {
    return Nothing();
  }

  nsAutoString codecs;
  rv = parser.GetParameter("codecs", codecs);
  bool haveCodecs = NS_SUCCEEDED(rv);

  int32_t channels = 2;  // use a stereo config if not known.
  if (aConfig.mChannels.WasPassed()) {
    // A channels string was passed. Make sure it is valid.
    nsresult error;
    double value = aConfig.mChannels.Value().ToDouble(&error);
    if (NS_FAILED(error)) {
      return Nothing();
    }
    // Value is a channel configuration such as 5.1. We want to treat this as 6.
    channels = value;
    double fp = value - channels;
    // round up as .1 and .2 aren't exactly expressible in binary.
    channels += (fp * 10) + .5;
  }

  return Some(MediaExtendedMIMEType(
      NS_ConvertUTF16toUTF8(aConfig.mContentType), mime8, haveCodecs, codecs,
      channels,
      aConfig.mSamplerate.WasPassed() ? aConfig.mSamplerate.Value() : 48000,
      aConfig.mBitrate.WasPassed() ? aConfig.mBitrate.Value() : 131072));
}

size_t MediaExtendedMIMEType::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return mOriginalString.SizeOfExcludingThisIfUnshared(aMallocSizeOf) +
         mMIMEType.SizeOfExcludingThis(aMallocSizeOf) +
         mCodecs.SizeOfExcludingThis(aMallocSizeOf);
}

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(
    const nsACString& aType) {
  return MakeMediaExtendedMIMEType(NS_ConvertUTF8toUTF16(aType));
}

Maybe<MediaExtendedMIMEType> MakeMediaExtendedMIMEType(const char* aType) {
  if (!aType) {
    return Nothing();
  }
  return MakeMediaExtendedMIMEType(nsDependentCString(aType));
}

}  // namespace mozilla
