/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYLISTCLIPSTATE_H_
#define DISPLAYLISTCLIPSTATE_H_

#include "DisplayItemClip.h"

class nsIFrame;
class nsDisplayListBuilder;

namespace mozilla {

/**
 * All clip coordinates are in appunits relative to the reference frame
 * for the display item we're building.
 */
class DisplayListClipState {
public:
  DisplayListClipState()
    : mClipContentDescendants(nullptr)
    , mClipContainingBlockDescendants(nullptr)
    , mCurrentCombinedClip(nullptr)
  {}

  const DisplayItemClip* GetCurrentCombinedClip(nsDisplayListBuilder* aBuilder);

  const DisplayItemClip* GetClipForContainingBlockDescendants()
  {
    return mClipContainingBlockDescendants;
  }

  const DisplayItemClip* GetClipForContentDescendants()
  {
    return mClipContentDescendants;
  }

  void SetClipForContainingBlockDescendants(const DisplayItemClip* aClip)
  {
    mClipContainingBlockDescendants = aClip;
    mCurrentCombinedClip = nullptr;
  }
  void SetClipForContentDescendants(const DisplayItemClip* aClip)
  {
    mClipContentDescendants = aClip;
    mCurrentCombinedClip = nullptr;
  }

private:
  /**
   * All content descendants (i.e. following placeholder frames to their
   * out-of-flows if necessary) should be clipped by mClipContentDescendants.
   * Null if no clipping applies.
   */
  const DisplayItemClip* mClipContentDescendants;
  /**
   * All containing-block descendants (i.e. frame descendants) should be
   * clipped by mClipContainingBlockDescendants.
   * Null if no clipping applies.
   */
  const DisplayItemClip* mClipContainingBlockDescendants;
  /**
   * The intersection of mClipContentDescendants and
   * mClipContainingBlockDescendants.
   * Allocated in the nsDisplayListBuilder arena. Null if none has been
   * allocated or both mClipContentDescendants and mClipContainingBlockDescendants
   * are null.
   */
  const DisplayItemClip* mCurrentCombinedClip;
};

}

#endif /* DISPLAYLISTCLIPSTATE_H_ */
