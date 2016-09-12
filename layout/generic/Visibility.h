/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Declares visibility-related types. @Visibility is an enumeration of the
 * possible visibility states of a frame. @OnNonvisible is an enumeration that
 * allows callers to request a specific action when a frame transitions from
 * visible to nonvisible.
 */

#ifndef mozilla_layout_generic_Visibility_h
#define mozilla_layout_generic_Visibility_h

namespace mozilla {

// Visibility states for frames.
enum class Visibility : uint8_t
{
  // Indicates that we're not tracking visibility for this frame.
  UNTRACKED,

  // Indicates that the frame is probably nonvisible. Visible frames *may* be
  // APPROXIMATELY_NONVISIBLE because approximate visibility is not updated
  // synchronously. Some truly nonvisible frames may be marked
  // APPROXIMATELY_VISIBLE instead if our heuristics lead us to think they may
  // be visible soon.
  APPROXIMATELY_NONVISIBLE,

  // Indicates that the frame is either visible now or is likely to be visible
  // soon according to our heuristics. As with APPROXIMATELY_NONVISIBLE, it's
  // important to note that approximately visibility is not updated
  // synchronously, so this information may be out of date.
  APPROXIMATELY_VISIBLE
};

// Requested actions when frames transition to the nonvisible state.
enum class OnNonvisible : uint8_t
{
  DISCARD_IMAGES  // Discard images associated with the frame.
};

} // namespace mozilla

#endif // mozilla_layout_generic_Visibility_h
