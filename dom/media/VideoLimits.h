/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoLimits_h
#define VideoLimits_h

namespace mozilla {

// The maximum height and width of the video. Used for
// sanitizing the memory allocation of video frame buffers.
// The maximum resolution we anticipate encountering in the
// wild is 2160p (UHD "4K") or 4320p - 7680x4320 pixels for VR.
static const int32_t MAX_VIDEO_WIDTH = 8192;
static const int32_t MAX_VIDEO_HEIGHT = 4608;

} // namespace mozilla

#endif // VideoLimits_h
