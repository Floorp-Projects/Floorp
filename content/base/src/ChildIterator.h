/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChildIterator_h
#define ChildIterator_h

#include "nsIContent.h"

/**
 * Iterates over the children on a node. If a child is an insertion point,
 * iterates over the children inserted there instead, or the default content
 * if no children are inserted there.
 *
 * The FlattenedChildIterator expands any anonymous content bound from an XBL
 * binding's <xbl:content> element.
 */

#include <stdint.h>
#include "nsAutoPtr.h"

class nsIContent;

namespace mozilla {
namespace dom {

// This class iterates normal DOM child nodes of a given DOM node with
// <xbl:children> nodes replaced by the elements that have been filtered into that
// insertion point. Any bindings on the given element are ignored for purposes
// of determining which insertion point children are filtered into. The iterator
// can be initialized to start at the end by providing false for aStartAtBeginning
// in order to start iterating in reverse from the last child.
class ExplicitChildIterator
{
public:
  explicit ExplicitChildIterator(nsIContent* aParent, bool aStartAtBeginning = true)
    : mParent(aParent),
      mChild(nullptr),
      mDefaultChild(nullptr),
      mIndexInInserted(0),
      mIsFirst(aStartAtBeginning)
  {
  }

  ExplicitChildIterator(const ExplicitChildIterator& aOther)
    : mParent(aOther.mParent), mChild(aOther.mChild),
      mDefaultChild(aOther.mDefaultChild),
      mShadowIterator(aOther.mShadowIterator ?
                      new ExplicitChildIterator(*aOther.mShadowIterator) :
                      nullptr),
      mIndexInInserted(aOther.mIndexInInserted), mIsFirst(aOther.mIsFirst) {}

  ExplicitChildIterator(ExplicitChildIterator&& aOther)
    : mParent(aOther.mParent), mChild(aOther.mChild),
      mDefaultChild(aOther.mDefaultChild),
      mShadowIterator(Move(aOther.mShadowIterator)),
      mIndexInInserted(aOther.mIndexInInserted), mIsFirst(aOther.mIsFirst) {}

  nsIContent* GetNextChild();

  // Looks for aChildToFind respecting insertion points until aChildToFind
  // or aBound is found. If aBound is nullptr then the seek is unbounded. Returns
  // whether aChildToFind was found as an explicit child prior to encountering
  // aBound.
  bool Seek(nsIContent* aChildToFind, nsIContent* aBound = nullptr)
  {
    // It would be nice to assert that we find aChildToFind, but bz thinks that
    // we might not find aChildToFind when called from ContentInserted
    // if first-letter frames are about.

    nsIContent* child;
    do {
      child = GetNextChild();
    } while (child && child != aChildToFind && child != aBound);

    return child == aChildToFind;
  }

  // Returns the current target of this iterator (which might be an explicit
  // child of the node, fallback content of an insertion point or
  // a node distributed to an insertion point.
  nsIContent* Get();

  // The inverse of GetNextChild. Properly steps in and out of insertion
  // points.
  nsIContent* GetPreviousChild();

protected:
  // The parent of the children being iterated. For the FlattenedChildIterator,
  // if there is a binding attached to the original parent, mParent points to
  // the <xbl:content> element for the binding.
  nsIContent* mParent;

  // The current child. When we encounter an insertion point,
  // mChild remains as the insertion point whose content we're iterating (and
  // our state is controled by mDefaultChild or mIndexInInserted depending on
  // whether the insertion point expands to its default content or not).
  nsIContent* mChild;

  // If non-null, this points to the current default content for the current
  // insertion point that we're iterating (i.e. mChild, which must be an
  // nsXBLChildrenElement or HTMLContentElement). Once this transitions back
  // to null, we continue iterating at mChild's next sibling.
  nsIContent* mDefaultChild;

  // If non-null, this points to an iterator of the explicit children of
  // the ShadowRoot projected by the current shadow element that we're
  // iterating.
  nsAutoPtr<ExplicitChildIterator> mShadowIterator;

  // If not zero, we're iterating inserted children for an insertion point. This
  // is an index into mChild's inserted children array (mChild must be an
  // nsXBLChildrenElement). The index is one past the "current" child (as
  // opposed to mChild which represents the "current" child).
  uint32_t mIndexInInserted;

  // A flag to let us know that we haven't started iterating yet.
  bool mIsFirst;
};

// Iterates over the flattened children of a node, which accounts for anonymous
// children and nodes moved by insertion points. If a node has anonymous
// children, those are iterated over.
class FlattenedChildIterator : public ExplicitChildIterator
{
public:
  explicit FlattenedChildIterator(nsIContent* aParent)
    : ExplicitChildIterator(aParent), mXBLInvolved(false)
  {
    Init(false);
  }

  FlattenedChildIterator(FlattenedChildIterator&& aOther)
    : ExplicitChildIterator(Move(aOther)), mXBLInvolved(aOther.mXBLInvolved) {}

  FlattenedChildIterator(const FlattenedChildIterator& aOther)
    : ExplicitChildIterator(aOther), mXBLInvolved(aOther.mXBLInvolved) {}

  bool XBLInvolved() { return mXBLInvolved; }

protected:
  /**
   * This constructor is a hack to help AllChildrenIterator which sometimes
   * doesn't want to consider XBL.
   */
  FlattenedChildIterator(nsIContent* aParent, bool aIgnoreXBL)
    : ExplicitChildIterator(aParent), mXBLInvolved(false)
  {
    Init(aIgnoreXBL);
  }

  void Init(bool aIgnoreXBL);

  // For certain optimizations, nsCSSFrameConstructor needs to know if the
  // child list of the element that we're iterating matches its .childNodes.
  bool mXBLInvolved;
};

/**
 * AllChildrenIterator returns the children of a element including before /
 * after content and optionally XBL children.  It assumes that no mutation of
 * the DOM or frame tree takes place during iteration, and will break horribly
 * if that is not true.
 */
class AllChildrenIterator : private FlattenedChildIterator
{
public:
  AllChildrenIterator(nsIContent* aNode, uint32_t aFlags) :
    FlattenedChildIterator(aNode, (aFlags & nsIContent::eAllButXBL)),
    mOriginalContent(aNode), mFlags(aFlags),
    mPhase(eNeedBeforeKid) {}

  AllChildrenIterator(AllChildrenIterator&& aOther)
    : FlattenedChildIterator(Move(aOther)),
      mOriginalContent(aOther.mOriginalContent),
      mAnonKids(Move(aOther.mAnonKids)), mFlags(aOther.mFlags),
      mPhase(aOther.mPhase)
#ifdef DEBUG
      , mMutationGuard(aOther.mMutationGuard)
#endif
      {}

#ifdef DEBUG
  ~AllChildrenIterator() { MOZ_ASSERT(!mMutationGuard.Mutated(0)); }
#endif

  nsIContent* GetNextChild();

private:
  enum IteratorPhase
  {
    eNeedBeforeKid,
    eNeedExplicitKids,
    eNeedAnonKids,
    eNeedAfterKid,
    eDone
  };

  nsIContent* mOriginalContent;
  nsTArray<nsIContent*> mAnonKids;
  uint32_t mFlags;
  IteratorPhase mPhase;
#ifdef DEBUG
  // XXX we should really assert there are no frame tree changes as well, but
  // there's no easy way to do that.
  nsMutationGuard mMutationGuard;
#endif
};

} // namespace dom
} // namespace mozilla

#endif
