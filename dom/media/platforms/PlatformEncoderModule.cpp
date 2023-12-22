/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformEncoderModule.h"
#include "nsPrintfCString.h"
#include "mozilla/ToString.h"

namespace mozilla {

extern LazyLogModule sPEMLog;
#define LOGD(fmt, ...)                       \
  MOZ_LOG(sPEMLog, mozilla::LogLevel::Debug, \
          ("PEM: %s: " fmt, __func__, ##__VA_ARGS__))

// TODO: Automatically generate this (Bug 1865896)
const char* GetCodecTypeString(const CodecType& aCodecType) {
  switch (aCodecType) {
    case CodecType::_BeginVideo_:
      return "_BeginVideo_";
    case CodecType::H264:
      return "H264";
    case CodecType::VP8:
      return "VP8";
    case CodecType::VP9:
      return "VP9";
    case CodecType::AV1:
      return "AV1";
    case CodecType::_EndVideo_:  // CodecType::_BeginAudio_
      return "_EndVideo_/_BeginAudio_";
    case CodecType::Opus:
      return "Opus";
    case CodecType::G722:
      return "G722";
    case CodecType::_EndAudio_:
      return "_EndAudio_";
    case CodecType::Unknown:
      return "Unknown";
  }
  MOZ_ASSERT_UNREACHABLE("undefined codec type");
  return "Undefined";
}

RefPtr<PlatformEncoderModule::CreateEncoderPromise>
PlatformEncoderModule::AsyncCreateEncoder(const EncoderConfig& aEncoderConfig,
                                          const RefPtr<TaskQueue>& aTaskQueue) {
  RefPtr<MediaDataEncoder> encoder;
  MediaResult result = NS_OK;
  if (aEncoderConfig.IsAudio()) {
    encoder = CreateAudioEncoder(aEncoderConfig, aTaskQueue);
  } else if (aEncoderConfig.IsVideo()) {
    encoder = CreateVideoEncoder(aEncoderConfig, aTaskQueue);
  }
  if (!encoder) {
    if (NS_FAILED(result)) {
      return CreateEncoderPromise::CreateAndReject(result, __func__);
    }
    return CreateEncoderPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    nsPrintfCString("Error creating encoder for %d",
                                    static_cast<int>(aEncoderConfig.mCodec))
                        .get()),
        __func__);
  }
  return CreateEncoderPromise::CreateAndResolve(encoder, __func__);
}

template <typename T>
nsCString MaybeToString(const Maybe<T>& aMaybe) {
  return nsPrintfCString(
      "%s", aMaybe.isSome() ? ToString(aMaybe.value()).c_str() : "nothing");
}

struct ConfigurationChangeToString {
  nsCString operator()(const DimensionsChange& aDimensionsChange) {
    return nsPrintfCString("Dimensions: %dx%d", aDimensionsChange.get().width,
                           aDimensionsChange.get().height);
  }
  nsCString operator()(const DisplayDimensionsChange& aDisplayDimensionChange) {
    if (aDisplayDimensionChange.get().isNothing()) {
      return nsCString("Display dimensions: nothing");
    }
    gfx::IntSize displayDimensions = aDisplayDimensionChange.get().value();
    return nsPrintfCString("Display dimensions: %dx%d", displayDimensions.width,
                           displayDimensions.height);
  }
  nsCString operator()(const BitrateChange& aBitrateChange) {
    if (aBitrateChange.get().isSome()) {
      return nsLiteralCString("Bitrate: nothing");
    }
    return nsPrintfCString("Bitrate: %skbps",
                           MaybeToString(aBitrateChange.get()).get());
  }
  nsCString operator()(const FramerateChange& aFramerateChange) {
    if (aFramerateChange.get().isNothing()) {
      return nsCString("Framerate: nothing");
    }
    return nsPrintfCString("Framerate: %lfHz", aFramerateChange.get().value());
  }
  nsCString operator()(const BitrateModeChange& aBitrateModeChange) {
    return nsPrintfCString(
        "Bitrate mode: %s",
        aBitrateModeChange.get() == MediaDataEncoder::BitrateMode::Constant
            ? "Constant"
            : "Variable");
  }
  nsCString operator()(const UsageChange& aUsageChange) {
    return nsPrintfCString(
        "Usage mode: %s",
        aUsageChange.get() == MediaDataEncoder::Usage::Realtime ? "Realtime"
                                                                : "Recoding");
  }
  nsCString operator()(const ContentHintChange& aContentHintChange) {
    return nsPrintfCString("Content hint: %s",
                           MaybeToString(aContentHintChange.get()).get());
  }
};

nsString EncoderConfigurationChangeList::ToString() const {
  nsString rv(
      NS_LITERAL_STRING_FROM_CSTRING("EncoderConfigurationChangeList:"_ns));
  for (const EncoderConfigurationItem& change : mChanges) {
    nsCString str = change.match(ConfigurationChangeToString());
    rv.AppendPrintf("- %s\n", str.get());
  }
  return rv;
}

bool CanLikelyEncode(const EncoderConfig& aConfig) {
  if (aConfig.mCodec == CodecType::H264) {
    if (!aConfig.mCodecSpecific ||
        !aConfig.mCodecSpecific->is<H264Specific>()) {
      LOGD("Error: asking for support codec for h264 without h264 specific config.");
      return false;
    }
    H264Specific specific = aConfig.mCodecSpecific->as<H264Specific>();
    int width = aConfig.mSize.width;
    int height = aConfig.mSize.height;
    if (width % 2 || !width) {
      LOGD("Invalid width of %d for h264", width);
      return false;
    }
    if (height % 2 || !height) {
      LOGD("Invalid height of %d for h264", height);
      return false;
    }
    if (specific.mProfile != H264_PROFILE_BASE &&
        specific.mProfile != H264_PROFILE_MAIN &&
        specific.mProfile != H264_PROFILE_HIGH) {
      LOGD("Invalid profile of %x for h264", specific.mProfile);
      return false;
    }
    if (width > 4096 || height > 4096) {
      LOGD("Invalid size of %dx%d for h264", width, height);
      return false;
    }
  }
  if (aConfig.mCodec == CodecType::VP8) {
    int width = aConfig.mSize.width;
    int height = aConfig.mSize.height;
    if (width > 2 << 13 || height > 2 << 13) {
      LOGD("Invalid size of %dx%d for VP8", width, height);
      return false;
    }
  }
  if (aConfig.mCodec == CodecType::VP9) {
    int width = aConfig.mSize.width;
    int height = aConfig.mSize.height;
    if (width > 2 << 15 || height > 2 << 15) {
      LOGD("Invalid size of %dx%d for VP9", width, height);
      return false;
    }
  }

  return true;
}

}  // namespace mozilla

#undef LOGD
