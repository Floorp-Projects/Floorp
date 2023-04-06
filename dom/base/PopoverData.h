/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PopoverData_h
#define mozilla_dom_PopoverData_h

#include "nsStringFwd.h"

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

 private:
  PopoverVisibilityState mVisibilityState = PopoverVisibilityState::Hidden;
  PopoverState mState = PopoverState::None;
  // Popover and dialog don't share mPreviouslyFocusedElement for there are
  // chances to lose the previously focused element.
  // See, https://github.com/whatwg/html/issues/9063
  nsWeakPtr mPreviouslyFocusedElement = nullptr;
};
}  // namespace mozilla::dom

#endif
