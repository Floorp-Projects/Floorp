/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stagefright/foundation/ADebug.h>
#include "AudioOutput.h"

#include "mozilla/Logging.h"

namespace mozilla {

extern LazyLogModule gAudioOffloadPlayerLog;
#define AUDIO_OFFLOAD_LOG(type, msg) \
  MOZ_LOG(gAudioOffloadPlayerLog, type, msg)

using namespace android;

AudioOutput::AudioOutput(int aSessionId, int aUid) :
  mCallbackCookie(nullptr),
  mCallback(nullptr),
  mCallbackData(nullptr),
  mUid(aUid),
  mSessionId(aSessionId)
{
}

AudioOutput::~AudioOutput()
{
  Close();
}

ssize_t AudioOutput::FrameSize() const
{
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->frameSize();
}

status_t AudioOutput::GetPosition(uint32_t *aPosition) const
{
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->getPosition(aPosition);
}

status_t AudioOutput::SetVolume(float aVolume) const
{
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->setVolume(aVolume);
}

status_t AudioOutput::SetParameters(const String8& aKeyValuePairs)
{
  if (!mTrack.get()) {
    return NO_INIT;
  }
  return mTrack->setParameters(aKeyValuePairs);
}

status_t AudioOutput::Open(uint32_t aSampleRate,
                           int aChannelCount,
                           audio_channel_mask_t aChannelMask,
                           audio_format_t aFormat,
                           AudioCallback aCb,
                           void* aCookie,
                           audio_output_flags_t aFlags,
                           const audio_offload_info_t *aOffloadInfo)
{
  mCallback = aCb;
  mCallbackCookie = aCookie;

  if (((aFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) == 0) || !aCb ||
      !aOffloadInfo) {
    return BAD_VALUE;
  }

  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("open(%u, %d, 0x%x, 0x%x, %d 0x%x)",
      aSampleRate, aChannelCount, aChannelMask, aFormat, mSessionId, aFlags));

  if (aChannelMask == CHANNEL_MASK_USE_CHANNEL_ORDER) {
    aChannelMask = audio_channel_out_mask_from_count(aChannelCount);
    if (0 == aChannelMask) {
      AUDIO_OFFLOAD_LOG(LogLevel::Error, ("open() error, can\'t derive mask for"
          " %d audio channels", aChannelCount));
      return NO_INIT;
    }
  }

  sp<AudioTrack> t;
  CallbackData* newcbd = new CallbackData(this);

  t = new AudioTrack(
      AUDIO_STREAM_MUSIC,
      aSampleRate,
      aFormat,
      aChannelMask,
      0,  // Offloaded tracks will get frame count from AudioFlinger
      aFlags,
      CallbackWrapper,
      newcbd,
      0,  // notification frames
      mSessionId,
      AudioTrack::TRANSFER_CALLBACK,
      aOffloadInfo,
      mUid);

  if ((!t.get()) || (t->initCheck() != NO_ERROR)) {
    AUDIO_OFFLOAD_LOG(LogLevel::Error, ("Unable to create audio track"));
    delete newcbd;
    return NO_INIT;
  }

  mCallbackData = newcbd;
  t->setVolume(1.0);

  mTrack = t;
  return NO_ERROR;
}

status_t AudioOutput::Start()
{
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __PRETTY_FUNCTION__));
  if (!mTrack.get()) {
    return NO_INIT;
  }
  mTrack->setVolume(1.0);
  return mTrack->start();
}

void AudioOutput::Stop()
{
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __PRETTY_FUNCTION__));
  if (mTrack.get()) {
    mTrack->stop();
  }
}

void AudioOutput::Flush()
{
  if (mTrack.get()) {
    mTrack->flush();
  }
}

void AudioOutput::Pause()
{
  if (mTrack.get()) {
    mTrack->pause();
  }
}

void AudioOutput::Close()
{
  AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("%s", __PRETTY_FUNCTION__));
  mTrack.clear();

  delete mCallbackData;
  mCallbackData = nullptr;
}

// static
void AudioOutput::CallbackWrapper(int aEvent, void* aCookie, void* aInfo)
{
  CallbackData* data = (CallbackData*) aCookie;
  data->Lock();
  AudioOutput* me = data->GetOutput();
  AudioTrack::Buffer* buffer = (AudioTrack::Buffer*) aInfo;
  if (!me) {
    // no output set, likely because the track was scheduled to be reused
    // by another player, but the format turned out to be incompatible.
    data->Unlock();
    if (buffer) {
      buffer->size = 0;
    }
    return;
  }

  switch(aEvent) {

    case AudioTrack::EVENT_MORE_DATA: {

      size_t actualSize = (*me->mCallback)(me, buffer->raw, buffer->size,
          me->mCallbackCookie, CB_EVENT_FILL_BUFFER);

      if (actualSize == 0 && buffer->size > 0) {
        // We've reached EOS but the audio track is not stopped yet,
        // keep playing silence.
        memset(buffer->raw, 0, buffer->size);
        actualSize = buffer->size;
      }

      buffer->size = actualSize;
    } break;

    case AudioTrack::EVENT_STREAM_END:
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Callback wrapper: EVENT_STREAM_END"));
      (*me->mCallback)(me, nullptr /* buffer */, 0 /* size */,
          me->mCallbackCookie, CB_EVENT_STREAM_END);
      break;

    case AudioTrack::EVENT_NEW_IAUDIOTRACK :
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("Callback wrapper: EVENT_TEAR_DOWN"));
      (*me->mCallback)(me,  nullptr /* buffer */, 0 /* size */,
          me->mCallbackCookie, CB_EVENT_TEAR_DOWN);
      break;

    default:
      AUDIO_OFFLOAD_LOG(LogLevel::Debug, ("received unknown event type: %d in"
          " Callback wrapper!", aEvent));
      break;
  }

  data->Unlock();
}

} // namespace mozilla
