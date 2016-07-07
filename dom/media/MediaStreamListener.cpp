/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamListener.h"

#include "AudioSegment.h"
#include "VideoSegment.h"
#include "StreamTracks.h"

namespace mozilla {

void
DirectMediaStreamTrackListener::MirrorAndDisableSegment(AudioSegment& aFrom,
                                                       AudioSegment& aTo)
{
  aTo.Clear();
  aTo.AppendNullData(aFrom.GetDuration());
}

void
DirectMediaStreamTrackListener::MirrorAndDisableSegment(VideoSegment& aFrom,
                                                       VideoSegment& aTo)
{
  aTo.Clear();
  for (VideoSegment::ChunkIterator it(aFrom); !it.IsEnded(); it.Next()) {
    aTo.AppendFrame(do_AddRef(it->mFrame.GetImage()), it->GetDuration(),
                    it->mFrame.GetIntrinsicSize(), it->GetPrincipalHandle(), true);
  }
}

void
DirectMediaStreamTrackListener::NotifyRealtimeTrackDataAndApplyTrackDisabling(MediaStreamGraph* aGraph,
                                                                             StreamTime aTrackOffset,
                                                                             MediaSegment& aMedia)
{
  if (mDisabledCount == 0) {
    NotifyRealtimeTrackData(aGraph, aTrackOffset, aMedia);
    return;
  }

  if (!mMedia) {
    mMedia = aMedia.CreateEmptyClone();
  }
  if (aMedia.GetType() == MediaSegment::AUDIO) {
    MirrorAndDisableSegment(static_cast<AudioSegment&>(aMedia),
                            static_cast<AudioSegment&>(*mMedia));
  } else if (aMedia.GetType() == MediaSegment::VIDEO) {
    MirrorAndDisableSegment(static_cast<VideoSegment&>(aMedia),
                            static_cast<VideoSegment&>(*mMedia));
  } else {
    MOZ_CRASH("Unsupported media type");
  }
  NotifyRealtimeTrackData(aGraph, aTrackOffset, *mMedia);
}

} // namespace mozilla
