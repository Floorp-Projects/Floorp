/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DragTracker.h"

#include "InputData.h"

#define DRAG_LOG(...)
// #define DRAG_LOG(...) printf_stderr("DRAG: " __VA_ARGS__)

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
  // On Windows, we don't receive a MOUSE_UP at the end of a drag if an
  // actual drag session took place. As a backup, we detect the end of the
  // drag using the MOUSE_DRAG_END event, which normally is routed directly
  // to content, but we're specially routing to APZ for this purpose. Bug
  // 1265105 tracks a solution to this at the Windows widget layer; once
  // that is implemented, this workaround can be removed.
  return (aInput.IsLeftButton() && aInput.mType == MouseInput::MOUSE_UP)
      || aInput.mType == MouseInput::MOUSE_DRAG_END;
}

void
DragTracker::Update(const MouseInput& aInput)
{
  if (StartsDrag(aInput)) {
    DRAG_LOG("Starting drag\n");
    mInDrag = true;
  } else if (EndsDrag(aInput)) {
    DRAG_LOG("Ending drag\n");
    mInDrag = false;
    mOnScrollbar = Nothing();
  }
}

bool
DragTracker::InDrag() const
{
  return mInDrag;
}

bool
DragTracker::IsOnScrollbar(bool aOnScrollbar)
{
  if (!mOnScrollbar) {
    DRAG_LOG("Setting hitscrollbar %d\n", aOnScrollbar);
    mOnScrollbar = Some(aOnScrollbar);
  }
  return mOnScrollbar.value();
}

} // namespace layers
} // namespace mozilla
