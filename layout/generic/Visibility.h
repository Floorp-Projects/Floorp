/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Declares visibility-related types. @Visibility is an enumeration of the
 * possible visibility states of a frame. @OnNonvisible is an enumeration that
 * allows callers to request a specific action when a frame transitions from
 * visible to nonvisible.
 *
 * IPC serializers are available in VisibilityIPC.h.
 */

#ifndef mozilla_layout_generic_Visibility_h
#define mozilla_layout_generic_Visibility_h

namespace mozilla {

// Visibility states for frames.
enum class Visibility : uint8_t
{
  // We're not tracking visibility for this frame.
  UNTRACKED,

  // This frame is nonvisible - i.e., it was not within the displayport as of
  // the last paint (in which case it'd be IN_DISPLAYPORT) and our heuristics
  // aren't telling us that it may become visible soon (in which case it'd be
  // MAY_BECOME_VISIBLE).
  NONVISIBLE,

  // This frame is nonvisible now, but our heuristics tell us it may become
  // visible soon. These heuristics are updated on a relatively slow timer, so a
  // frame being marked MAY_BECOME_VISIBLE does not imply any particular
  // relationship between the frame and the displayport.
  MAY_BECOME_VISIBLE,

  // This frame was within the displayport as of the last paint. That doesn't
  // necessarily mean that the frame is visible - it may still lie outside the
  // viewport - but it does mean that the user may scroll the frame into view
  // asynchronously at any time (due to APZ), so for most purposes such a frame
  // should be treated as truly visible.
  IN_DISPLAYPORT
};

// The subset of the states in @Visibility which have a per-frame counter. This
// is used in the implementation of visibility tracking.
enum class VisibilityCounter : uint8_t
{
  MAY_BECOME_VISIBLE,
  IN_DISPLAYPORT
};

// Requested actions when frames transition to the nonvisible state.
enum class OnNonvisible : uint8_t
{
  DISCARD_IMAGES  // Discard images associated with the frame.
};

} // namespace mozilla

#endif // mozilla_layout_generic_Visibility_h
