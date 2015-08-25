/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
// your input events to update the internal state.
class TouchCounter
{
public:
  TouchCounter();
  void Update(const MultiTouchInput& aInput);
  uint32_t GetActiveTouchCount() const;

private:
  uint32_t mActiveTouchCount;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_TouchCounter_h */
