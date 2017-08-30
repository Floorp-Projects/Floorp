/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 #ifndef MOZILLA_TRACK_ID_H_
 #define MOZILLA_TRACK_ID_H_

 namespace mozilla {

 /**
  * Unique ID for track within a StreamTracks. Tracks from different
  * StreamTrackss may have the same ID; this matters when appending StreamTrackss,
  * since tracks with the same ID are matched. Only IDs greater than 0 are allowed.
  */
 typedef int32_t TrackID;
 const TrackID TRACK_NONE = 0;
 const TrackID TRACK_INVALID = -1;
 const TrackID TRACK_ANY = -2;

 inline bool IsTrackIDExplicit(const TrackID& aId) {
   return aId > TRACK_NONE;
 }

} // namespace mozilla

#endif // MOZILLA_TRACK_ID_H_
