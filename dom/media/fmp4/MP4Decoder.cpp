/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Decoder.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MP4Demuxer.h"
#include "mozilla/Preferences.h"
#include "nsCharSeparatedTokenizer.h"
#ifdef MOZ_EME
#include "mozilla/CDMProxy.h"
#endif
#include "mozilla/Logging.h"
#include "nsMimeTypes.h"
#include "nsContentTypeParser.h"
#include "VideoUtils.h"

#ifdef XP_WIN
#include "mozilla/WindowsVersion.h"
#endif
#ifdef MOZ_WIDGET_ANDROID
#include "nsIGfxInfo.h"
#include "AndroidBridge.h"
#endif
#include "mozilla/layers/LayersTypes.h"

#include "PDMFactory.h"

namespace mozilla {

#if defined(MOZ_GONK_MEDIACODEC) || defined(XP_WIN) || defined(MOZ_APPLEMEDIA) || defined(MOZ_FFMPEG)
#define MP4_READER_DORMANT_HEURISTIC
#else
#undef MP4_READER_DORMANT_HEURISTIC
#endif

MP4Decoder::MP4Decoder()
{
#if defined(MP4_READER_DORMANT_HEURISTIC)
  mDormantSupported = Preferences::GetBool("media.decoder.heuristic.dormant.enabled", false);
#endif
}

MediaDecoderStateMachine* MP4Decoder::CreateStateMachine()
{
  MediaDecoderReader* reader = new MediaFormatReader(this, new MP4Demuxer(GetResource()));

  return new MediaDecoderStateMachine(this, reader);
}

static bool
IsWhitelistedH264Codec(const nsAString& aCodec)
{
  int16_t profile = 0, level = 0;

  if (!ExtractH264CodecDetails(aCodec, profile, level)) {
    return false;
  }

#ifdef XP_WIN
  // Disable 4k video on windows vista since it performs poorly.
  if (!IsWin7OrLater() &&
      level >= H264_LEVEL_5) {
    return false;
  }
#endif

  // Just assume what we can play on all platforms the codecs/formats that
  // WMF can play, since we don't have documentation about what other
  // platforms can play... According to the WMF documentation:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/dd797815%28v=vs.85%29.aspx
  // "The Media Foundation H.264 video decoder is a Media Foundation Transform
  // that supports decoding of Baseline, Main, and High profiles, up to level
  // 5.1.". We also report that we can play Extended profile, as there are
  // bitstreams that are Extended compliant that are also Baseline compliant.
  return level >= H264_LEVEL_1 &&
         level <= H264_LEVEL_5_1 &&
         (profile == H264_PROFILE_BASE ||
          profile == H264_PROFILE_MAIN ||
          profile == H264_PROFILE_EXTENDED ||
          profile == H264_PROFILE_HIGH);
}

/* static */
bool
MP4Decoder::CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                               const nsAString& aCodecs)
{
  if (!IsEnabled()) {
    return false;
  }

  // Whitelist MP4 types, so they explicitly match what we encounter on
  // the web, as opposed to what we use internally (i.e. what our demuxers
  // etc output).
  const bool isMP4Audio = aMIMETypeExcludingCodecs.EqualsASCII("audio/mp4") ||
                          aMIMETypeExcludingCodecs.EqualsASCII("audio/x-m4a");
  const bool isMP4Video =
  // On B2G, treat 3GPP as MP4 when Gonk PDM is available.
#ifdef MOZ_GONK_MEDIACODEC
    aMIMETypeExcludingCodecs.EqualsASCII(VIDEO_3GPP) ||
#endif
    aMIMETypeExcludingCodecs.EqualsASCII("video/mp4") ||
    aMIMETypeExcludingCodecs.EqualsASCII("video/x-m4v");
  if (!isMP4Audio && !isMP4Video) {
    return false;
  }

  nsTArray<nsCString> codecMimes;
  if (aCodecs.IsEmpty()) {
    // No codecs specified. Assume AAC/H.264
    if (isMP4Audio) {
      codecMimes.AppendElement(NS_LITERAL_CSTRING("audio/mp4a-latm"));
    } else {
      MOZ_ASSERT(isMP4Video);
      codecMimes.AppendElement(NS_LITERAL_CSTRING("video/avc"));
    }
  } else {
    // Verify that all the codecs specified are ones that we expect that
    // we can play.
    nsTArray<nsString> codecs;
    if (!ParseCodecsString(aCodecs, codecs)) {
      return false;
    }
    for (const nsString& codec : codecs) {
      if (IsAACCodecString(codec)) {
        codecMimes.AppendElement(NS_LITERAL_CSTRING("audio/mp4a-latm"));
        continue;
      }
      if (codec.EqualsLiteral("mp3")) {
        codecMimes.AppendElement(NS_LITERAL_CSTRING("audio/mpeg"));
        continue;
      }
      // Note: Only accept H.264 in a video content type, not in an audio
      // content type.
      if (IsWhitelistedH264Codec(codec) && isMP4Video) {
        codecMimes.AppendElement(NS_LITERAL_CSTRING("video/avc"));
        continue;
      }
      // Some unsupported codec.
      return false;
    }
  }

  // Verify that we have a PDM that supports the whitelisted types.
  PDMFactory::Init();
  nsRefPtr<PDMFactory> platform = new PDMFactory();
  for (const nsCString& codecMime : codecMimes) {
    if (!platform->SupportsMimeType(codecMime)) {
      return false;
    }
  }

  return true;
}

