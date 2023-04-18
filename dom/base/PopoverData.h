/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PopoverData_h
#define mozilla_dom_PopoverData_h

#include "nsStringFwd.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsIWeakReferenceUtils.h"

namespace mozilla::dom {

// https://html.spec.whatwg.org/multipage/popover.html#attr-popover
enum class PopoverState : uint8_t {
  None,
  Auto,
  Manual,
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

  PopoverState GetPopoverState() const { return mState; }
  void SetPopoverState(PopoverState aState) { mState = aState; }

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

  bool HasPopoverInvoker() const { return mHasPopoverInvoker; }
  void SetHasPopoverInvoker(bool aHasPopoverInvoker) {
    mHasPopoverInvoker = aHasPopoverInvoker;
  }
  PopoverToggleEventTask* GetToggleEventTask() const { return mTask; }
  void SetToggleEventTask(PopoverToggleEventTask* aTask) { mTask = aTask; }
  void ClearToggleEventTask() { mTask = nullptr; }

 private:
  PopoverVisibilityState mVisibilityState = PopoverVisibilityState::Hidden;
  PopoverState mState = PopoverState::None;
  // Popover and dialog don't share mPreviouslyFocusedElement for there are
  // chances to lose the previously focused element.
  // See, https://github.com/whatwg/html/issues/9063
  nsWeakPtr mPreviouslyFocusedElement = nullptr;

  // https://html.spec.whatwg.org/multipage/popover.html#popover-invoker, also
  // see https://github.com/whatwg/html/issues/9168.
  bool mHasPopoverInvoker = false;
  RefPtr<PopoverToggleEventTask> mTask;
};
}  // namespace mozilla::dom

#endif
