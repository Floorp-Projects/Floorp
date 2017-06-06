/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FocusState_h
#define mozilla_layers_FocusState_h

#include <unordered_map>    // for std::unordered_map
#include <unordered_set>    // for std::unordered_set

#include "FrameMetrics.h"   // for FrameMetrics::ViewID
#include "mozilla/layers/FocusTarget.h" // for FocusTarget

namespace mozilla {
namespace layers {

/**
 * This class is used for tracking chrome and content focus targets and calculating
 * global focus information from them for use by APZCTreeManager for async keyboard
 * scrolling.
 *
 * # Calculating the element to scroll
 *
 * Chrome and content processes have independently focused elements. This makes it
 * difficult to calculate the global focused element and its scrollable frame from
 * the chrome or content side. So instead we send the local focus information from
 * each process to here and then calculate the global focus information. This
 * local information resides in a `focus target`.
 *
 * A focus target indicates that either:
 *    1. The focused element is a remote browser along with its layer tree ID
 *    2. The focused element is not scrollable
 *    3. The focused element is scrollable along with the ViewID's of its
         scrollable layers
 *
 * Using this information we can determine the global focus information by
 * starting at the focus target of the root layer tree ID and following remote
 * browsers until we reach a scrollable or non-scrollable focus target.
 *
 * # Determinism and sequence numbers
 *
 * The focused element in content can be changed within any javascript code. And
 * javascript can run in response to an event or at any moment from `setTimeout`
 * and others. This makes it impossible to always have the current focus
 * information in APZ as it can be changed asynchronously at any moment. If we
 * don't have the latest focus information, we may incorrectly scroll a target
 * when we shouldn't.
 *
 * A tradeoff is designed here whereby we will maintain deterministic focus
 * changes for user input, but not for other javascript code. The reasoning
 * here is that `setTimeout` and others are already non-deterministic and so it
 * might not be as breaking to web content.
 *
 * To maintain deterministic focus changes for a given stream of user inputs, we
 * invalidate our focus state whenever we receive a user input that may trigger
 * event listeners. We then attach a new sequence number to these events and
 * dispatch them to content. Content will then include the latest sequence number
 * it has processed to every focus update. Using this we can determine whether
 * any potentially focus changing events have yet to be handled by content.
 *
 * Once we have received the latest focus sequence number from content, we know
 * that all event listeners triggered by user inputs, and their resulting focus
 * changes, have been processed and so we have a current target that we can use
 * again.
 */
class FocusState final
{
public:
  FocusState();

  /**
   * Update the internal focus tree and recalculate the global focus target for
   * a focus target update received from chrome or content.
   *
   * @param aRootLayerTreeId the layer tree ID of the root layer for the
                             parent APZCTreeManager
   * @param aOriginatingLayersId the layer tree ID that this focus target
                                 belongs to
   */
  void Update(uint64_t aRootLayerTreeId,
              uint64_t aOriginatingLayersId,
              const FocusTarget& aTarget);

  /**
   * Collects a set of the layer tree IDs that we have a focus target for.
   */
  std::unordered_set<uint64_t> GetFocusTargetLayerIds() const;

  /**
   * Removes a focus target by its layer tree ID.
   */
  void RemoveFocusTarget(uint64_t aLayersId);

private:
  // The set of focus targets received indexed by their layer tree ID
  std::unordered_map<uint64_t, FocusTarget> mFocusTree;

  // The layer tree ID which contains the scrollable frame of the focused element
  uint64_t mFocusLayersId;
  // The scrollable layer corresponding to the scrollable frame that is used to
  // scroll the focused element. This depends on the direction the user is
  // scrolling.
  FrameMetrics::ViewID mFocusHorizontalTarget;
  FrameMetrics::ViewID mFocusVerticalTarget;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_FocusState_h
