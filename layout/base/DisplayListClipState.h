/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYLISTCLIPSTATE_H_
#define DISPLAYLISTCLIPSTATE_H_

#include "DisplayItemClip.h"
#include "DisplayItemScrollClip.h"

#include "mozilla/DebugOnly.h"

class nsIFrame;
class nsIScrollableFrame;
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
    , mScrollClipContentDescendants(nullptr)
    , mScrollClipContainingBlockDescendants(nullptr)
    , mClipContentDescendantsScrollClip(nullptr)
    , mStackingContextAncestorSC(nullptr)
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

  const DisplayItemScrollClip* GetCurrentInnermostScrollClip();

  const DisplayItemScrollClip* CurrentAncestorScrollClipForStackingContextContents()
  {
    return mStackingContextAncestorSC;
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

  void SetScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                                  const DisplayItemScrollClip* aScrollClip);

  void Clear()
  {
    mClipContentDescendants = nullptr;
    mClipContainingBlockDescendants = nullptr;
    mCurrentCombinedClip = nullptr;
    // We do not clear scroll clips.
  }

  void EnterStackingContextContents(bool aClear)
  {
    if (aClear) {
      mClipContentDescendants = nullptr;
      mClipContainingBlockDescendants = nullptr;
      mCurrentCombinedClip = nullptr;
      mScrollClipContentDescendants = nullptr;
      mScrollClipContainingBlockDescendants = nullptr;
      mStackingContextAncestorSC = nullptr;
    } else {
      mStackingContextAncestorSC = GetCurrentInnermostScrollClip();
    }
  }

  /**
   * Clear the current clip, and instead add it as a scroll clip to the current
   * scroll clip chain.
   */
  void TurnClipIntoScrollClipForContentDescendants(nsDisplayListBuilder* aBuilder,
                                                   nsIScrollableFrame* aScrollableFrame);
  void TurnClipIntoScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                                           nsIScrollableFrame* aScrollableFrame);

  /**
   * Insert a scroll clip without clearing the current clip.
   * The returned DisplayItemScrollClip will have mIsAsyncScrollable == false,
   * and it can be activated once the scroll frame knows that it needs to be
   * async scrollable.
   */
  DisplayItemScrollClip* InsertInactiveScrollClipForContentDescendants(nsDisplayListBuilder* aBuilder,
                                                                       nsIScrollableFrame* aScrollableFrame);
  DisplayItemScrollClip* InsertInactiveScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                                                               nsIScrollableFrame* aScrollableFrame);

  DisplayItemScrollClip* CreateInactiveScrollClip(nsDisplayListBuilder* aBuilder,
                                                  nsIScrollableFrame* aScrollableFrame);

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

  /**
   * The same for scroll clips.
   */
  const DisplayItemScrollClip* mScrollClipContentDescendants;
  const DisplayItemScrollClip* mScrollClipContainingBlockDescendants;

  /**
   * The scroll clip that was in effect when mClipContentDescendants was set.
   */
  const DisplayItemScrollClip* mClipContentDescendantsScrollClip;

  /**
   * A scroll clip that is an ancestor of all the scroll clips that were
   * "current" on this clip state since EnterStackingContextContents was
   * called.
   */
  const DisplayItemScrollClip* mStackingContextAncestorSC;
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
    if (!mClearedForStackingContextContents) {
      // Forward along the ancestor scroll clip to the original clip state.
      mSavedState.mStackingContextAncestorSC =
        DisplayItemScrollClip::PickAncestor(mSavedState.mStackingContextAncestorSC,
                                            mState.mStackingContextAncestorSC);
    }
    mState = mSavedState;
#ifdef DEBUG
    mRestored = true;
#endif
  }
  ~AutoSaveRestore()
  {
    Restore();
  }

  void Clear()
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.Clear();
#ifdef DEBUG
    mClipUsed = false;
