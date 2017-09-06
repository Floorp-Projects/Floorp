/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoRestyleManager_h
#define mozilla_ServoRestyleManager_h

#include "mozilla/EventStates.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoElementSnapshotTable.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsAttrValue;
class nsIAtom;
class nsIContent;
class nsIFrame;
class nsStyleChangeList;

namespace mozilla {

/**
 * A stack class used to pass some common restyle state in a slightly more
 * comfortable way than a bunch of individual arguments, and that also checks
 * that the change hint used for optimization is correctly used in debug mode.
 */
class ServoRestyleState
{
public:
  ServoRestyleState(ServoStyleSet& aStyleSet, nsStyleChangeList& aChangeList,
                    nsTArray<nsIFrame*>& aPendingWrapperRestyles)
    : mStyleSet(aStyleSet)
    , mChangeList(aChangeList)
    , mPendingWrapperRestyles(aPendingWrapperRestyles)
    , mPendingWrapperRestyleOffset(aPendingWrapperRestyles.Length())
    , mChangesHandled(nsChangeHint(0))
  {}

  // We shouldn't assume that changes handled from our parent are handled for
  // our children too if we're out of flow since they aren't necessarily
  // parented in DOM order, and thus a change handled by a DOM ancestor doesn't
  // necessarily mean that it's handled for an ancestor frame.
  enum class Type
  {
    InFlow,
    OutOfFlow,
  };

  ServoRestyleState(const nsIFrame& aOwner,
                    ServoRestyleState& aParentState,
                    nsChangeHint aHintForThisFrame,
                    Type aType,
                    bool aAssertWrapperRestyleLength = true)
    : mStyleSet(aParentState.mStyleSet)
    , mChangeList(aParentState.mChangeList)
    , mPendingWrapperRestyles(aParentState.mPendingWrapperRestyles)
    , mPendingWrapperRestyleOffset(aParentState.mPendingWrapperRestyles.Length())
    , mChangesHandled(
        aType == Type::InFlow
          ? aParentState.mChangesHandled | aHintForThisFrame
          : aHintForThisFrame)
#ifdef DEBUG
    , mOwner(&aOwner)
    , mAssertWrapperRestyleLength(aAssertWrapperRestyleLength)
#endif
  {
    if (aType == Type::InFlow) {
      AssertOwner(aParentState);
    }
  }

  ~ServoRestyleState() {
    MOZ_ASSERT(!mAssertWrapperRestyleLength ||
               mPendingWrapperRestyles.Length() == mPendingWrapperRestyleOffset,
               "Someone forgot to call ProcessWrapperRestyles!");
  }

  nsStyleChangeList& ChangeList() { return mChangeList; }
  ServoStyleSet& StyleSet() { return mStyleSet; }

#ifdef DEBUG
  void AssertOwner(const ServoRestyleState& aParentState) const;
  nsChangeHint ChangesHandledFor(const nsIFrame&) const;
#else
  void AssertOwner(const ServoRestyleState&) const {}
  nsChangeHint ChangesHandledFor(const nsIFrame&) const
  {
    return mChangesHandled;
  }
#endif

  // Add a pending wrapper restyle.  We don't have to do anything if the thing
  // being added is already last in the list, but otherwise we do want to add
  // it, in order for ProcessWrapperRestyles to work correctly.
  void AddPendingWrapperRestyle(nsIFrame* aWrapperFrame);

  // Process wrapper restyles for this restyle state.  This should be done
  // before it comes off the stack.
  void ProcessWrapperRestyles(nsIFrame* aParentFrame);

  // Get the table-aware parent for the given child.  This will walk through
  // outer table and cellcontent frames.
  static nsIFrame* TableAwareParentFor(const nsIFrame* aChild);

private:
  // Process a wrapper restyle at the given index, and restyles for any
  // wrappers nested in it.  Returns the number of entries from
  // mPendingWrapperRestyles that we processed.  The return value is always at
  // least 1.
  size_t ProcessMaybeNestedWrapperRestyle(nsIFrame* aParent, size_t aIndex);

  ServoStyleSet& mStyleSet;
  nsStyleChangeList& mChangeList;

