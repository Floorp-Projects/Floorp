/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferDecoder.h"

#include "nsISupports.h"
#include "MediaResource.h"

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#endif

NS_IMPL_ISUPPORTS0(BufferDecoder)

BufferDecoder::BufferDecoder(MediaResource* aResource)
  : mReentrantMonitor("BufferDecoder")
  , mResource(aResource)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(BufferDecoder);
#ifdef PR_LOGGING
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }
#endif
}

BufferDecoder::~BufferDecoder()
{
  // The dtor may run on any thread, we cannot be sure.
  MOZ_COUNT_DTOR(BufferDecoder);
}

void
BufferDecoder::BeginDecoding(nsIThread* aDecodeThread)
{
  MOZ_ASSERT(!mDecodeThread && aDecodeThread);
  mDecodeThread = aDecodeThread;
}

ReentrantMonitor&
BufferDecoder::GetReentrantMonitor()
{
  return mReentrantMonitor;
}

bool
BufferDecoder::IsShutdown() const
{
  // BufferDecoder cannot be shut down.
  return false;
}

bool
BufferDecoder::OnStateMachineThread() const
{
  // BufferDecoder doesn't have the concept of a state machine.
  return true;
}

bool
BufferDecoder::OnDecodeThread() const
{
  MOZ_ASSERT(mDecodeThread, "Forgot to call BeginDecoding?");
  return IsCurrentThread(mDecodeThread);
}

MediaResource*
BufferDecoder::GetResource() const
{
  return mResource;
}

void
BufferDecoder::NotifyBytesConsumed(int64_t aBytes, int64_t aOffset)
{
  // ignore
}

void
BufferDecoder::NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded)
{
  // ignore
}

int64_t
BufferDecoder::GetEndMediaTime() const
{
  // unknown
  return -1;
}

int64_t
BufferDecoder::GetMediaDuration()
{
  // unknown
  return -1;
}

void
BufferDecoder::SetMediaDuration(int64_t aDuration)
{
  // ignore
}

void
BufferDecoder::UpdateEstimatedMediaDuration(int64_t aDuration)
{
  // ignore
}

void
BufferDecoder::SetMediaSeekable(bool aMediaSeekable)
{
  // ignore
}

void
BufferDecoder::SetTransportSeekable(bool aTransportSeekable)
{
  // ignore
}

VideoFrameContainer*
BufferDecoder::GetVideoFrameContainer()
{
  // no video frame
  return nullptr;
}

layers::ImageContainer*
BufferDecoder::GetImageContainer()
{
  // no image container
  return nullptr;
}

bool
BufferDecoder::IsTransportSeekable()
{
  return false;
}

bool
BufferDecoder::IsMediaSeekable()
{
  return false;
}

void
BufferDecoder::MetadataLoaded(int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags)
{
  // ignore
}

void
BufferDecoder::QueueMetadata(int64_t aTime, int aChannels, int aRate, bool aHasAudio, bool aHasVideo, MetadataTags* aTags)
{
  // ignore
}

void
BufferDecoder::SetMediaEndTime(int64_t aTime)
{
  // ignore
}

void
BufferDecoder::UpdatePlaybackPosition(int64_t aTime)
{
  // ignore
}

void
BufferDecoder::OnReadMetadataCompleted()
{
  // ignore
}

MediaDecoderOwner*
BufferDecoder::GetOwner()
{
  // unknown
  return nullptr;
}

} // namespace mozilla
