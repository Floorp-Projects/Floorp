/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYLISTCLIPSTATE_H_
#define DISPLAYLISTCLIPSTATE_H_

#include "DisplayItemClip.h"

#include "mozilla/DebugOnly.h"

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

  /**
   * Returns intersection of mClipContainingBlockDescendants and
   * mClipContentDescendants, allocated on aBuilder's arena.
   */
  const DisplayItemClip* GetCurrentCombinedClip(nsDisplayListBuilder* aBuilder);

  const DisplayItemClip* GetClipForContainingBlockDescendants() const
  {
    return mClipContainingBlockDescendants;
  }
  const DisplayItemClip* GetClipForContentDescendants() const
  {
    return mClipContentDescendants;
  }

  class AutoSaveRestore;
  friend class AutoSaveRestore;

  class AutoClipContainingBlockDescendantsToContentBox;
  friend class AutoClipContainingBlockDescendantsToContentBox;

  class AutoClipMultiple;
  friend class AutoClipMultiple;

  enum {
    ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT = 0x01
  };

private:
  void SetClipForContainingBlockDescendants(const DisplayItemClip* aClip)
  {
    mClipContainingBlockDescendants = aClip;
    mCurrentCombinedClip = nullptr;
  }

  void Clear()
  {
    mClipContentDescendants = nullptr;
    mClipContainingBlockDescendants = nullptr;
    mCurrentCombinedClip = nullptr;
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendants(const nsRect& aRect,
                                      const nscoord* aRadii,
                                      DisplayItemClip& aClipOnStack);

  void ClipContentDescendants(const nsRect& aRect,
                              const nscoord* aRadii,
                              DisplayItemClip& aClipOnStack);
  void ClipContentDescendants(const nsRect& aRect,
                              const nsRect& aRoundedRect,
                              const nscoord* aRadii,
                              DisplayItemClip& aClipOnStack);

  /**
   * Clips containing-block descendants to the frame's content-box,
   * taking border-radius into account.
   * If aFlags contains ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT then
   * we assume display items will not draw outside the content rect, so
   * clipping is only required if there is a border-radius. This is an
   * optimization to reduce the amount of clipping required.
   */
  void ClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                  nsIFrame* aFrame,
                                                  DisplayItemClip& aClipOnStack,
                                                  uint32_t aFlags);

  /**
   * All content descendants (i.e. following placeholder frames to their
   * out-of-flows if necessary) should be clipped by mClipContentDescendants.
   * Null if no clipping applies.
   */
  const DisplayItemClip* mClipContentDescendants;
  /**
   * All containing-block descendants (i.e. frame descendants), including
   * display items for the current frame, should be clipped by
   * mClipContainingBlockDescendants.
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

/**
 * A class to automatically save and restore the current clip state. Also
 * offers methods for modifying the clip state. Only one modification is allowed
 * to be in scope at a time using one of these objects; multiple modifications
 * require nested objects. The interface is written this way to prevent
 * dangling pointers to DisplayItemClips.
 */
class DisplayListClipState::AutoSaveRestore {
public:
  explicit AutoSaveRestore(nsDisplayListBuilder* aBuilder);
  void Restore()
  {
    mState = mSavedState;
    mRestored = true;
  }
  ~AutoSaveRestore()
  {
    mState = mSavedState;
  }

  void Clear()
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.Clear();
    mClipUsed = false;
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendants(const nsRect& aRect,
                                      const nscoord* aRadii = nullptr)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
    mClipUsed = true;
    mState.ClipContainingBlockDescendants(aRect, aRadii, mClip);
  }

  void ClipContentDescendants(const nsRect& aRect,
                              const nscoord* aRadii = nullptr)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
    mClipUsed = true;
    mState.ClipContentDescendants(aRect, aRadii, mClip);
  }

  void ClipContentDescendants(const nsRect& aRect,
                              const nsRect& aRoundedRect,
                              const nscoord* aRadii = nullptr)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
    mClipUsed = true;
    mState.ClipContentDescendants(aRect, aRoundedRect, aRadii, mClip);
  }

  /**
   * Clips containing-block descendants to the frame's content-box,
   * taking border-radius into account.
   * If aFlags contains ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT then
   * we assume display items will not draw outside the content rect, so
   * clipping is only required if there is a border-radius. This is an
   * optimization to reduce the amount of clipping required.
   */
  void ClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                  nsIFrame* aFrame,
                                                  uint32_t aFlags = 0)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
    mClipUsed = true;
    mState.ClipContainingBlockDescendantsToContentBox(aBuilder, aFrame, mClip, aFlags);
  }

protected:
  DisplayListClipState& mState;
  DisplayListClipState mSavedState;
  DisplayItemClip mClip;
  DebugOnly<bool> mClipUsed;
  DebugOnly<bool> mRestored;
};

class DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox : public AutoSaveRestore {
public:
  AutoClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 uint32_t aFlags = 0)
    : AutoSaveRestore(aBuilder)
  {
    mClipUsed = true;
    mState.ClipContainingBlockDescendantsToContentBox(aBuilder, aFrame, mClip, aFlags);
  }
};

/**
 * Do not use this outside of nsFrame::BuildDisplayListForChild, use
 * multiple AutoSaveRestores instead. We provide this class just to ensure
 * BuildDisplayListForChild is as efficient as possible.
 */
class DisplayListClipState::AutoClipMultiple : public AutoSaveRestore {
public:
  explicit AutoClipMultiple(nsDisplayListBuilder* aBuilder)
    : AutoSaveRestore(aBuilder)
    , mExtraClipUsed(false)
  {}

  /**
   * *aClip must survive longer than this object. Be careful!!!
   */
  void SetClipForContainingBlockDescendants(const DisplayItemClip* aClip)
  {
    mState.SetClipForContainingBlockDescendants(aClip);
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendantsExtra(const nsRect& aRect,
                                           const nscoord* aRadii)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mExtraClipUsed, "mExtraClip already used");
    mExtraClipUsed = true;
    mState.ClipContainingBlockDescendants(aRect, aRadii, mExtraClip);
  }

protected:
  DisplayItemClip mExtraClip;
  DebugOnly<bool> mExtraClipUsed;
};

}

#endif /* DISPLAYLISTCLIPSTATE_H_ */
