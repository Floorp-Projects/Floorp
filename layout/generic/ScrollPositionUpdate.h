/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollPositionUpdate_h_
#define mozilla_ScrollPositionUpdate_h_

#include <cstdint>

#include "nsPoint.h"
#include "mozilla/ScrollOrigin.h"
#include "mozilla/ScrollTypes.h"
#include "Units.h"

namespace mozilla {

enum class ScrollUpdateType {
  // A scroll update to a specific destination, regardless of the current
  // scroll position.
  Absolute,
  // A scroll update by a specific amount, based off a given starting scroll
  // position. XXX Fold this into PureRelative, it should be relatively
  // straightforward after bug 1655733.
  Relative,
  // A scroll update by a specific amount, where only the delta is provided.
  // The delta should be applied to whatever the current scroll position is
  // on the receiver side.
  PureRelative,
};

/**
 * This class represents an update to the scroll position that is initiated by
 * something on the main thread. A list of these classes is accumulated by
 * scrollframes on the main thread, and the list is sent over as part of a
 * paint transaction to the compositor. The compositor can then iterate through
 * the scroll updates and apply/merge them with scrolling that has already
 * occurred independently on the compositor side.
 */
class ScrollPositionUpdate {
 public:
  // Constructor for IPC use only.
  explicit ScrollPositionUpdate();

  // Create a ScrollPositionUpdate for a newly created (or reconstructed)
  // scrollframe.
  static ScrollPositionUpdate NewScrollframe(uint32_t aGeneration,
                                             nsPoint aInitialPosition);
  // Create a ScrollPositionUpdate for a new absolute/instant scroll, to
  // the given destination.
  static ScrollPositionUpdate NewScroll(uint32_t aGeneration,
                                        ScrollOrigin aOrigin,
                                        nsPoint aDestination);
  // Create a ScrollPositionUpdate for a new relative/instant scroll, with
  // the given source/destination.
  static ScrollPositionUpdate NewRelativeScroll(uint32_t aGeneration,
                                                nsPoint aSource,
                                                nsPoint aDestination);
  // Create a ScrollPositionUpdate for a new absolute/smooth scroll, which
  // animates smoothly to the given destination from whatever the current
  // scroll position is in the receiver.
  static ScrollPositionUpdate NewSmoothScroll(uint32_t aGeneration,
                                              ScrollOrigin aOrigin,
                                              nsPoint aDestination);
  // Create a ScrollPositionUpdate for a new pure-relative scroll. The
  // aMode parameter controls whether or not this is a smooth animation or
  // instantaneous scroll.
  static ScrollPositionUpdate NewPureRelativeScroll(uint32_t aGeneration,
                                                    ScrollOrigin aOrigin,
                                                    ScrollMode aMode,
                                                    const nsPoint& aDelta);

  bool operator==(const ScrollPositionUpdate& aOther) const;

  uint32_t GetGeneration() const;
  ScrollUpdateType GetType() const;
  ScrollMode GetMode() const;
  ScrollOrigin GetOrigin() const;
  // GetDestination is only valid for Absolute and Relative types; it asserts
  // otherwise.
  CSSPoint GetDestination() const;
  // GetSource is only valid for the Relative type; it asserts otherwise.
  CSSPoint GetSource() const;
  // GetDelta is only valid for the PureRelative type; it asserts otherwise.
  CSSPoint GetDelta() const;

  void AppendToString(std::stringstream& aStream) const;

 private:
  uint32_t mScrollGeneration;
  // Refer to the ScrollUpdateType documentation for what the types mean.
  // All fields are populated for all types, except as noted below.
  ScrollUpdateType mType;
  ScrollMode mScrollMode;
  ScrollOrigin mScrollOrigin;
  // mDestination is not populated when mType == PureRelative.
  CSSPoint mDestination;
  // mSource is not populated when mType == Absolute || mType == PureRelative.
  CSSPoint mSource;
  // mDelta is not populated when mType == Absolute || mType == Relative.
  CSSPoint mDelta;
};

}  // namespace mozilla

#endif  // mozilla_ScrollPositionUpdate_h_
