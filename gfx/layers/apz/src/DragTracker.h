/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_DragTracker_h
#define mozilla_layers_DragTracker_h

#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"

namespace mozilla {

class MouseInput;

namespace layers {

// DragTracker simply tracks a sequence of mouse inputs and allows us to tell
// if we are in a drag or not (i.e. the left mouse button went down and hasn't
// gone up yet).
class DragTracker
{
public:
  DragTracker();
  static bool StartsDrag(const MouseInput& aInput);
  static bool EndsDrag(const MouseInput& aInput);
  void Update(const MouseInput& aInput);
  bool InDrag() const;
  bool IsOnScrollbar(bool aOnScrollbar);

private:
  Maybe<bool> mOnScrollbar;
  bool mInDrag;
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_DragTracker_h */
