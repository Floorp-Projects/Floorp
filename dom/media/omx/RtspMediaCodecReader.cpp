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

nsresult
RtspMediaCodecReader::Seek(int64_t aTime, int64_t aStartTime,
                           int64_t aEndTime, int64_t aCurrentTime)
{
  // The seek function of Rtsp is time-based, we call the SeekTime function in
  // RtspMediaResource. The SeekTime function finally send a seek command to
  // Rtsp stream server through network and also clear the buffer data in
  // RtspMediaResource.
  mRtspResource->SeekTime(aTime);

  return MediaCodecReader::Seek(aTime, aStartTime, aEndTime, aCurrentTime);
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

void
RtspMediaCodecReader::RequestAudioData()
{
  EnsureActive();
  MediaCodecReader::RequestAudioData();
}

void
RtspMediaCodecReader::RequestVideoData(bool aSkipToNextKeyframe,
                                       int64_t aTimeThreshold)
{
  EnsureActive();
  MediaCodecReader::RequestVideoData(aSkipToNextKeyframe, aTimeThreshold);
}

nsresult
RtspMediaCodecReader::ReadMetadata(MediaInfo* aInfo,
                                   MetadataTags** aTags)
{
  nsresult rv = MediaCodecReader::ReadMetadata(aInfo, aTags);
  if (rv == NS_OK && !IsWaitingMediaResources()) {
    EnsureActive();
  }

  return rv;
}

} // namespace mozilla
