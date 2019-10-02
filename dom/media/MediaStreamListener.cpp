/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamListener.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "StreamTracks.h"

namespace mozilla {

#ifdef LOG
#  undef LOG
#endif

#define LOG(type, msg) MOZ_LOG(gMediaStreamGraphLog, type, msg)

void DirectMediaStreamTrackListener::MirrorAndDisableSegment(
    AudioSegment& aFrom, AudioSegment& aTo) {
  aTo.AppendNullData(aFrom.GetDuration());
}

void DirectMediaStreamTrackListener::MirrorAndDisableSegment(
    VideoSegment& aFrom, VideoSegment& aTo, DisabledTrackMode aMode) {
  if (aMode == DisabledTrackMode::SILENCE_BLACK) {
    for (VideoSegment::ChunkIterator it(aFrom); !it.IsEnded(); it.Next()) {
      aTo.AppendFrame(do_AddRef(it->mFrame.GetImage()),
                      it->mFrame.GetIntrinsicSize(), it->GetPrincipalHandle(),
                      true);
      aTo.ExtendLastFrameBy(it->GetDuration());
    }
  } else if (aMode == DisabledTrackMode::SILENCE_FREEZE) {
    aTo.AppendNullData(aFrom.GetDuration());
  }
}

void DirectMediaStreamTrackListener::
    NotifyRealtimeTrackDataAndApplyTrackDisabling(MediaStreamGraph* aGraph,
                                                  StreamTime aTrackOffset,
                                                  MediaSegment& aMedia) {
  if (mDisabledFreezeCount == 0 && mDisabledBlackCount == 0) {
    NotifyRealtimeTrackData(aGraph, aTrackOffset, aMedia);
    return;
  }

  DisabledTrackMode mode = mDisabledBlackCount > 0
                               ? DisabledTrackMode::SILENCE_BLACK
                               : DisabledTrackMode::SILENCE_FREEZE;
  UniquePtr<MediaSegment> media(aMedia.CreateEmptyClone());
  if (aMedia.GetType() == MediaSegment::AUDIO) {
    MirrorAndDisableSegment(static_cast<AudioSegment&>(aMedia),
                            static_cast<AudioSegment&>(*media));
  } else if (aMedia.GetType() == MediaSegment::VIDEO) {
    MirrorAndDisableSegment(static_cast<VideoSegment&>(aMedia),
                            static_cast<VideoSegment&>(*media), mode);
  } else {
    MOZ_CRASH("Unsupported media type");
  }
  NotifyRealtimeTrackData(aGraph, aTrackOffset, *media);
}

void DirectMediaStreamTrackListener::IncreaseDisabled(DisabledTrackMode aMode) {
  if (aMode == DisabledTrackMode::SILENCE_FREEZE) {
    ++mDisabledFreezeCount;
  } else if (aMode == DisabledTrackMode::SILENCE_BLACK) {
    ++mDisabledBlackCount;
  } else {
    MOZ_ASSERT(false, "Unknown disabled mode");
  }

  LOG(LogLevel::Debug,
      ("DirectMediaStreamTrackListener %p increased disabled "
       "mode %s. Current counts are: freeze=%d, black=%d",
       this, aMode == DisabledTrackMode::SILENCE_FREEZE ? "freeze" : "black",
       int32_t(mDisabledFreezeCount), int32_t(mDisabledBlackCount)));
}

void DirectMediaStreamTrackListener::DecreaseDisabled(DisabledTrackMode aMode) {
  if (aMode == DisabledTrackMode::SILENCE_FREEZE) {
    --mDisabledFreezeCount;
    MOZ_ASSERT(mDisabledFreezeCount >= 0, "Double decrease");
  } else if (aMode == DisabledTrackMode::SILENCE_BLACK) {
    --mDisabledBlackCount;
    MOZ_ASSERT(mDisabledBlackCount >= 0, "Double decrease");
  } else {
    MOZ_ASSERT(false, "Unknown disabled mode");
  }

  LOG(LogLevel::Debug,
      ("DirectMediaStreamTrackListener %p decreased disabled "
       "mode %s. Current counts are: freeze=%d, black=%d",
       this, aMode == DisabledTrackMode::SILENCE_FREEZE ? "freeze" : "black",
       int32_t(mDisabledFreezeCount), int32_t(mDisabledBlackCount)));
}

}  // namespace mozilla
