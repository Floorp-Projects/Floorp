/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BufferDecoder.h"

#include "nsISupports.h"
#include "MediaResource.h"

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;

NS_IMPL_ISUPPORTS0(BufferDecoder)

BufferDecoder::BufferDecoder(MediaResource* aResource)
  : mReentrantMonitor("BufferDecoder")
  , mResource(aResource)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(BufferDecoder);
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }
}

BufferDecoder::~BufferDecoder()
{
  // The dtor may run on any thread, we cannot be sure.
  MOZ_COUNT_DTOR(BufferDecoder);
}

void
BufferDecoder::BeginDecoding(TaskQueue* aTaskQueueIdentity)
{
  MOZ_ASSERT(!mTaskQueueIdentity && aTaskQueueIdentity);
  mTaskQueueIdentity = aTaskQueueIdentity;
}

ReentrantMonitor&
BufferDecoder::GetReentrantMonitor()
{
  return mReentrantMonitor;
}

bool
BufferDecoder::OnStateMachineTaskQueue() const
{
  // BufferDecoder doesn't have the concept of a state machine.
  return true;
}

bool
BufferDecoder::OnDecodeTaskQueue() const
{
  MOZ_ASSERT(mTaskQueueIdentity, "Forgot to call BeginDecoding?");
  return mTaskQueueIdentity->IsCurrentThreadIn();
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
BufferDecoder::NotifyDecodedFrames(uint32_t aParsed, uint32_t aDecoded,
                                   uint32_t aDropped)
{
  // ignore
}

void
BufferDecoder::SetMediaSeekable(bool aMediaSeekable)
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
BufferDecoder::MetadataLoaded(nsAutoPtr<MediaInfo> aInfo, nsAutoPtr<MetadataTags> aTags, MediaDecoderEventVisibility aEventVisibility)
{
  // ignore
}

void
BufferDecoder::FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo, MediaDecoderEventVisibility aEventVisibility)
{
  // ignore
}

void
BufferDecoder::OnReadMetadataCompleted()
{
  // ignore
}

void
BufferDecoder::NotifyWaitingForResourcesStatusChanged()
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
