/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_MEDIASTREAMTYPES_h_
#define MOZILLA_MEDIASTREAMTYPES_h_

#include "StreamTracks.h" // for TrackID

namespace mozilla {

enum MediaStreamGraphEvent : uint32_t {
  EVENT_FINISHED,
  EVENT_REMOVED,
  EVENT_HAS_DIRECT_LISTENERS, // transition from no direct listeners
  EVENT_HAS_NO_DIRECT_LISTENERS,  // transition to no direct listeners
};

// maskable flags, not a simple enumerated value
enum TrackEventCommand : uint32_t {
  TRACK_EVENT_NONE = 0x00,
  TRACK_EVENT_CREATED = 0x01,
  TRACK_EVENT_ENDED = 0x02,
  TRACK_EVENT_UNUSED = ~(TRACK_EVENT_ENDED | TRACK_EVENT_CREATED),
};

/**
 * Describes how a track should be disabled.
 *
 * ENABLED        Not disabled.
 * SILENCE_BLACK  Audio data is turned into silence, video frames are made black.
 * SILENCE_FREEZE Audio data is turned into silence, video freezes at last frame.
 */
enum class DisabledTrackMode
{
  ENABLED, SILENCE_BLACK, SILENCE_FREEZE
};
struct DisabledTrack {
  DisabledTrack(TrackID aTrackID, DisabledTrackMode aMode)
    : mTrackID(aTrackID), mMode(aMode) {}
  TrackID mTrackID;
  DisabledTrackMode mMode;
};

} // namespace mozilla

#endif // MOZILLA_MEDIASTREAMTYPES_h_
