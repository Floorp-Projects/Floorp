/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP4Decoder.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MP4Demuxer.h"
#include "mozilla/Preferences.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/Logging.h"
#include "mozilla/SharedThreadPool.h"
#include "nsMimeTypes.h"
#include "VideoUtils.h"

#ifdef MOZ_WIDGET_ANDROID
#include "nsIGfxInfo.h"
#endif
#include "mozilla/layers/LayersTypes.h"

#include "PDMFactory.h"

namespace mozilla {

MP4Decoder::MP4Decoder(MediaDecoderInit& aInit)
  : ChannelMediaDecoder(aInit)
{
}

MediaDecoderStateMachine* MP4Decoder::CreateStateMachine()
{
  MediaDecoderReaderInit init(this);
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  mReader = new MediaFormatReader(init, new MP4Demuxer(mResource));
  return new MediaDecoderStateMachine(this, mReader);
}

static bool
IsWhitelistedH264Codec(const nsAString& aCodec)
{
  int16_t profile = 0, level = 0;

  if (!ExtractH264CodecDetails(aCodec, profile, level)) {
    return false;
  }

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
MP4Decoder::IsSupportedType(const MediaContainerType& aType,
                            DecoderDoctorDiagnostics* aDiagnostics)
{
  if (!IsEnabled()) {
    return false;
  }

  // Whitelist MP4 types, so they explicitly match what we encounter on
  // the web, as opposed to what we use internally (i.e. what our demuxers
  // etc output).
  const bool isAudio = aType.Type() == MEDIAMIMETYPE("audio/mp4")
                       || aType.Type() == MEDIAMIMETYPE("audio/x-m4a");
  const bool isVideo = aType.Type() == MEDIAMIMETYPE("video/mp4")
                       || aType.Type() == MEDIAMIMETYPE("video/quicktime")
  // On B2G, treat 3GPP as MP4 when Gonk PDM is available.
#ifdef MOZ_GONK_MEDIACODEC
                       || aType.Type() == MEDIAMIMETYPE(VIDEO_3GPP)
#endif
                       || aType.Type() == MEDIAMIMETYPE("video/x-m4v");

  if (!isAudio && !isVideo) {
    return false;
  }

  nsTArray<UniquePtr<TrackInfo>> trackInfos;
  if (aType.ExtendedType().Codecs().IsEmpty()) {
    // No codecs specified. Assume H.264
    if (isAudio) {
      trackInfos.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
    } else {
      MOZ_ASSERT(isVideo);
      trackInfos.AppendElement(
        CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          NS_LITERAL_CSTRING("video/avc"), aType));
    }
  } else {
    // Verify that all the codecs specified are ones that we expect that
    // we can play.
    for (const auto& codec : aType.ExtendedType().Codecs().Range()) {
      if (IsAACCodecString(codec)) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/mp4a-latm"), aType));
        continue;
      }
      if (codec.EqualsLiteral("mp3")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/mpeg"), aType));
        continue;
      }
      if (codec.EqualsLiteral("opus")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/opus"), aType));
        continue;
      }
      if (codec.EqualsLiteral("flac")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("audio/flac"), aType));
        continue;
      }
      if (codec.EqualsLiteral("vp9") || codec.EqualsLiteral("vp9.0")) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("video/vp9"), aType));
        continue;
      }
      // Note: Only accept H.264 in a video content type, not in an audio
      // content type.
      if (IsWhitelistedH264Codec(codec) && isVideo) {
        trackInfos.AppendElement(
          CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
            NS_LITERAL_CSTRING("video/avc"), aType));
        continue;
      }
      // Some unsupported codec.
      return false;
    }
  }

  // Verify that we have a PDM that supports the whitelisted types.
  RefPtr<PDMFactory> platform = new PDMFactory();
  for (const auto& trackInfo : trackInfos) {
    if (!trackInfo || !platform->Supports(*trackInfo, aDiagnostics)) {
      return false;
    }
  }

  return true;
}

/* static */
bool
MP4Decoder::IsH264(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("video/mp4") ||
         aMimeType.EqualsLiteral("video/avc");
}

/* static */
bool
MP4Decoder::IsAAC(const nsACString& aMimeType)
{
  return aMimeType.EqualsLiteral("audio/mp4a-latm");
}

/* static */
bool
MP4Decoder::IsEnabled()
{
  return MediaPrefs::MP4Enabled();
}

