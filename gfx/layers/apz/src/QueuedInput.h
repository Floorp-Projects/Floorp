/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_QueuedInput_h
#define mozilla_layers_QueuedInput_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class InputData;
class MultiTouchInput;
class ScrollWheelInput;
class MouseInput;
class PanGestureInput;
class KeyboardInput;

namespace layers {

class InputBlockState;
class TouchBlockState;
class WheelBlockState;
class DragBlockState;
class PanGestureBlockState;
class KeyboardBlockState;

/**
 * This lightweight class holds a pointer to an input event that has not yet
 * been completely processed, along with the input block that the input event
 * is associated with.
 */
class QueuedInput
{
public:
  QueuedInput(const MultiTouchInput& aInput, TouchBlockState& aBlock);
  QueuedInput(const ScrollWheelInput& aInput, WheelBlockState& aBlock);
  QueuedInput(const MouseInput& aInput, DragBlockState& aBlock);
  QueuedInput(const PanGestureInput& aInput, PanGestureBlockState& aBlock);
  QueuedInput(const KeyboardInput& aInput, KeyboardBlockState& aBlock);

  InputData* Input();
  InputBlockState* Block();

private:
  // A copy of the input event that is provided to the constructor. This must
  // be non-null, and is owned by this QueuedInput instance (hence the
  // UniquePtr).
  UniquePtr<InputData> mInput;
  // A pointer to the block that the input event is associated with. This must
  // be non-null.
  RefPtr<InputBlockState> mBlock;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_QueuedInput_h