#endif
  }

  void EnterStackingContextContents(bool aClear)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.EnterStackingContextContents(aClear);
    mClearedForStackingContextContents = aClear;
  }

  void ExitStackingContextContents(const DisplayItemScrollClip** aOutContainerSC)
  {
    if (mClearedForStackingContextContents) {
      // If we cleared the scroll clip, then the scroll clip that was current
      // just before we cleared it is the one that needs to be set on the
      // container item.
      *aOutContainerSC = mSavedState.GetCurrentInnermostScrollClip();
    } else {
      // If we didn't clear the scroll clip, then the container item needs to
      // get a scroll clip that's an ancestor of all its direct child items'
      // scroll clips.
      // The simplest way to satisfy this requirement would be to just take the
      // root scroll clip (i.e. nullptr). However, this can cause the bounds of
      // the container items to be enlarged unnecessarily, so instead we try to
      // take the "deepest" scroll clip that satisfies the requirement.
      // Usually this is the scroll clip that was current before we entered
      // the stacking context contents (call that the "initial scroll clip").
      // There are two cases in which the container scroll clip *won't* be the
      // initial scroll clip (instead the container scroll clip will be a
      // proper ancestor of the initial scroll clip):
      //  (1) If SetScrollClipForContainingBlockDescendants was called with an
      //      ancestor scroll clip of the initial scroll clip while we were
      //      building our direct child items. This happens if we entered a
      //      position:absolute or position:fixed element whose containing
      //      block is an ancestor of the frame that generated the initial
      //      scroll clip. Then the "ancestor scroll clip for stacking context
      //      contents" will be set to that scroll clip.
      //  (2) If one of our direct child items is a container item for which
      //      (1) or (2) happened.
      *aOutContainerSC = mState.CurrentAncestorScrollClipForStackingContextContents();
    }
    Restore();
  }

  bool SavedStateHasRoundedCorners()
  {
    const DisplayItemScrollClip* scrollClip = mSavedState.GetCurrentInnermostScrollClip();
    if (scrollClip && scrollClip->HasRoundedCorners()) {
      return true;
    }
    const DisplayItemClip* clip = mSavedState.GetClipForContainingBlockDescendants();
    if (clip && clip->GetRoundedRectCount() > 0) {
      return true;
    }

    clip = mSavedState.GetClipForContentDescendants();
    if (clip && clip->GetRoundedRectCount() > 0) {
      return true;
    }
    return false;
  }

  void TurnClipIntoScrollClipForContentDescendants(nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.TurnClipIntoScrollClipForContentDescendants(aBuilder, aScrollableFrame);
#ifdef DEBUG
    mClipUsed = true;
#endif
  }

  void TurnClipIntoScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.TurnClipIntoScrollClipForContainingBlockDescendants(aBuilder, aScrollableFrame);
#ifdef DEBUG
    mClipUsed = true;
#endif
  }

  DisplayItemScrollClip* InsertInactiveScrollClipForContentDescendants(nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    DisplayItemScrollClip* scrollClip = mState.InsertInactiveScrollClipForContentDescendants(aBuilder, aScrollableFrame);
#ifdef DEBUG
    mClipUsed = true;
#endif
    return scrollClip;
  }

  DisplayItemScrollClip* InsertInactiveScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    DisplayItemScrollClip* scrollClip = mState.InsertInactiveScrollClipForContainingBlockDescendants(aBuilder, aScrollableFrame);
#ifdef DEBUG
    mClipUsed = true;
#endif
    return scrollClip;
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
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContainingBlockDescendants(aRect, aRadii, mClip);
  }

  void ClipContentDescendants(const nsRect& aRect,
                              const nscoord* aRadii = nullptr)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContentDescendants(aRect, aRadii, mClip);
  }

  void ClipContentDescendants(const nsRect& aRect,
                              const nsRect& aRoundedRect,
                              const nscoord* aRadii = nullptr)
  {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
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
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContainingBlockDescendantsToContentBox(aBuilder, aFrame, mClip, aFlags);
  }

protected:
  DisplayListClipState& mState;
  DisplayListClipState mSavedState;
  DisplayItemClip mClip;
#ifdef DEBUG
  bool mClipUsed;
  bool mRestored;
#endif
  bool mClearedForStackingContextContents;
};

class DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox : public AutoSaveRestore {
public:
  AutoClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 uint32_t aFlags = 0)
    : AutoSaveRestore(aBuilder)
  {
#ifdef DEBUG
    mClipUsed = true;
#endif
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
#ifdef DEBUG
    , mExtraClipUsed(false)
#endif
  {}

  /**
   * *aClip must survive longer than this object. Be careful!!!
   */
  void SetClipForContainingBlockDescendants(const DisplayItemClip* aClip)
  {
    mState.SetClipForContainingBlockDescendants(aClip);
  }

  void SetScrollClipForContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                                  const DisplayItemScrollClip* aScrollClip)
  {
    mState.SetScrollClipForContainingBlockDescendants(aBuilder, aScrollClip);
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
#ifdef DEBUG
    mExtraClipUsed = true;
#endif
    mState.ClipContainingBlockDescendants(aRect, aRadii, mExtraClip);
  }

protected:
  DisplayItemClip mExtraClip;
#ifdef DEBUG
  bool mExtraClipUsed;
#endif
};

} // namespace mozilla

#endif /* DISPLAYLISTCLIPSTATE_H_ */
