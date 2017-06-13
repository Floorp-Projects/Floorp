/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FocusTarget_h
#define mozilla_layers_FocusTarget_h

#include <stdint.h> // for int32_t, uint32_t

#include "FrameMetrics.h" // for FrameMetrics::ViewID

class nsIPresShell;

namespace mozilla {
namespace layers {

/**
 * This class is used for communicating information about the currently focused
 * element of a document and the scrollable frames to use when keyboard scrolling
 * it. It is created on the main thread at paint-time, but is then passed over
 * IPC to the compositor/APZ code.
 */
class FocusTarget final
{
public:
  struct ScrollTargets
  {
    FrameMetrics::ViewID mHorizontal;
    FrameMetrics::ViewID mVertical;
  };

  enum FocusTargetType
  {
    eNone,
    eRefLayer,
    eScrollLayer,

    // Used as an upper bound for ContiguousEnumSerializer
    eSentinel,
  };
  union FocusTargetData
  {
    uint64_t      mRefLayerId;
    ScrollTargets mScrollTargets;
  };

  FocusTarget();

  /**
   * Construct a focus target for the specified top level PresShell
   */
  FocusTarget(nsIPresShell* aRootPresShell);

  bool operator==(const FocusTarget& aRhs) const;

public:
  // Whether there are keydown, keypress, or keyup event listeners
  // in the event target chain of the focused element
  bool mFocusHasKeyEventListeners;

  FocusTargetType mType;
  FocusTargetData mData;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_FocusTarget_h
