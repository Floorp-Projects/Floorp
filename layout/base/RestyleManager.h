/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code responsible for managing style changes: tracking what style
 * changes need to happen, scheduling them, and doing them.
 */

#ifndef mozilla_RestyleManager_h
#define mozilla_RestyleManager_h

#include "nsISupportsImpl.h"
#include "nsChangeHint.h"
#include "RestyleTracker.h"
#include "nsPresContext.h"

class nsRefreshDriver;
class nsIFrame;
struct TreeMatchContext;

namespace mozilla {

namespace dom {
  class Element;
} // namespace dom

class RestyleManager MOZ_FINAL {
public:
  friend class ::nsRefreshDriver;
  friend class RestyleTracker;

  typedef mozilla::dom::Element Element;

  RestyleManager(nsPresContext* aPresContext);

  NS_INLINE_DECL_REFCOUNTING(mozilla::RestyleManager)

  void Disconnect() {
    mPresContext = nullptr;
  }

  nsPresContext* PresContext() const {
    MOZ_ASSERT(mPresContext);
    return mPresContext;
  }

  nsCSSFrameConstructor* FrameConstructor() const
    { return PresContext()->FrameConstructor(); }

  // Should be called when a frame is going to be destroyed and
  // WillDestroyFrameTree hasn't been called yet.
  void NotifyDestroyingFrame(nsIFrame* aFrame);

  // Forwarded nsIDocumentObserver method, to handle restyling (and
  // passing the notification to the frame).
  nsresult ContentStateChanged(nsIContent*   aContent,
                               nsEventStates aStateMask);

  // Forwarded nsIMutationObserver method, to handle restyling.
  void AttributeWillChange(Element* aElement,
                           int32_t  aNameSpaceID,
                           nsIAtom* aAttribute,
                           int32_t  aModType);
  // Forwarded nsIMutationObserver method, to handle restyling (and
  // passing the notification to the frame).
  void AttributeChanged(Element* aElement,
                        int32_t  aNameSpaceID,
                        nsIAtom* aAttribute,
                        int32_t  aModType);

  // Get an integer that increments every time there is a style change
  // as a result of a change to the :hover content state.
  uint32_t GetHoverGeneration() const { return mHoverGeneration; }

  // Get a counter that increments on every style change, that we use to
  // track whether off-main-thread animations are up-to-date.
  uint64_t GetAnimationGeneration() const { return mAnimationGeneration; }

  /**
   * Reparent the style contexts of this frame subtree.  The parent frame of
   * aFrame must be changed to the new parent before this function is called;
   * the new parent style context will be automatically computed based on the
   * new position in the frame tree.
   *
   * @param aFrame the root of the subtree to reparent.  Must not be null.
   */
  NS_HIDDEN_(nsresult) ReparentStyleContext(nsIFrame* aFrame);

  /**
   * Re-resolve the style contexts for a frame tree, building
   * aChangeList based on the resulting style changes, plus aMinChange
   * applied to aFrame.
   */
  NS_HIDDEN_(void)
    ComputeStyleChangeFor(nsIFrame* aFrame,
                          nsStyleChangeList* aChangeList,
                          nsChangeHint aMinChange,
                          RestyleTracker& aRestyleTracker,
                          bool aRestyleDescendants);

#ifdef DEBUG
  /**
   * DEBUG ONLY method to verify integrity of style tree versus frame tree
   */
  NS_HIDDEN_(void) DebugVerifyStyleTree(nsIFrame* aFrame);
#endif

  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessRestyledFrames call in a view update batch and a script blocker.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  nsresult ProcessRestyledFrames(nsStyleChangeList& aRestyleArray);

private:
  void RestyleForEmptyChange(Element* aContainer);

public:
  // Restyling for a ContentInserted (notification after insertion) or
  // for a CharacterDataChanged.  |aContainer| must be non-null; when
  // the container is null, no work is needed.
  void RestyleForInsertOrChange(Element* aContainer, nsIContent* aChild);

