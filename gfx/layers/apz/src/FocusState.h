/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FocusState_h
#define mozilla_layers_FocusState_h

#include <unordered_map>  // for std::unordered_map
#include <unordered_set>  // for std::unordered_set

#include "mozilla/layers/FocusTarget.h"          // for FocusTarget
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ViewID
#include "mozilla/Mutex.h"                       // for Mutex

namespace mozilla {
namespace layers {

/**
 * This class is used for tracking chrome and content focus targets and
 * calculating global focus information from them for use by APZCTreeManager
 * for async keyboard scrolling.
 *
 * # Calculating the element to scroll
 *
 * Chrome and content processes have independently focused elements. This makes
 * it difficult to calculate the global focused element and its scrollable
 * frame from the chrome or content side. So instead we send the local focus
 * information from each process to here and then calculate the global focus
 * information. This local information resides in a `focus target`.
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
 * To maintain deterministic focus changes for a given stream of user inputs,
 * we invalidate our focus state whenever we receive a user input that may
 * trigger event listeners. We then attach a new sequence number to these
 * events and dispatch them to content. Content will then include the latest
 * sequence number it has processed to every focus update. Using this we can
 * determine whether any potentially focus changing events have yet to be
 * handled by content.
 *
 * Once we have received the latest focus sequence number from content, we know
 * that all event listeners triggered by user inputs, and their resulting focus
 * changes, have been processed and so we have a current target that we can use
 * again.
 */
class FocusState final {
 public:
  FocusState();

  /**
   * The sequence number of the last potentially focus changing event processed
   * by APZ. This number starts at one and increases monotonically. This number
   * will never be zero as that is used to catch uninitialized focus sequence
   * numbers on input events.
   */
  uint64_t LastAPZProcessedEvent() const;

  /**
   * Notify focus state of a potentially focus changing event. This will
   * increment the current focus sequence number. The new value can be gotten
   * from LastAPZProcessedEvent().
   */
  void ReceiveFocusChangingEvent();

  /**
   * Update the internal focus tree and recalculate the global focus target for
   * a focus target update received from chrome or content.
   *
   * @param aRootLayerTreeId the layer tree ID of the root layer for the
                             parent APZCTreeManager
   * @param aOriginatingLayersId the layer tree ID that this focus target
                                 belongs to
   */
  void Update(LayersId aRootLayerTreeId, LayersId aOriginatingLayersId,
              const FocusTarget& aTarget);

  /**
   * Removes a focus target by its layer tree ID.
   */
  void RemoveFocusTarget(LayersId aLayersId);

  /**
   * Gets the scrollable layer that should be horizontally scrolled for a key
   * event, if any. The returned ScrollableLayerGuid doesn't contain a
   * presShellId, and so it should not be used in comparisons.
   *
   * No scrollable layer is returned if any of the following are true:
   *   1. We don't have a current focus target
   *   2. There are event listeners that could change the focus
   *   3. The target has not been layerized
   */
  Maybe<ScrollableLayerGuid> GetHorizontalTarget() const;
  /**
   * The same as GetHorizontalTarget() but for vertical scrolling.
   */
  Maybe<ScrollableLayerGuid> GetVerticalTarget() const;

  /**
   * Gets whether it is safe to not increment the focus sequence number for an
   * unmatched keyboard event.
   */
  bool CanIgnoreKeyboardShortcutMisses() const;

 private:
  /**
   * Whether the current focus state is known to be current or else if an event
   * has been processed that could change the focus but we have not received an
   * update with a new confirmed target.
   * This can only be called by methods that have already acquired mMutex; they
   * have to pass their lock as compile-time proof.
   */
  bool IsCurrent(const MutexAutoLock& aLock) const;

 private:
  // All methods should hold this lock, since this class is accessed via both
  // the updater and controller threads.
  mutable Mutex mMutex;

  // The set of focus targets received indexed by their layer tree ID
  std::unordered_map<LayersId, FocusTarget, LayersId::HashFn> mFocusTree;

  // The focus sequence number of the last potentially focus changing event
  // processed by APZ. This number starts at one and increases monotonically.
  // We don't worry about wrap around here because at a pace of 100
  // increments/sec, it would take 5.85*10^9 years before we would wrap around.
  // This number will never be zero as that is used to catch uninitialized focus
  // sequence numbers on input events.
  uint64_t mLastAPZProcessedEvent;
  // The focus sequence number last received in a focus update.
  uint64_t mLastContentProcessedEvent;

  // A flag whether there is a key listener on the event target chain for the
  // focused element
  bool mFocusHasKeyEventListeners;
  // A flag that is false until the first call to Update().
  bool mReceivedUpdate;

  // The layer tree ID which contains the scrollable frame of the focused
  // element
  LayersId mFocusLayersId;
  // The scrollable layer corresponding to the scrollable frame that is used to
  // scroll the focused element. This depends on the direction the user is
  // scrolling.
  ScrollableLayerGuid::ViewID mFocusHorizontalTarget;
  ScrollableLayerGuid::ViewID mFocusVerticalTarget;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_FocusState_h
