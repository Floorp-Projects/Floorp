/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayListClipState.h"

#include "nsDisplayList.h"

namespace mozilla {

const DisplayItemClip*
DisplayListClipState::GetCurrentCombinedClip(nsDisplayListBuilder* aBuilder)
{
  if (mCurrentCombinedClip) {
    return mCurrentCombinedClip;
  }
  if (!mClipContentDescendants && !mClipContainingBlockDescendants) {
    return nullptr;
  }
  void* mem = aBuilder->Allocate(sizeof(DisplayItemClip));
  if (mClipContentDescendants) {
    mCurrentCombinedClip =
      new (mem) DisplayItemClip(*mClipContentDescendants);
    if (mClipContainingBlockDescendants) {
      mCurrentCombinedClip->IntersectWith(*mClipContainingBlockDescendants);
    }
  } else {
    mCurrentCombinedClip =
      new (mem) DisplayItemClip(*mClipContainingBlockDescendants);
  }
  return mCurrentCombinedClip;
}

}
