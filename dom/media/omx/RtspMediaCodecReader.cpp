/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspMediaCodecReader.h"

#include "RtspExtractor.h"
#include "RtspMediaResource.h"
#include "RtspMediaCodecDecoder.h"

using namespace android;

namespace mozilla {

RtspMediaCodecReader::RtspMediaCodecReader(AbstractMediaDecoder* aDecoder)
  : MediaCodecReader(aDecoder)
{
  NS_ASSERTION(mDecoder, "RtspMediaCodecReader mDecoder is null.");
  NS_ASSERTION(mDecoder->GetResource(),
               "RtspMediaCodecReader mDecoder->GetResource() is null.");
  mRtspResource = mDecoder->GetResource()->GetRtspPointer();
  MOZ_ASSERT(mRtspResource);
}

RtspMediaCodecReader::~RtspMediaCodecReader() {}

bool
RtspMediaCodecReader::CreateExtractor()
{
  if (mExtractor != nullptr) {
    return true;
  }

  mExtractor = new RtspExtractor(mRtspResource);

  return mExtractor != nullptr;
}

nsRefPtr<MediaDecoderReader::SeekPromise>
RtspMediaCodecReader::Seek(int64_t aTime, int64_t aEndTime)
{
  // The seek function of Rtsp is time-based, we call the SeekTime function in
  // RtspMediaResource. The SeekTime function finally send a seek command to
  // Rtsp stream server through network and also clear the buffer data in
  // RtspMediaResource.
  mRtspResource->SeekTime(aTime);

  return MediaCodecReader::Seek(aTime, aEndTime);
}

void
RtspMediaCodecReader::SetIdle()
{
  nsIStreamingProtocolController* controller =
    mRtspResource->GetMediaStreamController();
  if (controller) {
    controller->Pause();
  }
  mRtspResource->SetSuspend(true);
}

void
RtspMediaCodecReader::EnsureActive()
{
  // Need to start RTSP streaming OMXCodec decoding.
  nsIStreamingProtocolController* controller =
    mRtspResource->GetMediaStreamController();
  if (controller) {
    controller->Play();
  }
  mRtspResource->SetSuspend(false);
}

nsRefPtr<MediaDecoderReader::AudioDataPromise>
RtspMediaCodecReader::RequestAudioData()
{
  EnsureActive();
  return MediaCodecReader::RequestAudioData();
}

nsRefPtr<MediaDecoderReader::VideoDataPromise>
RtspMediaCodecReader::RequestVideoData(bool aSkipToNextKeyframe,
                                       int64_t aTimeThreshold)
{
  EnsureActive();
  return MediaCodecReader::RequestVideoData(aSkipToNextKeyframe, aTimeThreshold);
}

nsRefPtr<MediaDecoderReader::MetadataPromise>
RtspMediaCodecReader::AsyncReadMetadata()
{
  mRtspResource->DisablePlayoutDelay();
  EnsureActive();

  nsRefPtr<MediaDecoderReader::MetadataPromise> p =
    MediaCodecReader::AsyncReadMetadata();

  // Send a PAUSE to the RTSP server because the underlying media resource is
  // not ready.
  SetIdle();

  return p;
}

void
RtspMediaCodecReader::HandleResourceAllocated()
{
  EnsureActive();
  MediaCodecReader::HandleResourceAllocated();
  mRtspResource->EnablePlayoutDelay();;
}

} // namespace mozilla