/* static */ bool
MP4Decoder::CanHandleMediaType(const nsAString& aContentType)
{
  nsContentTypeParser parser(aContentType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return false;
  }
  nsString codecs;
  parser.GetParameter("codecs", codecs);

  return CanHandleMediaType(NS_ConvertUTF16toUTF8(mimeType), codecs);
}

/* static */
bool
MP4Decoder::IsEnabled()
{
  return Preferences::GetBool("media.fragmented-mp4.enabled");
}

static const uint8_t sTestH264ExtraData[] = {
  0x01, 0x64, 0x00, 0x0a, 0xff, 0xe1, 0x00, 0x17, 0x67, 0x64,
  0x00, 0x0a, 0xac, 0xd9, 0x44, 0x26, 0x84, 0x00, 0x00, 0x03,
  0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0xc8, 0x3c, 0x48, 0x96,
  0x58, 0x01, 0x00, 0x06, 0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0
};

static already_AddRefed<MediaDataDecoder>
CreateTestH264Decoder(layers::LayersBackend aBackend,
                      VideoInfo& aConfig)
{
  aConfig.mMimeType = "video/avc";
  aConfig.mId = 1;
  aConfig.mDuration = 40000;
  aConfig.mMediaTime = 0;
  aConfig.mDisplay = nsIntSize(64, 64);
  aConfig.mImage = nsIntRect(0, 0, 64, 64);
  aConfig.mExtraData = new MediaByteBuffer();
  aConfig.mExtraData->AppendElements(sTestH264ExtraData,
                                     MOZ_ARRAY_LENGTH(sTestH264ExtraData));

  PDMFactory::Init();

  nsRefPtr<PDMFactory> platform = new PDMFactory();
  nsRefPtr<MediaDataDecoder> decoder(
    platform->CreateDecoder(aConfig, nullptr, nullptr, aBackend, nullptr));

  return decoder.forget();
}

/* static */ bool
MP4Decoder::IsVideoAccelerated(layers::LayersBackend aBackend, nsACString& aFailureReason)
{
  VideoInfo config;
  nsRefPtr<MediaDataDecoder> decoder(CreateTestH264Decoder(aBackend, config));
  if (!decoder) {
    aFailureReason.AssignLiteral("Failed to create H264 decoder");
    return false;
  }
  bool result = decoder->IsHardwareAccelerated(aFailureReason);
  decoder->Shutdown();
  return result;
}

} // namespace mozilla
