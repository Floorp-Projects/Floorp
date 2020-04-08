/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DISPLAYLISTCLIPSTATE_H_
#define DISPLAYLISTCLIPSTATE_H_

#include "DisplayItemClip.h"
#include "DisplayItemClipChain.h"

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
      : mClipChainContentDescendants(nullptr),
        mClipChainContainingBlockDescendants(nullptr),
        mCurrentCombinedClipChain(nullptr),
        mCurrentCombinedClipChainIsValid(false),
        mClippedToDisplayPort(false) {}

  void SetClippedToDisplayPort() { mClippedToDisplayPort = true; }
  bool IsClippedToDisplayPort() const { return mClippedToDisplayPort; }

  /**
   * Returns intersection of mClipChainContainingBlockDescendants and
   * mClipChainContentDescendants, allocated on aBuilder's arena.
   */
  const DisplayItemClipChain* GetCurrentCombinedClipChain(
      nsDisplayListBuilder* aBuilder);

  const DisplayItemClipChain* GetClipChainForContainingBlockDescendants()
      const {
    return mClipChainContainingBlockDescendants;
  }
  const DisplayItemClipChain* GetClipChainForContentDescendants() const {
    return mClipChainContentDescendants;
  }

  const ActiveScrolledRoot* GetContentClipASR() const {
    return mClipChainContentDescendants ? mClipChainContentDescendants->mASR
                                        : nullptr;
  }

  class AutoSaveRestore;

  class AutoClipContainingBlockDescendantsToContentBox;

  class AutoClipMultiple;

  enum { ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT = 0x01 };

 private:
  void Clear() {
    mClipChainContentDescendants = nullptr;
    mClipChainContainingBlockDescendants = nullptr;
    mCurrentCombinedClipChain = nullptr;
    mCurrentCombinedClipChainIsValid = false;
    mClippedToDisplayPort = false;
  }

  void SetClipChainForContainingBlockDescendants(
      const DisplayItemClipChain* aClipChain) {
    mClipChainContainingBlockDescendants = aClipChain;
    InvalidateCurrentCombinedClipChain(aClipChain ? aClipChain->mASR : nullptr);
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                      const nsRect& aRect,
                                      const nscoord* aRadii,
                                      DisplayItemClipChain& aClipChainOnStack);

  void ClipContentDescendants(nsDisplayListBuilder* aBuilder,
                              const nsRect& aRect, const nscoord* aRadii,
                              DisplayItemClipChain& aClipChainOnStack);
  void ClipContentDescendants(nsDisplayListBuilder* aBuilder,
                              const nsRect& aRect, const nsRect& aRoundedRect,
                              const nscoord* aRadii,
                              DisplayItemClipChain& aClipChainOnStack);

  void InvalidateCurrentCombinedClipChain(
      const ActiveScrolledRoot* aInvalidateUpTo);

  /**
   * Clips containing-block descendants to the frame's content-box,
   * taking border-radius into account.
   * If aFlags contains ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT then
   * we assume display items will not draw outside the content rect, so
   * clipping is only required if there is a border-radius. This is an
   * optimization to reduce the amount of clipping required.
   */
  void ClipContainingBlockDescendantsToContentBox(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
      DisplayItemClipChain& aClipChainOnStack, uint32_t aFlags);

  /**
   * All content descendants (i.e. following placeholder frames to their
   * out-of-flows if necessary) should be clipped by
   * mClipChainContentDescendants. Null if no clipping applies.
   */
  const DisplayItemClipChain* mClipChainContentDescendants;
  /**
   * All containing-block descendants (i.e. frame descendants), including
   * display items for the current frame, should be clipped by
   * mClipChainContainingBlockDescendants.
   * Null if no clipping applies.
   */
  const DisplayItemClipChain* mClipChainContainingBlockDescendants;
  /**
   * The intersection of mClipChainContentDescendants and
   * mClipChainContainingBlockDescendants.
   * Allocated in the nsDisplayListBuilder arena. Null if none has been
   * allocated or both mClipChainContentDescendants and
   * mClipChainContainingBlockDescendants are null.
   */
  const DisplayItemClipChain* mCurrentCombinedClipChain;
  bool mCurrentCombinedClipChainIsValid;
  /**
   * A flag that is used by sticky positioned items to know if the clip applied
   * to them is just the displayport clip or if there is additional clipping.
   */
  bool mClippedToDisplayPort;
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
  void Restore() {
    mState = mSavedState;
#ifdef DEBUG
    mRestored = true;
#endif
  }
  ~AutoSaveRestore() { Restore(); }

  void Clear() {
    NS_ASSERTION(!mRestored, "Already restored!");
    mState.Clear();
#ifdef DEBUG
    mClipUsed = false;
#endif
  }

  void SetClipChainForContainingBlockDescendants(
      const DisplayItemClipChain* aClipChain) {
    mState.SetClipChainForContainingBlockDescendants(aClipChain);
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendants(const nsRect& aRect,
                                      const nscoord* aRadii = nullptr) {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContainingBlockDescendants(mBuilder, aRect, aRadii, mClipChain);
  }

  void ClipContentDescendants(const nsRect& aRect,
                              const nscoord* aRadii = nullptr) {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContentDescendants(mBuilder, aRect, aRadii, mClipChain);
  }

  void ClipContentDescendants(const nsRect& aRect, const nsRect& aRoundedRect,
                              const nscoord* aRadii = nullptr) {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContentDescendants(mBuilder, aRect, aRoundedRect, aRadii,
                                  mClipChain);
  }

  /**
   * Clips containing-block descendants to the frame's content-box,
   * taking border-radius into account.
   * If aFlags contains ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT then
   * we assume display items will not draw outside the content rect, so
   * clipping is only required if there is a border-radius. This is an
   * optimization to reduce the amount of clipping required.
   */
  void ClipContainingBlockDescendantsToContentBox(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, uint32_t aFlags = 0) {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mClipUsed, "mClip already used");
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContainingBlockDescendantsToContentBox(aBuilder, aFrame,
                                                      mClipChain, aFlags);
  }

  void SetClippedToDisplayPort() { mState.SetClippedToDisplayPort(); }
  bool IsClippedToDisplayPort() const {
    return mState.IsClippedToDisplayPort();
  }

 protected:
  nsDisplayListBuilder* mBuilder;
  DisplayListClipState& mState;
  DisplayListClipState mSavedState;
  DisplayItemClipChain mClipChain;
