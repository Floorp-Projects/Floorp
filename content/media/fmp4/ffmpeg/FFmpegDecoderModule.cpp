/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegRuntimeLinker.h"
#include "FFmpegAACDecoder.h"
#include "FFmpegH264Decoder.h"

#include "FFmpegDecoderModule.h"

namespace mozilla
{

PRLogModuleInfo* GetFFmpegDecoderLog()
{
  static PRLogModuleInfo* sFFmpegDecoderLog = nullptr;
  if (!sFFmpegDecoderLog) {
    sFFmpegDecoderLog = PR_NewLogModule("FFmpegDecoderModule");
  }
  return sFFmpegDecoderLog;
}

bool FFmpegDecoderModule::sFFmpegLinkDone = false;

FFmpegDecoderModule::FFmpegDecoderModule()
{
  MOZ_COUNT_CTOR(FFmpegDecoderModule);
}

FFmpegDecoderModule::~FFmpegDecoderModule() {
  MOZ_COUNT_DTOR(FFmpegDecoderModule);
}

bool
FFmpegDecoderModule::Link()
{
  if (sFFmpegLinkDone) {
    return true;
  }

  if (!FFmpegRuntimeLinker::Link()) {
    NS_WARNING("Failed to link FFmpeg shared libraries.");
    return false;
  }

  sFFmpegLinkDone = true;

  return true;
}

nsresult
FFmpegDecoderModule::Shutdown()
{
  // Nothing to do here.
  return NS_OK;
}

MediaDataDecoder*
FFmpegDecoderModule::CreateH264Decoder(
  const mp4_demuxer::VideoDecoderConfig& aConfig,
  mozilla::layers::LayersBackend aLayersBackend,
  mozilla::layers::ImageContainer* aImageContainer,
  MediaTaskQueue* aVideoTaskQueue, MediaDataDecoderCallback* aCallback)
{
  FFMPEG_LOG("Creating FFmpeg H264 decoder.");
  return new FFmpegH264Decoder(aVideoTaskQueue, aCallback, aConfig,
                               aImageContainer);
}

MediaDataDecoder*
FFmpegDecoderModule::CreateAACDecoder(
  const mp4_demuxer::AudioDecoderConfig& aConfig,
  MediaTaskQueue* aAudioTaskQueue, MediaDataDecoderCallback* aCallback)
{
  FFMPEG_LOG("Creating FFmpeg AAC decoder.");
  return new FFmpegAACDecoder(aAudioTaskQueue, aCallback, aConfig);
}

} // namespace mozilla
