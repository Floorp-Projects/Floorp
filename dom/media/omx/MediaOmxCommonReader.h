/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_OMX_COMMON_READER_H
#define MEDIA_OMX_COMMON_READER_H

#include "MediaDecoderReader.h"

#include <utils/RefBase.h>

#include "mozilla/dom/AudioChannelBinding.h"

namespace android {
struct MOZ_EXPORT MediaSource;
class MediaStreamSource;
} // namespace android

namespace mozilla {

class AbstractMediaDecoder;

class MediaOmxCommonReader : public MediaDecoderReader
{
public:
  typedef MozPromise<bool /* aIgnored */, bool /* aIgnored */, /* IsExclusive = */ true> MediaResourcePromise;

  MediaOmxCommonReader(AbstractMediaDecoder* aDecoder);

  void SetAudioChannel(dom::AudioChannel aAudioChannel) {
    mAudioChannel = aAudioChannel;
  }

  virtual android::sp<android::MediaSource> GetAudioOffloadTrack() = 0;

#ifdef MOZ_AUDIO_OFFLOAD
  // Check whether it is possible to offload current audio track. This access
  // canOffloadStream() from libStageFright Utils.cpp, which is not there in
  // ANDROID_VERSION < 19
  void CheckAudioOffload();
#endif

protected:
  dom::AudioChannel mAudioChannel;
  // Weak reference to the MediaStreamSource that will be created by either
  // MediaOmxReader or MediaCodecReader.
  android::MediaStreamSource* mStreamSource;
  // Get value from the preferece, if true, we stop the audio offload.
  bool IsMonoAudioEnabled();
};

} // namespace mozilla

#endif // MEDIA_OMX_COMMON_READER_H