  // This would be the same as RestyleForInsertOrChange if we got the
  // notification before the removal.  However, we get it after, so we need the
  // following sibling in addition to the old child.  |aContainer| must be
  // non-null; when the container is null, no work is needed.  aFollowingSibling
  // is the sibling that used to come after aOldChild before the removal.
  void RestyleForRemove(Element* aContainer,
                        nsIContent* aOldChild,
                        nsIContent* aFollowingSibling);

  // Same for a ContentAppended.  |aContainer| must be non-null; when
  // the container is null, no work is needed.
  void RestyleForAppend(Element* aContainer, nsIContent* aFirstNewContent);

  // Process any pending restyles. This should be called after
  // CreateNeededFrames.
  // Note: It's the caller's responsibility to make sure to wrap a
  // ProcessPendingRestyles call in a view update batch and a script blocker.
  // This function does not call ProcessAttachedQueue() on the binding manager.
  // If the caller wants that to happen synchronously, it needs to handle that
  // itself.
  void ProcessPendingRestyles();

  // ProcessPendingRestyles calls into one of our RestyleTracker
  // objects.  It then calls back to these functions at the beginning
  // and end of its work.
  void BeginProcessingRestyles();
  void EndProcessingRestyles();

  // Rebuilds all style data by throwing out the old rule tree and
  // building a new one, and additionally applying aExtraHint (which
  // must not contain nsChangeHint_ReconstructFrame) to the root frame.
  void RebuildAllStyleData(nsChangeHint aExtraHint);

  // Helper that does part of the work of RebuildAllStyleData, shared by
  // RestyleElement for 'rem' handling.
  void DoRebuildAllStyleData(RestyleTracker& aRestyleTracker,
                             nsChangeHint aExtraHint);

  // See PostRestyleEventCommon below.
  void PostRestyleEvent(Element* aElement,
                        nsRestyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint)
  {
    if (mPresContext) {
      PostRestyleEventCommon(aElement, aRestyleHint, aMinChangeHint,
                             mPresContext->IsProcessingAnimationStyleChange());
    }
  }

  // See PostRestyleEventCommon below.
  void PostAnimationRestyleEvent(Element* aElement,
                                 nsRestyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint)
  {
    PostRestyleEventCommon(aElement, aRestyleHint, aMinChangeHint, true);
  }

  void PostRestyleEventForLazyConstruction()
  {
    PostRestyleEventInternal(true);
  }

  void FlushOverflowChangedTracker()
  {
    mOverflowChangedTracker.Flush();
  }

private:
  /**
   * Notify the frame constructor that an element needs to have its
   * style recomputed.
   * @param aElement: The element to be restyled.
   * @param aRestyleHint: Which nodes need to have selector matching run
   *                      on them.
   * @param aMinChangeHint: A minimum change hint for aContent and its
   *                        descendants.
   * @param aForAnimation: Whether the style should be computed with or
   *                       without animation data.  Animation code
   *                       sometimes needs to pass true; other code
   *                       should generally pass the the pres context's
   *                       IsProcessingAnimationStyleChange() value
   *                       (which is the default value).
   */
  void PostRestyleEventCommon(Element* aElement,
                              nsRestyleHint aRestyleHint,
                              nsChangeHint aMinChangeHint,
                              bool aForAnimation);
  void PostRestyleEventInternal(bool aForLazyConstruction);

public:
  /**
   * Asynchronously clear style data from the root frame downwards and ensure
   * it will all be rebuilt. This is safe to call anytime; it will schedule
   * a restyle and take effect next time style changes are flushed.
   * This method is used to recompute the style data when some change happens
   * outside of any style rules, like a color preference change or a change
   * in a system font size, or to fix things up when an optimization in the
   * style data has become invalid. We assume that the root frame will not
   * need to be reframed.
   */
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint);

private:
  /* aMinHint is the minimal change that should be made to the element */
  // XXXbz do we really need the aPrimaryFrame argument here?
  void RestyleElement(Element*        aElement,
                      nsIFrame*       aPrimaryFrame,
                      nsChangeHint    aMinHint,
                      RestyleTracker& aRestyleTracker,
                      bool            aRestyleDescendants);

