/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxPlatformLayer.h"

#include "OMX_VideoExt.h" // For VP8.

#include "VPXDecoder.h"

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("OmxPlatformLayer -- %s: " arg, __func__, ##__VA_ARGS__))

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

static OMX_ERRORTYPE
ConfigAudioOutputPort(OmxPlatformLayer& aOmx, const AudioInfo& aInfo)
{
  OMX_ERRORTYPE err;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxParameter(&def);
  def.nPortIndex = aOmx.OutputPortIndex();
  err = aOmx.GetParameter(OMX_IndexParamPortDefinition, &def, sizeof(def));
  RETURN_IF_ERR(err);

  def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  err = aOmx.SetParameter(OMX_IndexParamPortDefinition, &def, sizeof(def));
  RETURN_IF_ERR(err);

  OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
  InitOmxParameter(&pcmParams);
  pcmParams.nPortIndex = def.nPortIndex;
  err = aOmx.GetParameter(OMX_IndexParamAudioPcm, &pcmParams, sizeof(pcmParams));
  RETURN_IF_ERR(err);

  pcmParams.nChannels = aInfo.mChannels;
  pcmParams.eNumData = OMX_NumericalDataSigned;
  pcmParams.bInterleaved = OMX_TRUE;
  pcmParams.nBitPerSample = 16;
  pcmParams.nSamplingRate = aInfo.mRate;
  pcmParams.ePCMMode = OMX_AUDIO_PCMModeLinear;
  err = aOmx.SetParameter(OMX_IndexParamAudioPcm, &pcmParams, sizeof(pcmParams));
  RETURN_IF_ERR(err);

  LOG("Config OMX_IndexParamAudioPcm, channel %lu, sample rate %lu",
      pcmParams.nChannels, pcmParams.nSamplingRate);

  return OMX_ErrorNone;
}

class OmxAacConfig : public OmxAudioConfig
{
public:
  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const AudioInfo& aInfo) override
  {
    OMX_ERRORTYPE err;

    OMX_AUDIO_PARAM_AACPROFILETYPE aacProfile;
    InitOmxParameter(&aacProfile);
    aacProfile.nPortIndex = aOmx.InputPortIndex();
    err = aOmx.GetParameter(OMX_IndexParamAudioAac, &aacProfile, sizeof(aacProfile));
    RETURN_IF_ERR(err);

    aacProfile.nChannels = aInfo.mChannels;
    aacProfile.nSampleRate = aInfo.mRate;
    aacProfile.eAACProfile = static_cast<OMX_AUDIO_AACPROFILETYPE>(aInfo.mProfile);
    err = aOmx.SetParameter(OMX_IndexParamAudioAac, &aacProfile, sizeof(aacProfile));
    RETURN_IF_ERR(err);

    LOG("Config OMX_IndexParamAudioAac, channel %lu, sample rate %lu, profile %d",
        aacProfile.nChannels, aacProfile.nSampleRate, aacProfile.eAACProfile);

    return ConfigAudioOutputPort(aOmx, aInfo);
  }
};

class OmxMp3Config : public OmxAudioConfig
{
public:
  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const AudioInfo& aInfo) override
  {
    OMX_ERRORTYPE err;

    OMX_AUDIO_PARAM_MP3TYPE mp3Param;
    InitOmxParameter(&mp3Param);
    mp3Param.nPortIndex = aOmx.InputPortIndex();
    err = aOmx.GetParameter(OMX_IndexParamAudioMp3, &mp3Param, sizeof(mp3Param));
    RETURN_IF_ERR(err);

    mp3Param.nChannels = aInfo.mChannels;
    mp3Param.nSampleRate = aInfo.mRate;
    err = aOmx.SetParameter(OMX_IndexParamAudioMp3, &mp3Param, sizeof(mp3Param));
    RETURN_IF_ERR(err);

    LOG("Config OMX_IndexParamAudioMp3, channel %lu, sample rate %lu",
        mp3Param.nChannels, mp3Param.nSampleRate);

    return ConfigAudioOutputPort(aOmx, aInfo);
  }
};

enum OmxAmrSampleRate {
  kNarrowBand = 8000,
  kWideBand = 16000,
};