  // A list of pending wrapper restyles.  Anonymous box wrapper frames that need
  // restyling are added to this list when their non-anonymous kids are
  // restyled.  This avoids us having to do linear searches along the frame tree
  // for these anonymous boxes.  The problem then becomes that we can have
  // multiple kids all with the same anonymous parent, and we don't want to
  // restyle it more than once.  We use mPendingWrapperRestyles to track which
  // anonymous wrapper boxes we've requested be restyled and which of them have
  // already been restyled.  We use a single array propagated through
  // ServoRestyleStates by reference, because in a situation like this:
  //
  //  <div style="display: table"><span></span></div>
  //
  // We have multiple wrappers to restyle (cell, row, table-row-group) and we
  // want to add them in to the list all at once but restyle them using
  // different ServoRestyleStates with different owners.  When this situation
  // occurs, the relevant frames will be placed in the array with ancestors
  // before descendants.
  nsTArray<nsIFrame*>& mPendingWrapperRestyles;

  // Since we're given a possibly-nonempty mPendingWrapperRestyles to start
  // with, we need to keep track of where the part of it we're responsible for
  // starts.
  size_t mPendingWrapperRestyleOffset;

  const nsChangeHint mChangesHandled;

  // We track the "owner" frame of this restyle state, that is, the frame that
  // generated the last change that is stored in mChangesHandled, in order to
  // verify that we only use mChangesHandled for actual descendants of that
  // frame (given DOM order isn't always frame order, and that there are a few
  // special cases for stuff like wrapper frames, ::backdrop, and so on).
#ifdef DEBUG
  const nsIFrame* mOwner { nullptr };
#endif

  // Whether we should assert in our destructor that we've processed all of the
  // relevant wrapper restyles.
#ifdef DEBUG
  const bool mAssertWrapperRestyleLength = true;
#endif // DEBUG
};

enum class ServoPostTraversalFlags : uint32_t;

/**
 * Restyle manager for a Servo-backed style system.
 */
class ServoRestyleManager : public RestyleManager
{
  friend class ServoStyleSet;

public:
  typedef ServoElementSnapshotTable SnapshotTable;
  typedef RestyleManager base_type;

  explicit ServoRestyleManager(nsPresContext* aPresContext);

  void PostRestyleEvent(dom::Element* aElement,
                        nsRestyleHint aRestyleHint,
                        nsChangeHint aMinChangeHint);
  void PostRestyleEventForCSSRuleChanges();
  void RebuildAllStyleData(nsChangeHint aExtraHint,
                           nsRestyleHint aRestyleHint);
  void PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint);
  void ProcessPendingRestyles();
  void ProcessAllPendingAttributeAndStateInvalidations();
  bool HasPendingRestyleAncestor(dom::Element* aElement) const;

  /**
   * Performs a Servo animation-only traversal to compute style for all nodes
   * with the animation-only dirty bit in the document.
   *
   * This processes just the traversal for animation-only restyles and skips the
   * normal traversal for other restyles unrelated to animations.
   * This is used to bring throttled animations up-to-date such as when we need
   * to get correct position for transform animations that are throttled because
   * they are running on the compositor.
   *
   * This will traverse all of the document's style roots (that is, its document
   * element, and the roots of the document-level native anonymous content).
   */
  void UpdateOnlyAnimationStyles();

  void ContentStateChanged(nsIContent* aContent, EventStates aStateMask);
  void AttributeWillChange(dom::Element* aElement,
                           int32_t aNameSpaceID,
                           nsIAtom* aAttribute,
                           int32_t aModType,
                           const nsAttrValue* aNewValue);
  void ClassAttributeWillBeChangedBySMIL(dom::Element* aElement);

  void AttributeChanged(dom::Element* aElement, int32_t aNameSpaceID,
                        nsIAtom* aAttribute, int32_t aModType,
                        const nsAttrValue* aOldValue);

  // This is only used to reparent things when moving them in/out of the
  // ::first-line.  Once we get rid of the Gecko style system, we should rename
  // this method accordingly (e.g. to ReparentStyleContextForFirstLine).
  nsresult ReparentStyleContext(nsIFrame* aFrame);

private:
  /**
   * Reparent the descendants of aFrame.  This is used by ReparentStyleContext
   * and shouldn't be called by anyone else.  aProviderChild, if non-null, is a
   * child that was the style parent for aFrame and hence shouldn't be
   * reparented.
   */
  void ReparentFrameDescendants(nsIFrame* aFrame, nsIFrame* aProviderChild,
                                ServoStyleSet& aStyleSet);

public:
  /**
   * Clears the ServoElementData and HasDirtyDescendants from all elements
   * in the subtree rooted at aElement.
   */
  static void ClearServoDataFromSubtree(Element* aElement);

