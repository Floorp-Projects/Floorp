
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

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

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

bool
MP3Decoder::IsEnabled() {
#ifdef MOZ_WIDGET_ANDROID
  // We need android.media.MediaCodec which exists in API level 16 and higher.
  return AndroidBridge::Bridge()->GetAPIVersion() >= 16;
#else
  return Preferences::GetBool("media.mp3.enabled");
#endif
}

} // namespace mozilla