  nsresult StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint);

  // Returns true if this function managed to successfully move a frame, and
  // false if it could not process the position change, and a reflow should
  // be performed instead.
  bool RecomputePosition(nsIFrame* aFrame);

private:
  nsPresContext* mPresContext; // weak, disconnected in Disconnect

  bool mRebuildAllStyleData : 1;
  // True if we're already waiting for a refresh notification
  bool mObservingRefreshDriver : 1;
  // True if we're in the middle of a nsRefreshDriver refresh
  bool mInStyleRefresh : 1;
  uint32_t mHoverGeneration;
  nsChangeHint mRebuildAllExtraHint;

  OverflowChangedTracker mOverflowChangedTracker;

  // The total number of animation flushes by this frame constructor.
  // Used to keep the layer and animation manager in sync.
  uint64_t mAnimationGeneration;

  RestyleTracker mPendingRestyles;
  RestyleTracker mPendingAnimationRestyles;
};

/**
 * An ElementRestyler is created for *each* element in a subtree that we
 * recompute styles for.
 */
class ElementRestyler MOZ_FINAL {
public:
  typedef mozilla::dom::Element Element;

  // Construct for the root of the subtree that we're restyling.
  ElementRestyler(nsPresContext* aPresContext);

  // Construct for an element whose parent is being restyled.
  ElementRestyler(const ElementRestyler& aParentRestyler);

  // Construct for a frame whose parent is being restyled, but whose
  // style context is the parent style context for its parent frame.
  // (This is only used for table frames, whose style contexts are used
  // as the parent style context for their outer table frame (table
  // wrapper frame).  We should probably try to get rid of this
  // exception and have the inheritance go the other way.)
  enum ParentContextFromChildFrame { PARENT_CONTEXT_FROM_CHILD_FRAME };
  ElementRestyler(ParentContextFromChildFrame,
                  const ElementRestyler& aParentFrameRestyler);

public: // FIXME: private
  enum DesiredA11yNotifications {
    eSkipNotifications,
    eSendAllNotifications,
    eNotifyIfShown
  };

  enum A11yNotificationType {
    eDontNotify,
    eNotifyShown,
    eNotifyHidden
  };

public:
  /**
   * Restyle our frame's element and its subtree.
   *
   * Use eRestyle_Self for the aRestyleHint argument to mean
   * "reresolve our style context but not kids", use eRestyle_Subtree
   * to mean "reresolve our style context and kids", and use
   * nsRestyleHint(0) to mean recompute a new style context for our
   * current parent and existing rulenode, and the same for kids.
   */
  nsChangeHint Restyle(nsPresContext     *aPresContext,
               nsIFrame          *aFrame,
               nsIContent        *aParentContent,
               nsStyleChangeList *aChangeList,
               nsChangeHint       aMinChange,
               nsChangeHint       aParentFrameHintsNotHandledForDescendants,
               nsRestyleHint      aRestyleHint,
               RestyleTracker&    aRestyleTracker,
               DesiredA11yNotifications aDesiredA11yNotifications,
               nsTArray<nsIContent*>& aVisibleKidsOfHiddenElement,
               TreeMatchContext &aTreeMatchContext);

private:
  void CaptureChange(nsStyleContext* aOldContext,
                     nsStyleContext* aNewContext,
                     nsIFrame* aFrame, nsIContent* aContent,
                     nsStyleChangeList* aChangeList,
                     /*inout*/nsChangeHint &aMinChange,
                     /*in*/nsChangeHint aParentHintsNotHandledForDescendants,
                     /*out*/nsChangeHint &aHintsNotHandledForDescendants,
                     nsChangeHint aChangeToAssume);

private:
  nsPresContext* const mPresContext;
};

} // namespace mozilla

#endif /* mozilla_RestyleManager_h */
