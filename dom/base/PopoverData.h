/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PopoverData_h
#define mozilla_dom_PopoverData_h

#include "Element.h"
#include "nsINode.h"
#include "nsIRunnable.h"
#include "nsIWeakReferenceUtils.h"
#include "nsStringFwd.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

// https://html.spec.whatwg.org/#attr-popover
enum class PopoverAttributeState : uint8_t {
  None,
  Auto,    ///< https://html.spec.whatwg.org/#attr-popover-auto-state
  Manual,  ///< https://html.spec.whatwg.org/#attr-popover-manual-state
};

enum class PopoverVisibilityState : uint8_t {
  Hidden,
  Showing,
};

class PopoverToggleEventTask : public Runnable {
 public:
  explicit PopoverToggleEventTask(nsWeakPtr aElement,
                                  PopoverVisibilityState aOldState);

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override;

  PopoverVisibilityState GetOldState() const { return mOldState; }

 private:
  nsWeakPtr mElement;
  PopoverVisibilityState mOldState;
};

class PopoverData {
 public:
  PopoverData() = default;
  ~PopoverData() = default;

  PopoverAttributeState GetPopoverAttributeState() const { return mState; }
  void SetPopoverAttributeState(PopoverAttributeState aState) {
    mState = aState;
  }

  PopoverVisibilityState GetPopoverVisibilityState() const {
    return mVisibilityState;
  }
  void SetPopoverVisibilityState(PopoverVisibilityState aVisibilityState) {
    mVisibilityState = aVisibilityState;
  }

  nsWeakPtr GetPreviouslyFocusedElement() const {
    return mPreviouslyFocusedElement;
  }
  void SetPreviouslyFocusedElement(nsWeakPtr aPreviouslyFocusedElement) {
    mPreviouslyFocusedElement = aPreviouslyFocusedElement;
  }

  RefPtr<Element> GetInvoker() const {
    return do_QueryReferent(mInvokerElement);
  }
  void SetInvoker(Element* aInvokerElement) {
    mInvokerElement =
        do_GetWeakReference(static_cast<nsINode*>(aInvokerElement));
  }

  PopoverToggleEventTask* GetToggleEventTask() const { return mTask; }
  void SetToggleEventTask(PopoverToggleEventTask* aTask) { mTask = aTask; }
  void ClearToggleEventTask() { mTask = nullptr; }

  bool IsShowingOrHiding() const { return mIsShowingOrHiding; }
  void SetIsShowingOrHiding(bool aIsShowingOrHiding) {
    mIsShowingOrHiding = aIsShowingOrHiding;
  }

 private:
  PopoverVisibilityState mVisibilityState = PopoverVisibilityState::Hidden;
  PopoverAttributeState mState = PopoverAttributeState::None;
  // Popover and dialog don't share mPreviouslyFocusedElement for there are
  // chances to lose the previously focused element.
  // See, https://github.com/whatwg/html/issues/9063
  nsWeakPtr mPreviouslyFocusedElement = nullptr;

  // https://html.spec.whatwg.org/#popover-invoker
  // Since having a popover invoker only makes a difference if the invoker
  // is in the document (in another open popover to be precise) we can make
  // this a weak reference, as if the element goes away it's necessarily not
  // connected to our document.
  nsWeakPtr mInvokerElement;
  bool mIsShowingOrHiding = false;
  RefPtr<PopoverToggleEventTask> mTask;
};
}  // namespace mozilla::dom

#endif
