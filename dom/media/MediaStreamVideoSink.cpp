/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaStreamVideoSink.h"

#include "VideoSegment.h"

namespace mozilla {
void
MediaStreamVideoSink::NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                                              StreamTime aTrackOffset,
                                              const MediaSegment& aMedia)
{
  if (aMedia.GetType() == MediaSegment::VIDEO) {
    SetCurrentFrames(static_cast<const VideoSegment&>(aMedia));
  }
}

} // namespace mozilla