// sTestH264ExtraData represents the content of the avcC atom found in
// an AVC1 h264 video. It contains the H264 SPS and PPS NAL.
// the structure of the avcC atom is as follow:
// write(0x1);  // version, always 1
// write(sps[0].data[1]); // profile
// write(sps[0].data[2]); // compatibility
// write(sps[0].data[3]); // level
// write(0xFC | 3); // reserved (6 bits), NULA length size - 1 (2 bits)
// write(0xE0 | 1); // reserved (3 bits), num of SPS (5 bits)
// write_word(sps[0].size); // 2 bytes for length of SPS
// for(size_t i=0 ; i < sps[0].size ; ++i)
//   write(sps[0].data[i]); // data of SPS
// write(&b, pps.size());  // num of PPS
// for(size_t i=0 ; i < pps.size() ; ++i) {
//   write_word(pps[i].size);  // 2 bytes for length of PPS
//   for(size_t j=0 ; j < pps[i].size ; ++j)
//     write(pps[i].data[j]);  // data of PPS
//   }
// }
// here we have a h264 Baseline, 640x360
// We use a 640x360 extradata, as some video framework (Apple VT) will never
// attempt to use hardware decoding for small videos.
static const uint8_t sTestH264ExtraData[] = {
  0x01, 0x42, 0xc0, 0x1e, 0xff, 0xe1, 0x00, 0x17, 0x67, 0x42,
  0xc0, 0x1e, 0xbb, 0x40, 0x50, 0x17, 0xfc, 0xb8, 0x08, 0x80,
  0x00, 0x00, 0x32, 0x00, 0x00, 0x0b, 0xb5, 0x07, 0x8b, 0x17,
  0x50, 0x01, 0x00, 0x04, 0x68, 0xce, 0x32, 0xc8
};

static already_AddRefed<MediaDataDecoder>
CreateTestH264Decoder(layers::KnowsCompositor* aKnowsCompositor,
                      VideoInfo& aConfig,
                      TaskQueue* aTaskQueue)
{
  aConfig.mMimeType = "video/avc";
  aConfig.mId = 1;
  aConfig.mDuration = media::TimeUnit::FromMicroseconds(40000);
  aConfig.mImage = aConfig.mDisplay = nsIntSize(640, 360);
  aConfig.mExtraData = new MediaByteBuffer();
  aConfig.mExtraData->AppendElements(sTestH264ExtraData,
                                     MOZ_ARRAY_LENGTH(sTestH264ExtraData));

  RefPtr<PDMFactory> platform = new PDMFactory();
  RefPtr<MediaDataDecoder> decoder(platform->CreateDecoder({ aConfig, aTaskQueue, aKnowsCompositor }));

  return decoder.forget();
}

/* static */ already_AddRefed<dom::Promise>
MP4Decoder::IsVideoAccelerated(layers::KnowsCompositor* aKnowsCompositor, nsIGlobalObject* aParent)
{
  MOZ_ASSERT(NS_IsMainThread());

  ErrorResult rv;
  RefPtr<dom::Promise> promise;
  promise = dom::Promise::Create(aParent, rv);
  if (rv.Failed()) {
    rv.SuppressException();
    return nullptr;
  }

  RefPtr<TaskQueue> taskQueue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
    "MP4Decoder::IsVideoAccelerated::taskQueue");
  VideoInfo config;
  RefPtr<MediaDataDecoder> decoder(CreateTestH264Decoder(aKnowsCompositor, config, taskQueue));
  if (!decoder) {
    taskQueue->BeginShutdown();
    taskQueue->AwaitShutdownAndIdle();
    promise->MaybeResolve(NS_LITERAL_STRING("No; Failed to create H264 decoder"));
    return promise.forget();
  }

  decoder->Init()
    ->Then(aParent->AbstractMainThreadFor(TaskCategory::Other),
           __func__,
           [promise, decoder, taskQueue] (TrackInfo::TrackType aTrack) {
             nsCString failureReason;
             bool ok = decoder->IsHardwareAccelerated(failureReason);
             nsAutoString result;
             if (ok) {
               result.AssignLiteral("Yes");
             } else {
               result.AssignLiteral("No");
             }
             if (failureReason.Length()) {
               result.AppendLiteral("; ");
               AppendUTF8toUTF16(failureReason, result);
             }
             decoder->Shutdown();
             taskQueue->BeginShutdown();
             taskQueue->AwaitShutdownAndIdle();
             promise->MaybeResolve(result);
           },
           [promise, decoder, taskQueue] (MediaResult aError) {
             decoder->Shutdown();
             taskQueue->BeginShutdown();
             taskQueue->AwaitShutdownAndIdle();
             promise->MaybeResolve(NS_LITERAL_STRING("No; Failed to initialize H264 decoder"));
           });

  return promise.forget();
}

void
MP4Decoder::GetMozDebugReaderData(nsACString& aString)
{
  // This is definitely a MediaFormatReader. See CreateStateMachine() above.
  auto reader = static_cast<MediaFormatReader*>(mReader.get());
  if (reader) {
    reader->GetMozDebugReaderData(aString);
  }
}

} // namespace mozilla