  /**
   * Clears HasDirtyDescendants and RestyleData from all elements in the
   * subtree rooted at aElement.
   */
  static void ClearRestyleStateFromSubtree(Element* aElement);

  /**
   * Posts restyle hints for animations.
   * This is only called for the second traversal for CSS animations during
   * updating CSS animations in a SequentialTask.
   * This function does neither register a refresh observer nor flag that a
   * style flush is needed since this function is supposed to be called during
   * restyling process and this restyle event will be processed in the second
   * traversal of the same restyling process.
   */
  void PostRestyleEventForAnimations(dom::Element* aElement,
                                     CSSPseudoElementType aPseudoType,
                                     nsRestyleHint aRestyleHint);
protected:
  ~ServoRestyleManager() override
  {
    MOZ_ASSERT(!mReentrantChanges);
  }

private:
  /**
   * Performs post-Servo-traversal processing on this element and its
   * descendants.
   *
   * Returns whether any style did actually change. There may be cases where we
   * didn't need to change any style after all, for example, when a content
   * attribute changes that happens not to have any effect on the style of that
   * element or any descendant or sibling.
   */
  bool ProcessPostTraversal(Element* aElement,
                            ServoStyleContext* aParentContext,
                            ServoRestyleState& aRestyleState,
                            ServoPostTraversalFlags aFlags);

  struct TextPostTraversalState;
  bool ProcessPostTraversalForText(nsIContent* aTextNode,
                                   TextPostTraversalState& aState,
                                   ServoRestyleState& aRestyleState,
                                   ServoPostTraversalFlags aFlags);

  inline ServoStyleSet* StyleSet() const
  {
    MOZ_ASSERT(PresContext()->StyleSet()->IsServo(),
               "ServoRestyleManager should only be used with a Servo-flavored "
               "style backend");
    return PresContext()->StyleSet()->AsServo();
  }

  const SnapshotTable& Snapshots() const { return mSnapshots; }
  void ClearSnapshots();
  ServoElementSnapshot& SnapshotFor(mozilla::dom::Element* aElement);
  void TakeSnapshotForAttributeChange(mozilla::dom::Element* aElement,
                                      int32_t aNameSpaceID,
                                      nsIAtom* aAttribute);

  void DoProcessPendingRestyles(ServoTraversalFlags aFlags);

  // Function to do the actual (recursive) work of ReparentStyleContext, once we
  // have asserted the invariants that only hold on the initial call.
  void DoReparentStyleContext(nsIFrame* aFrame,
                              ServoStyleSet& aStyleSet);

  // We use a separate data structure from nsStyleChangeList because we need a
  // frame to create nsStyleChangeList entries, and the primary frame may not be
  // attached yet.
  struct ReentrantChange {
    nsCOMPtr<nsIContent> mContent;
    nsChangeHint mHint;
  };
  typedef AutoTArray<ReentrantChange, 10> ReentrantChangeList;

  // Only non-null while processing change hints. See the comment in
  // ProcessPendingRestyles.
  ReentrantChangeList* mReentrantChanges;

  // We use this flag to track if the current restyle contains any non-animation
  // update, which triggers a normal restyle, and so there might be any new
  // transition created later. Therefore, if this flag is true, we need to
  // increase mAnimationGeneration before creating new transitions, so their
  // creation sequence will be correct.
  bool mHaveNonAnimationRestyles = false;

  // Set to true when posting restyle events triggered by CSS rule changes.
  // This flag is cleared once ProcessPendingRestyles has completed.
  // When we process a traversal all descendants elements of the document
  // triggered by CSS rule changes, we will need to update all elements with
  // CSS animations.  We propagate TraversalRestyleBehavior::ForCSSRuleChanges
  // to traversal function if this flag is set.
  bool mRestyleForCSSRuleChanges = false;

  // A hashtable with the elements that have changed state or attributes, in
  // order to calculate restyle hints during the traversal.
  SnapshotTable mSnapshots;
};

} // namespace mozilla

#endif // mozilla_ServoRestyleManager_h
