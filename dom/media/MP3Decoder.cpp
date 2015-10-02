
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MP3Decoder.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MP3Demuxer.h"
#include "mozilla/Preferences.h"
#include "PlatformDecoderModule.h"

namespace mozilla {

MediaDecoder*
MP3Decoder::Clone() {
  if (!IsEnabled()) {
    return nullptr;
  }
  return new MP3Decoder();
}

MediaDecoderStateMachine*
MP3Decoder::CreateStateMachine() {
  nsRefPtr<MediaDecoderReader> reader =
      new MediaFormatReader(this, new mp3::MP3Demuxer(GetResource()));
  return new MediaDecoderStateMachine(this, reader);
}

static already_AddRefed<MediaDataDecoder>
CreateTestMP3Decoder(AudioInfo& aConfig)
{
  PlatformDecoderModule::Init();

  nsRefPtr<PlatformDecoderModule> platform = PlatformDecoderModule::Create();
  if (!platform || !platform->SupportsMimeType(aConfig.mMimeType)) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoder> decoder(
    platform->CreateDecoder(aConfig, nullptr, nullptr));
  if (!decoder) {
    return nullptr;
  }

  return decoder.forget();
}

static bool
CanCreateMP3Decoder()
{
  static bool haveCachedResult = false;
  static bool result = false;
  if (haveCachedResult) {
    return result;
  }
  AudioInfo config;
  config.mMimeType = "audio/mpeg";
  config.mRate = 48000;
  config.mChannels = 2;
  config.mBitDepth = 16;
  nsRefPtr<MediaDataDecoder> decoder(CreateTestMP3Decoder(config));
  if (decoder) {
    result = true;
  }
  haveCachedResult = true;
  return result;
}

/* static */
bool
MP3Decoder::IsEnabled() {
  return CanCreateMP3Decoder();
}

/* static */
bool MP3Decoder::CanHandleMediaType(const nsACString& aType,
                                    const nsAString& aCodecs)
{
  if (aType.EqualsASCII("audio/mp3") || aType.EqualsASCII("audio/mpeg")) {
    return CanCreateMP3Decoder() &&
      (aCodecs.IsEmpty() || aCodecs.EqualsASCII("mp3"));
  }
  return false;
}

} // namespace mozilla
