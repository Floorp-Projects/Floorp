/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxPlatformLayer.h"

#if defined(MOZ_WIDGET_GONK) && (ANDROID_VERSION == 20 || ANDROID_VERSION == 19)
#define OMX_PLATFORM_GONK
#include "GonkOmxPlatformLayer.h"
#endif

extern mozilla::LogModule* GetPDMLog();

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(GetPDMLog(), mozilla::LogLevel::Debug, ("OmxPlatformLayer -- %s: " arg, __func__, ##__VA_ARGS__))

#define RETURN_IF_ERR(err)     \
  if (err != OMX_ErrorNone) {  \
    LOG("error: 0x%08x", err); \
    return err;                \
  }                            \

// Common OMX decoder configuration code.
namespace mozilla {

// This helper class encapsulates the details of component parameters setting
// for different OMX audio & video codecs.
template<typename ParamType>
class OmxConfig
{
public:
  virtual ~OmxConfig() {}
  // Subclasses should implement this method to configure the codec.
  virtual OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const ParamType& aParam) = 0;
};

typedef OmxConfig<AudioInfo> OmxAudioConfig;
typedef OmxConfig<VideoInfo> OmxVideoConfig;

template<typename ConfigType>
UniquePtr<ConfigType> ConfigForMime(const nsACString&);

class OmxAacConfig : public OmxAudioConfig
{
public:
  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const AudioInfo& aInfo) override
  {
    OMX_ERRORTYPE err;

    OMX_AUDIO_PARAM_AACPROFILETYPE aacProfile;
    InitOmxParameter(&aacProfile);
    err = aOmx.GetParameter(OMX_IndexParamAudioAac, &aacProfile, sizeof(aacProfile));
    RETURN_IF_ERR(err);

    aacProfile.nChannels = aInfo.mChannels;
    aacProfile.nSampleRate = aInfo.mRate;
    aacProfile.eAACProfile = static_cast<OMX_AUDIO_AACPROFILETYPE>(aInfo.mProfile);
    err = aOmx.SetParameter(OMX_IndexParamAudioAac, &aacProfile, sizeof(aacProfile));
    RETURN_IF_ERR(err);

    LOG("Config OMX_IndexParamAudioAac, channel %d, sample rate %d, profile %d",
        aacProfile.nChannels, aacProfile.nSampleRate, aacProfile.eAACProfile);

    return OMX_ErrorNone;
  }
};

template<>
UniquePtr<OmxAudioConfig>
ConfigForMime(const nsACString& aMimeType)
{
  UniquePtr<OmxAudioConfig> conf;

  if (OmxPlatformLayer::SupportsMimeType(aMimeType)) {
    if (aMimeType.EqualsLiteral("audio/mp4a-latm")) {
      conf.reset(new OmxAacConfig());
    }
  }
  return Move(conf);
}

// There should be a better way to calculate it.
#define MIN_VIDEO_INPUT_BUFFER_SIZE 64 * 1024

class OmxCommonVideoConfig : public OmxVideoConfig
{
public:
  explicit OmxCommonVideoConfig(OMX_VIDEO_CODINGTYPE aCodec)
    : OmxVideoConfig()
    , mCodec(aCodec)
  {}

  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const VideoInfo& aInfo) override
  {
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE def;

    // Set up in/out port definition.
    nsTArray<uint32_t> ports;
    GetOmxPortIndex(ports);
    for (auto idx : ports) {
      InitOmxParameter(&def);
      def.nPortIndex = idx;
      err = aOmx.GetParameter(OMX_IndexParamPortDefinition, &def, sizeof(def));
      RETURN_IF_ERR(err);

      def.format.video.nFrameWidth =  aInfo.mDisplay.width;
      def.format.video.nFrameHeight = aInfo.mDisplay.height;
      def.format.video.nStride = aInfo.mImage.width;
      def.format.video.nSliceHeight = aInfo.mImage.height;

      if (def.eDir == OMX_DirInput) {
        def.format.video.eCompressionFormat = mCodec;
        def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
        if (def.nBufferSize < MIN_VIDEO_INPUT_BUFFER_SIZE) {
          def.nBufferSize = aInfo.mImage.width * aInfo.mImage.height;
          LOG("Change input buffer size to %d", def.nBufferSize);
        }
      } else {
        def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
      }

      err = aOmx.SetParameter(OMX_IndexParamPortDefinition, &def, sizeof(def));
    }
    return err;
  }

private:
  const OMX_VIDEO_CODINGTYPE mCodec;
};

template<>
UniquePtr<OmxVideoConfig>
ConfigForMime(const nsACString& aMimeType)
{
  UniquePtr<OmxVideoConfig> conf;

  if (OmxPlatformLayer::SupportsMimeType(aMimeType)) {
    if (aMimeType.EqualsLiteral("video/avc")) {
      conf.reset(new OmxCommonVideoConfig(OMX_VIDEO_CodingAVC));
    } else if (aMimeType.EqualsLiteral("video/mp4v-es") ||
         aMimeType.EqualsLiteral("video/mp4")) {
      conf.reset(new OmxCommonVideoConfig(OMX_VIDEO_CodingMPEG4));
    } else if (aMimeType.EqualsLiteral("video/3gpp")) {
      conf.reset(new OmxCommonVideoConfig(OMX_VIDEO_CodingH263));
    }
  }
  return Move(conf);
}

OMX_ERRORTYPE
OmxPlatformLayer::Config()
{
  MOZ_ASSERT(mInfo);

  if (mInfo->IsAudio()) {
    UniquePtr<OmxAudioConfig> conf(ConfigForMime<OmxAudioConfig>(mInfo->mMimeType));
    MOZ_ASSERT(conf.get());
    return conf->Apply(*this, *(mInfo->GetAsAudioInfo()));
  } else if (mInfo->IsVideo()) {
    UniquePtr<OmxVideoConfig> conf(ConfigForMime<OmxVideoConfig>(mInfo->mMimeType));
    MOZ_ASSERT(conf.get());
    return conf->Apply(*this, *(mInfo->GetAsVideoInfo()));
  } else {
    MOZ_ASSERT_UNREACHABLE("non-AV data (text?) is not supported.");
    return OMX_ErrorNotImplemented;
  }
}

// Implementations for different platforms will be defined in their own files.
#ifdef OMX_PLATFORM_GONK

bool
OmxPlatformLayer::SupportsMimeType(const nsACString& aMimeType)
{
  return GonkOmxPlatformLayer::FindComponents(aMimeType);
}

OmxPlatformLayer*
OmxPlatformLayer::Create(OmxDataDecoder* aDataDecoder,
                         OmxPromiseLayer* aPromiseLayer,
                         TaskQueue* aTaskQueue,
                         layers::ImageContainer* aImageContainer)
{
  return new GonkOmxPlatformLayer(aDataDecoder, aPromiseLayer, aTaskQueue, aImageContainer);
}

#else // For platforms without OMX IL support.

bool
OmxPlatformLayer::SupportsMimeType(const nsACString& aMimeType)
{
  return false;
}

OmxPlatformLayer*
OmxPlatformLayer::Create(OmxDataDecoder* aDataDecoder,
                        OmxPromiseLayer* aPromiseLayer,
                        TaskQueue* aTaskQueue,
                        layers::ImageContainer* aImageContainer)
{
  return nullptr;
}

#endif

}
