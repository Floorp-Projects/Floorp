/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  explicit ExplicitChildIterator(const nsIContent* aParent,
                                 bool aStartAtBeginning = true)
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

  // Looks for aChildToFind respecting insertion points until aChildToFind is
  // found.  This version can take shortcuts that the two-argument version
  // can't, so can be faster (and in fact can be O(1) instead of O(N) in many
  // cases).
  bool Seek(nsIContent* aChildToFind);

  // Looks for aChildToFind respecting insertion points until aChildToFind is found.
  // or aBound is found. If aBound is nullptr then the seek is unbounded. Returns
  // whether aChildToFind was found as an explicit child prior to encountering
  // aBound.
  bool Seek(nsIContent* aChildToFind, nsIContent* aBound)
  {
    // It would be nice to assert that we find aChildToFind, but bz thinks that
    // we might not find aChildToFind when called from ContentInserted
    // if first-letter frames are about.

    // We can't easily take shortcuts here because we'd have to have a way to
    // compare aChildToFind to aBound.
    nsIContent* child;
    do {
      child = GetNextChild();
    } while (child && child != aChildToFind && child != aBound);

    return child == aChildToFind;
  }

  // Returns the current target of this iterator (which might be an explicit
  // child of the node, fallback content of an insertion point or
  // a node distributed to an insertion point.
  nsIContent* Get() const;

  // The inverse of GetNextChild. Properly steps in and out of insertion
  // points.
  nsIContent* GetPreviousChild();

protected:
  // The parent of the children being iterated. For the FlattenedChildIterator,
  // if there is a binding attached to the original parent, mParent points to
  // the <xbl:content> element for the binding.
  const nsIContent* mParent;

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
// children, those are iterated over.  The iterator can be initialized to start
// at the end by providing false for aStartAtBeginning in order to start
// iterating in reverse from the last child.
class FlattenedChildIterator : public ExplicitChildIterator
{
public:
  explicit FlattenedChildIterator(const nsIContent* aParent,
                                  bool aStartAtBeginning = true)
    : ExplicitChildIterator(aParent, aStartAtBeginning), mXBLInvolved(false)
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
  FlattenedChildIterator(const nsIContent* aParent, uint32_t aFlags,
                         bool aStartAtBeginning = true)
    : ExplicitChildIterator(aParent, aStartAtBeginning), mXBLInvolved(false)
  {
    bool ignoreXBL = aFlags & nsIContent::eAllButXBL;
    Init(ignoreXBL);
  }

  void Init(bool aIgnoreXBL);

  // For certain optimizations, nsCSSFrameConstructor needs to know if the
  // child list of the element that we're iterating matches its .childNodes.
  bool mXBLInvolved;
};

/**
 * AllChildrenIterator traverses the children of an element including before /
 * after content and optionally XBL children.  The iterator can be initialized
 * to start at the end by providing false for aStartAtBeginning in order to
 * start iterating in reverse from the last child.
 *
 * Note: it assumes that no mutation of the DOM or frame tree takes place during
 * iteration, and will break horribly if that is not true.
 */
class AllChildrenIterator : private FlattenedChildIterator
{
public:
  AllChildrenIterator(const nsIContent* aNode, uint32_t aFlags,
                      bool aStartAtBeginning = true) :
    FlattenedChildIterator(aNode, aFlags, aStartAtBeginning),
    mOriginalContent(aNode), mAnonKidsIdx(aStartAtBeginning ? UINT32_MAX : 0),
    mFlags(aFlags), mPhase(aStartAtBeginning ? eAtBegin : eAtEnd) { }

  AllChildrenIterator(AllChildrenIterator&& aOther)
    : FlattenedChildIterator(Move(aOther)),
      mOriginalContent(aOther.mOriginalContent),
      mAnonKids(Move(aOther.mAnonKids)), mAnonKidsIdx(aOther.mAnonKidsIdx),
      mFlags(aOther.mFlags), mPhase(aOther.mPhase)
#ifdef DEBUG
      , mMutationGuard(aOther.mMutationGuard)
#endif
      {}

#ifdef DEBUG
  ~AllChildrenIterator() { MOZ_ASSERT(!mMutationGuard.Mutated(0)); }
#endif

  // Returns the current target the iterator is at, or null if the iterator
  // doesn't point to any child node (either eAtBegin or eAtEnd phase).
  nsIContent* Get() const;

  // Seeks the given node in children of a parent element, starting from
  // the current iterator's position, and sets the iterator at the given child
  // node if it was found.
  bool Seek(nsIContent* aChildToFind);

  nsIContent* GetNextChild();
  nsIContent* GetPreviousChild();
  const nsIContent* Parent() const { return mOriginalContent; }

  enum IteratorPhase
  {
    eAtBegin,
    eAtBeforeKid,
    eAtExplicitKids,
    eAtAnonKids,
    eAtAfterKid,
    eAtEnd
  };
  IteratorPhase Phase() const { return mPhase; }

private:
  // Helpers.
  void AppendNativeAnonymousChildren();
  const nsIContent* mOriginalContent;

  // mAnonKids is an array of native anonymous children, mAnonKidsIdx is index
  // in the array. If mAnonKidsIdx < mAnonKids.Length() and mPhase is
  // eAtAnonKids then the iterator points at a child at mAnonKidsIdx index. If
  // mAnonKidsIdx == mAnonKids.Length() then the iterator is somewhere after
  // the last native anon child. If mAnonKidsIdx == UINT32_MAX then the iterator
  // is somewhere before the first native anon child.
  nsTArray<nsIContent*> mAnonKids;
  uint32_t mAnonKidsIdx;

  uint32_t mFlags;
  IteratorPhase mPhase;
#ifdef DEBUG
  // XXX we should really assert there are no frame tree changes as well, but
  // there's no easy way to do that.
  nsMutationGuard mMutationGuard;
#endif
};

/**
 * StyleChildrenIterator traverses the children of the element from the
 * perspective of the style system, particularly the children we need to
 * traverse during restyle.
 *
 * At present, this is identical to AllChildrenIterator with
 * (eAllChildren | eSkipDocumentLevelNativeAnonymousContent). We used to have
 * detect and skip any native anonymous children that are used to implement some
 * special magic in here that went away, but we keep the separate class so
 * we can reintroduce special magic back if needed.
 *
 * Note: it assumes that no mutation of the DOM or frame tree takes place during
 * iteration, and will break horribly if that is not true.
 *
 * We require this to be memmovable since Rust code can create and move
 * StyleChildrenIterators.
 */
class MOZ_NEEDS_MEMMOVABLE_MEMBERS StyleChildrenIterator : private AllChildrenIterator
{
public:
  explicit StyleChildrenIterator(const nsIContent* aContent)
    : AllChildrenIterator(aContent,
                          nsIContent::eAllChildren |
                          nsIContent::eSkipDocumentLevelNativeAnonymousContent)
  {
    MOZ_COUNT_CTOR(StyleChildrenIterator);
  }
  ~StyleChildrenIterator() { MOZ_COUNT_DTOR(StyleChildrenIterator); }

  nsIContent* GetNextChild();
};

} // namespace dom
} // namespace mozilla

#endif
