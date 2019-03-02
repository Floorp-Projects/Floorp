/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_TouchCounter_h
#define mozilla_layers_TouchCounter_h

#include "mozilla/EventForwards.h"

namespace mozilla {

class MultiTouchInput;

namespace layers {

// TouchCounter simply tracks the number of active touch points. Feed it
// your input events to update the internal state. Generally you should
// only be calling one of the Update functions, depending on which type
// of touch inputs you have access to.
class TouchCounter {
 public:
  TouchCounter();
  void Update(const MultiTouchInput& aInput);
  void Update(const WidgetTouchEvent& aEvent);
  uint32_t GetActiveTouchCount() const;

 private:
  uint32_t mActiveTouchCount;
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_TouchCounter_h */
