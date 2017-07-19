/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderReader.h"

#include "AbstractMediaDecoder.h"
#include "ImageContainer.h"
#include "MediaPrefs.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/mozalloc.h"
#include "nsPrintfCString.h"
#include <algorithm>
#include <stdint.h>

using namespace mozilla::media;

namespace mozilla {

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

extern LazyLogModule gMediaDecoderLog;

// avoid redefined macro in unified build
#undef FMT
#undef DECODER_LOG
#undef DECODER_WARN

#define FMT(x, ...) "Decoder=%p " x, mDecoder, ##__VA_ARGS__
#define DECODER_LOG(...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define DECODER_WARN(...) NS_WARNING(nsPrintfCString(FMT(__VA_ARGS__)).get())

MediaDecoderReader::MediaDecoderReader(MediaDecoderReaderInit& aInit)
  : mDecoder(aInit.mDecoder)
  , mTaskQueue(new TaskQueue(
      GetMediaThreadPool(MediaThreadType::PLAYBACK),
      "MediaDecoderReader::mTaskQueue",
      /* aSupportsTailDispatch = */ true))
  , mBuffered(mTaskQueue, TimeIntervals(), "MediaDecoderReader::mBuffered (Canonical)")
  , mShutdown(false)
  , mResource(aInit.mResource)
{
  MOZ_COUNT_CTOR(MediaDecoderReader);
  MOZ_ASSERT(NS_IsMainThread());
}

nsresult
MediaDecoderReader::Init()
{
  return InitInternal();
}

MediaDecoderReader::~MediaDecoderReader()
{
  MOZ_ASSERT(mShutdown);
  MOZ_COUNT_DTOR(MediaDecoderReader);
}

size_t MediaDecoderReader::SizeOfVideoQueueInBytes() const
{
  return 0;
}

size_t MediaDecoderReader::SizeOfAudioQueueInBytes() const
{
  return 0;
}

size_t MediaDecoderReader::SizeOfVideoQueueInFrames()
{
  return 0;
}

size_t MediaDecoderReader::SizeOfAudioQueueInFrames()
{
  return 0;
}

void
MediaDecoderReader::UpdateDuration(const media::TimeUnit& aDuration)
{
  MOZ_ASSERT(OnTaskQueue());
  mDuration = Some(aDuration);
  UpdateBuffered();
}

nsresult MediaDecoderReader::ResetDecode(TrackSet aTracks)
{
  return NS_OK;
}

RefPtr<ShutdownPromise>
MediaDecoderReader::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  mShutdown = true;

  ReleaseResources();
  mBuffered.DisconnectAll();

  mDecoder = nullptr;

  return mTaskQueue->BeginShutdown();
}

} // namespace mozilla
