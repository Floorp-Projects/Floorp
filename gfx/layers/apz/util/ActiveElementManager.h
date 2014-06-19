/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ActiveElementManager_h
#define mozilla_layers_ActiveElementManager_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class inIDOMUtils;
class CancelableTask;

namespace mozilla {
namespace dom {
class Element;
class EventTarget;
}

namespace layers {

/**
 * Manages setting and clearing the ':active' CSS pseudostate in the presence
 * of touch input.
 */
class ActiveElementManager {
  ~ActiveElementManager();
public:
  NS_INLINE_DECL_REFCOUNTING(ActiveElementManager)

  ActiveElementManager();

  /**
   * Specify the target of a touch. Typically this should be called right
   * before HandleTouchStart(), but we give callers the flexibility to specify
   * the target later if they don't know it at the time they call
   * HandleTouchStart().
   * |aTarget| may be nullptr.
   */
  void SetTargetElement(dom::EventTarget* aTarget);
  /**
   * Handle a touch-start event.
   * @param aCanBePan whether the touch can be a pan
   */
  void HandleTouchStart(bool aCanBePan);
  /**
   * Handle the start of panning.
   */
  void HandlePanStart();
  /**
   * Handle a touch-end or touch-cancel event.
   * @param aWasClick whether the touch was a click
   */
  void HandleTouchEnd(bool aWasClick);
private:
  nsCOMPtr<inIDOMUtils> mDomUtils;
  /**
   * The target of the first touch point in the current touch block.
   */
  nsCOMPtr<dom::Element> mTarget;
  /**
   * Whether the current touch block can be a pan. Set in HandleTouchStart().
   */
  bool mCanBePan;
  /**
   * Whether mCanBePan has been set for the current touch block.
   * We need to keep track of this to allow HandleTouchStart() and
   * SetTargetElement() to be called in either order.
   */
  bool mCanBePanSet;
  /**
   * A task for calling SetActive() after a timeout.
   */
  CancelableTask* mSetActiveTask;

  // Helpers
  void TriggerElementActivation();
  void SetActive(dom::Element* aTarget);
  void ResetActive();
  void ResetTouchBlockState();
  void SetActiveTask(dom::Element* aTarget);
  void CancelTask();
};

}
}

#endif /* mozilla_layers_ActiveElementManager_h */
