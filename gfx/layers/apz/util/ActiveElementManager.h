/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ActiveElementManager_h
#define mozilla_layers_ActiveElementManager_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "mozilla/EnumSet.h"

namespace mozilla {

class CancelableRunnable;

namespace dom {
class Element;
class EventTarget;
}  // namespace dom

namespace layers {

class DelayedClearElementActivation;

namespace apz {
enum class SingleTapState : uint8_t;
}  // namespace apz

/**
 * Manages setting and clearing the ':active' CSS pseudostate in the presence
 * of touch input.
 */
class ActiveElementManager final {
  ~ActiveElementManager();

 public:
  NS_INLINE_DECL_REFCOUNTING(ActiveElementManager)

  ActiveElementManager();

  /**
   * Specify the target of a touch. Typically this should be called right
   * after HandleTouchStart(), but in cases where the APZ needs to wait for
   * a content response the HandleTouchStart() may be delayed, in which case
   * this function can be called first.
   * |aTarget| may be nullptr.
   */
  void SetTargetElement(dom::EventTarget* aTarget);
  /**
   * Handle a touch-start state notification from APZ. This notification
   * may be delayed until after touch listeners have responded to the APZ.
   * @param aCanBePanOrZoom whether the touch can be a pan or double-tap-to-zoom
   */
  void HandleTouchStart(bool aCanBePanOrZoom);
  /**
   * Clear the active element.
   */
  void ClearActivation();
  /**
   * Handle a touch-end or touch-cancel event.
   * @param aWasClick whether the touch was a click
   */
  bool HandleTouchEndEvent(apz::SingleTapState aState);
  /**
   * Handle a touch-end state notification from APZ. This notification may be
   * delayed until after touch listeners have responded to the APZ.
   */
  bool HandleTouchEnd(apz::SingleTapState aState);
  /**
   * Possibly clear active element sate in response to a single tap.
   */
  void ProcessSingleTap();
  /**
   * Cleanup on window destroy.
   */
  void Destroy();

 private:
  /**
   * The target of the first touch point in the current touch block.
   */
  RefPtr<dom::Element> mTarget;
  /**
   * Whether the current touch block can be a pan or double-tap-to-zoom. Set in
   * HandleTouchStart().
   */
  bool mCanBePanOrZoom;
  /**
   * Whether mCanBePanOrZoom has been set for the current touch block.
   * We need to keep track of this to allow HandleTouchStart() and
   * SetTargetElement() to be called in either order.
   */
  bool mCanBePanOrZoomSet;

  bool mSingleTapBeforeActivation;

  enum class TouchEndState : uint8_t {
    GotTouchEndNotification,
    GotTouchEndEvent,
  };
  using TouchEndStates = EnumSet<TouchEndState>;

  /**
   * A flag tracks whether `APZStateChange::eEndTouch` notification has arrived
   * and whether `eTouchEnd` event has arrived.
   */
  TouchEndStates mTouchEndState;

  /**
   * A tri-state variable to represent the single tap state when both of
   * `APZStateChange::eEndTouch` notification and `eTouchEnd` event arrived.
   */
  apz::SingleTapState mSingleTapState;

  /**
   * A task for calling SetActive() after a timeout.
   */
  RefPtr<CancelableRunnable> mSetActiveTask;

  // Store the pending single tap event element activation clearing
  // task.
  RefPtr<DelayedClearElementActivation> mDelayedClearElementActivation;

  // Helpers
  void TriggerElementActivation();
  void SetActive(dom::Element* aTarget);
  void ResetActive();
  void ResetTouchBlockState();
  void SetActiveTask(const nsCOMPtr<dom::Element>& aTarget);
  void CancelTask();
  // Returns true if the function changed the active element state.
  bool MaybeChangeActiveState(apz::SingleTapState aState);
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_ActiveElementManager_h */
