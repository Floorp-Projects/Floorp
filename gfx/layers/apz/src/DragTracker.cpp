/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DragTracker.h"

#include "InputData.h"

namespace mozilla {
namespace layers {

DragTracker::DragTracker()
  : mInDrag(false)
{
}

/*static*/ bool
DragTracker::StartsDrag(const MouseInput& aInput)
{
  return aInput.IsLeftButton() && aInput.mType == MouseInput::MOUSE_DOWN;
}

/*static*/ bool
DragTracker::EndsDrag(const MouseInput& aInput)
{
  return aInput.IsLeftButton() && aInput.mType == MouseInput::MOUSE_UP;
}

void
DragTracker::Update(const MouseInput& aInput)
{
  if (StartsDrag(aInput)) {
    mInDrag = true;
  } else if (EndsDrag(aInput)) {
    mInDrag = false;
  }
}

bool
DragTracker::InDrag() const
{
  return mInDrag;
}

} // namespace layers
} // namespace mozilla
