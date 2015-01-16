/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspOmxReader.h"

#include "AbstractMediaDecoder.h"
#include "MediaDecoderStateMachine.h"
#include "OmxDecoder.h"
#include "RtspExtractor.h"
#include "RtspMediaResource.h"
#include "RtspOmxDecoder.h"

using namespace android;

namespace mozilla {

nsresult RtspOmxReader::InitOmxDecoder()
{
  if (!mOmxDecoder.get()) {
    NS_ASSERTION(mDecoder, "RtspOmxReader mDecoder is null.");
    NS_ASSERTION(mDecoder->GetResource(),
                 "RtspOmxReader mDecoder->GetResource() is null.");
    mExtractor = new RtspExtractor(mRtspResource);
    mOmxDecoder = new OmxDecoder(mDecoder->GetResource(), mDecoder);
    if (!mOmxDecoder->Init(mExtractor)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsRefPtr<MediaDecoderReader::SeekPromise>
RtspOmxReader::Seek(int64_t aTime, int64_t aEndTime)
{
  // The seek function of Rtsp is time-based, we call the SeekTime function in
  // RtspMediaResource. The SeekTime function finally send a seek command to
  // Rtsp stream server through network and also clear the buffer data in
  // RtspMediaResource.
  if (mRtspResource) {
    mRtspResource->SeekTime(aTime);
    mRtspResource->EnablePlayoutDelay();
  }

  // Call |MediaOmxReader::Seek| to notify the OMX decoder we are performing a
  // seek operation. The function will clear the |mVideoQueue| and |mAudioQueue|
  // that store the decoded data and also call the |DecodeToTarget| to pass
  // the seek time to OMX a/v decoders.
  return MediaOmxReader::Seek(aTime, aEndTime);
}

void RtspOmxReader::SetIdle() {
  // Call parent class to set OMXCodec idle.
  MediaOmxReader::SetIdle();

  // Need to pause RTSP streaming OMXCodec decoding.
  if (mRtspResource) {
    nsIStreamingProtocolController* controller =
        mRtspResource->GetMediaStreamController();
    if (controller) {
      controller->Pause();
    }
    mRtspResource->SetSuspend(true);
  }
}

void RtspOmxReader::EnsureActive() {
  // Need to start RTSP streaming OMXCodec decoding.
  if (mRtspResource) {
    nsIStreamingProtocolController* controller =
        mRtspResource->GetMediaStreamController();
    if (controller) {
      controller->Play();
    }
    mRtspResource->SetSuspend(false);
  }

  // Call parent class to set OMXCodec active.
  MediaOmxReader::EnsureActive();
}

nsresult RtspOmxReader::ReadMetadata(MediaInfo *aInfo, MetadataTags **aTags)
{
  // Send a PLAY command to the RTSP server before reading metadata.
  // Because we might need some decoded samples to ensure we have configuration.
  mRtspResource->DisablePlayoutDelay();
  EnsureActive();
  nsresult rv = MediaOmxReader::ReadMetadata(aInfo, aTags);

  if (rv == NS_OK && !IsWaitingMediaResources()) {
    mRtspResource->EnablePlayoutDelay();
  } else if (IsWaitingMediaResources()) {
    // Send a PAUSE to the RTSP server because the underlying media resource is
    // not ready.
    SetIdle();
  }
  return rv;
}

} // namespace mozilla
