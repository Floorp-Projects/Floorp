/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RestyleManager_h
#define mozilla_RestyleManager_h

#include "mozilla/OverflowChangedTracker.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"

class nsCString;
class nsCSSFrameConstructor;
class nsStyleChangeList;

namespace mozilla {

class EventStates;
class GeckoRestyleManager;
class ServoRestyleManager;

namespace dom {
class Element;
}

/**
 * Class for sharing data and logic common to both GeckoRestyleManager and
 * ServoRestyleManager.
 */
class RestyleManager
{
protected:
  RestyleManager(StyleBackendType aType, nsPresContext* aPresContext);

public:
  typedef mozilla::dom::Element Element;

  // Get an integer that increments every time we process pending restyles.
  // The value is never 0.
  uint32_t GetRestyleGeneration() const { return mRestyleGeneration; }

  // Get an integer that increments every time there is a style change
  // as a result of a change to the :hover content state.
  uint32_t GetHoverGeneration() const { return mHoverGeneration; }

  bool ObservingRefreshDriver() const { return mObservingRefreshDriver; }

  void SetObservingRefreshDriver(bool aObserving) {
      mObservingRefreshDriver = aObserving;
  }

  void Disconnect() { mPresContext = nullptr; }

  static nsCString RestyleHintToString(nsRestyleHint aHint);

#ifdef DEBUG
  static nsCString ChangeHintToString(nsChangeHint aHint);

  /**
   * DEBUG ONLY method to verify integrity of style tree versus frame tree
   */
  void DebugVerifyStyleTree(nsIFrame* aFrame);
#endif

  void FlushOverflowChangedTracker() {
    mOverflowChangedTracker.Flush();
  }

  // Should be called when a frame is going to be destroyed and
  // WillDestroyFrameTree hasn't been called yet.
  void NotifyDestroyingFrame(nsIFrame* aFrame) {
    mOverflowChangedTracker.RemoveFrame(aFrame);
  }

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessRestyledFrames call in a view update batch and a script blocker.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  nsresult ProcessRestyledFrames(nsStyleChangeList& aChangeList);

  bool IsInStyleRefresh() const { return mInStyleRefresh; }

  // AnimationsWithDestroyedFrame is used to stop animations and transitions
  // on elements that have no frame at the end of the restyling process.
  // It only lives during the restyling process.
  class MOZ_STACK_CLASS AnimationsWithDestroyedFrame final {
  public:
    // Construct a AnimationsWithDestroyedFrame object.  The caller must
    // ensure that aRestyleManager lives at least as long as the
    // object.  (This is generally easy since the caller is typically a
    // method of RestyleManager.)
    explicit AnimationsWithDestroyedFrame(RestyleManager* aRestyleManager);

    // This method takes the content node for the generated content for
    // animation/transition on ::before and ::after, rather than the
    // content node for the real element.
    void Put(nsIContent* aContent, nsStyleContext* aStyleContext) {
      MOZ_ASSERT(aContent);
      CSSPseudoElementType pseudoType = aStyleContext->GetPseudoType();
      if (pseudoType == CSSPseudoElementType::NotPseudo) {
        mContents.AppendElement(aContent);
      } else if (pseudoType == CSSPseudoElementType::before) {
        MOZ_ASSERT(aContent->NodeInfo()->NameAtom() ==
                     nsGkAtoms::mozgeneratedcontentbefore);
        mBeforeContents.AppendElement(aContent->GetParent());
      } else if (pseudoType == CSSPseudoElementType::after) {
        MOZ_ASSERT(aContent->NodeInfo()->NameAtom() ==
                     nsGkAtoms::mozgeneratedcontentafter);
        mAfterContents.AppendElement(aContent->GetParent());
      }
    }

    void StopAnimationsForElementsWithoutFrames();

  private:
    void StopAnimationsWithoutFrame(nsTArray<RefPtr<nsIContent>>& aArray,
                                    CSSPseudoElementType aPseudoType);

    RestyleManager* mRestyleManager;
    AutoRestore<AnimationsWithDestroyedFrame*> mRestorePointer;

    // Below three arrays might include elements that have already had their
    // animations or transitions stopped.
    //
    // mBeforeContents and mAfterContents hold the real element rather than
    // the content node for the generated content (which might change during
    // a reframe)
    nsTArray<RefPtr<nsIContent>> mContents;
    nsTArray<RefPtr<nsIContent>> mBeforeContents;
    nsTArray<RefPtr<nsIContent>> mAfterContents;
  };

  /**
   * Return the current AnimationsWithDestroyedFrame struct, or null if we're
   * not currently in a restyling operation.
   */
  AnimationsWithDestroyedFrame* GetAnimationsWithDestroyedFrame() {
    return mAnimationsWithDestroyedFrame;
  }

protected:
  ~RestyleManager() {
    MOZ_ASSERT(!mAnimationsWithDestroyedFrame,
               "leaving dangling pointers from AnimationsWithDestroyedFrame");
  }

  void ContentStateChangedInternal(Element* aElement,
                                   EventStates aStateMask,
                                   nsChangeHint* aOutChangeHint,
                                   nsRestyleHint* aOutRestyleHint);

  bool IsDisconnected() { return mPresContext == nullptr; }

  void IncrementHoverGeneration() {
    ++mHoverGeneration;
  }

  void IncrementRestyleGeneration() {
    if (++mRestyleGeneration == 0) {
      // Keep mRestyleGeneration from being 0, since that's what
      // nsPresContext::GetRestyleGeneration returns when it no
      // longer has a RestyleManager.
      ++mRestyleGeneration;
    }
  }

  nsPresContext* PresContext() const {
    MOZ_ASSERT(mPresContext);
    return mPresContext;
  }

  nsCSSFrameConstructor* FrameConstructor() const {
    return PresContext()->FrameConstructor();
  }

  MOZ_DECL_STYLO_METHODS(GeckoRestyleManager, ServoRestyleManager)

private:
  nsPresContext* mPresContext; // weak, can be null after Disconnect().
  uint32_t mRestyleGeneration;
  uint32_t mHoverGeneration;

  const StyleBackendType mType;

  // True if we're already waiting for a refresh notification.
  bool mObservingRefreshDriver;

protected:
  // True if we're in the middle of a nsRefreshDriver refresh
  bool mInStyleRefresh;

  OverflowChangedTracker mOverflowChangedTracker;

  void PostRestyleEventInternal(bool aForLazyConstruction);

  /**
   * These are protected static methods that help with the change hint
   * processing bits of the restyle managers.
   */
  static nsIFrame*
  GetNearestAncestorFrame(nsIContent* aContent);

  static nsIFrame*
  GetNextBlockInInlineSibling(FramePropertyTable* aPropTable, nsIFrame* aFrame);

  /**
   * Get the next continuation or similar ib-split sibling (assuming
   * block/inline alternation), conditionally on it having the same style.
   *
   * Since this is used when deciding to copy the new style context, it
   * takes as an argument the old style context to check if the style is
   * the same.  When it is used in other contexts (i.e., where the next
   * continuation would already have the new style context), the current
   * style context should be passed.
   */
  static nsIFrame*
  GetNextContinuationWithSameStyle(nsIFrame* aFrame,
                                   nsStyleContext* aOldStyleContext,
                                   bool* aHaveMoreContinuations = nullptr);

  AnimationsWithDestroyedFrame* mAnimationsWithDestroyedFrame = nullptr;

  friend class mozilla::GeckoRestyleManager;
  friend class mozilla::ServoRestyleManager;
};

} // namespace mozilla

#endif