#ifdef DEBUG
  bool mClipUsed;
  bool mRestored;
#endif
};

class DisplayListClipState::AutoClipContainingBlockDescendantsToContentBox
    : public AutoSaveRestore {
 public:
  AutoClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 uint32_t aFlags = 0)
      : AutoSaveRestore(aBuilder) {
#ifdef DEBUG
    mClipUsed = true;
#endif
    mState.ClipContainingBlockDescendantsToContentBox(aBuilder, aFrame,
                                                      mClipChain, aFlags);
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
        ,
        mExtraClipUsed(false)
#endif
  {
  }

  /**
   * Intersects the given clip rect (with optional aRadii) with the current
   * mClipContainingBlockDescendants and sets mClipContainingBlockDescendants to
   * the result, stored in aClipOnStack.
   */
  void ClipContainingBlockDescendantsExtra(const nsRect& aRect,
                                           const nscoord* aRadii) {
    NS_ASSERTION(!mRestored, "Already restored!");
    NS_ASSERTION(!mExtraClipUsed, "mExtraClip already used");
#ifdef DEBUG
    mExtraClipUsed = true;
#endif
    mState.ClipContainingBlockDescendants(mBuilder, aRect, aRadii,
                                          mExtraClipChain);
  }

 protected:
  DisplayItemClipChain mExtraClipChain;
#ifdef DEBUG
  bool mExtraClipUsed;
#endif
};

}  // namespace mozilla

#endif /* DISPLAYLISTCLIPSTATE_H_ */