template <OmxAmrSampleRate R>
class OmxAmrConfig : public OmxAudioConfig
{
public:
  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const AudioInfo& aInfo) override
  {
    OMX_ERRORTYPE err;

    OMX_AUDIO_PARAM_AMRTYPE def;
    InitOmxParameter(&def);
    def.nPortIndex = aOmx.InputPortIndex();
    err = aOmx.GetParameter(OMX_IndexParamAudioAmr, &def, sizeof(def));
    RETURN_IF_ERR(err);

    def.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatFSF;
    err = aOmx.SetParameter(OMX_IndexParamAudioAmr, &def, sizeof(def));
    RETURN_IF_ERR(err);

    MOZ_ASSERT(aInfo.mChannels == 1);
    MOZ_ASSERT(aInfo.mRate == R);

    return ConfigAudioOutputPort(aOmx, aInfo);
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
    } else if (aMimeType.EqualsLiteral("audio/mp3") ||
                aMimeType.EqualsLiteral("audio/mpeg")) {
      conf.reset(new OmxMp3Config());
    } else if (aMimeType.EqualsLiteral("audio/3gpp")) {
      conf.reset(new OmxAmrConfig<OmxAmrSampleRate::kNarrowBand>());
    } else if (aMimeType.EqualsLiteral("audio/amr-wb")) {
      conf.reset(new OmxAmrConfig<OmxAmrSampleRate::kWideBand>());
    }
  }
  return Move(conf);
}

// There should be a better way to calculate it.
#define MIN_VIDEO_INPUT_BUFFER_SIZE 64 * 1024

class OmxCommonVideoConfig : public OmxVideoConfig
{
public:
  explicit OmxCommonVideoConfig()
    : OmxVideoConfig()
  {}

  OMX_ERRORTYPE Apply(OmxPlatformLayer& aOmx, const VideoInfo& aInfo) override
  {
    OMX_ERRORTYPE err;
    OMX_PARAM_PORTDEFINITIONTYPE def;

    // Set up in/out port definition.
    nsTArray<uint32_t> ports;
    aOmx.GetPortIndices(ports);
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
        def.format.video.eCompressionFormat = aOmx.CompressionFormat();
        def.format.video.eColorFormat = OMX_COLOR_FormatUnused;
        if (def.nBufferSize < MIN_VIDEO_INPUT_BUFFER_SIZE) {
          def.nBufferSize = aInfo.mImage.width * aInfo.mImage.height;
          LOG("Change input buffer size to %lu", def.nBufferSize);
        }
      } else {
        def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
      }

      err = aOmx.SetParameter(OMX_IndexParamPortDefinition, &def, sizeof(def));
    }
    return err;
  }
};

template<>
UniquePtr<OmxVideoConfig>
ConfigForMime(const nsACString& aMimeType)
{
  UniquePtr<OmxVideoConfig> conf;

  if (OmxPlatformLayer::SupportsMimeType(aMimeType)) {
    conf.reset(new OmxCommonVideoConfig());
  }
  return Move(conf);
}

OMX_ERRORTYPE
OmxPlatformLayer::Config()
{
  MOZ_ASSERT(mInfo);

  OMX_PORT_PARAM_TYPE portParam;
  InitOmxParameter(&portParam);
  if (mInfo->IsAudio()) {
    GetParameter(OMX_IndexParamAudioInit, &portParam, sizeof(portParam));
    mStartPortNumber = portParam.nStartPortNumber;
    UniquePtr<OmxAudioConfig> conf(ConfigForMime<OmxAudioConfig>(mInfo->mMimeType));
    MOZ_ASSERT(conf.get());
    return conf->Apply(*this, *(mInfo->GetAsAudioInfo()));
  } else if (mInfo->IsVideo()) {
    GetParameter(OMX_IndexParamVideoInit, &portParam, sizeof(portParam));
    UniquePtr<OmxVideoConfig> conf(ConfigForMime<OmxVideoConfig>(mInfo->mMimeType));
    MOZ_ASSERT(conf.get());
    return conf->Apply(*this, *(mInfo->GetAsVideoInfo()));
  } else {
    MOZ_ASSERT_UNREACHABLE("non-AV data (text?) is not supported.");
    return OMX_ErrorNotImplemented;
  }
}

OMX_VIDEO_CODINGTYPE
OmxPlatformLayer::CompressionFormat()
{
  MOZ_ASSERT(mInfo);

  if (mInfo->mMimeType.EqualsLiteral("video/avc")) {
    return OMX_VIDEO_CodingAVC;
  } else if (mInfo->mMimeType.EqualsLiteral("video/mp4v-es") ||
       mInfo->mMimeType.EqualsLiteral("video/mp4")) {
    return OMX_VIDEO_CodingMPEG4;
  } else if (mInfo->mMimeType.EqualsLiteral("video/3gpp")) {
    return OMX_VIDEO_CodingH263;
  } else if (VPXDecoder::IsVP8(mInfo->mMimeType)) {
    return static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVP8);
  } else {
    MOZ_ASSERT_UNREACHABLE("Unsupported compression format");
    return OMX_VIDEO_CodingUnused;
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
