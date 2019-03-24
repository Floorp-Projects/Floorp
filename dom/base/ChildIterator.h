/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChildIterator_h
#define ChildIterator_h

#include "nsIContent.h"
#include "nsIContentInlines.h"
#include <stdint.h>

/**
 * Iterates over the children on a node. If a child is an insertion point,
 * iterates over the children inserted there instead, or the default content
 * if no children are inserted there.
 *
 * The FlattenedChildIterator expands any anonymous content bound from an XBL
 * binding's <xbl:content> element.
 */

class nsIContent;

namespace mozilla {
namespace dom {

// This class iterates normal DOM child nodes of a given DOM node with
// <xbl:children> nodes replaced by the elements that have been filtered into
// that insertion point. Any bindings on the given element are ignored for
// purposes of determining which insertion point children are filtered into. The
// iterator can be initialized to start at the end by providing false for
// aStartAtBeginning in order to start iterating in reverse from the last child.
class ExplicitChildIterator {
 public:
  explicit ExplicitChildIterator(const nsIContent* aParent,
                                 bool aStartAtBeginning = true);

  ExplicitChildIterator(const ExplicitChildIterator& aOther)
      : mParent(aOther.mParent),
        mParentAsSlot(aOther.mParentAsSlot),
        mChild(aOther.mChild),
        mDefaultChild(aOther.mDefaultChild),
        mIsFirst(aOther.mIsFirst),
        mIndexInInserted(aOther.mIndexInInserted) {}

  ExplicitChildIterator(ExplicitChildIterator&& aOther)
      : mParent(aOther.mParent),
        mParentAsSlot(aOther.mParentAsSlot),
        mChild(aOther.mChild),
        mDefaultChild(aOther.mDefaultChild),
        mIsFirst(aOther.mIsFirst),
        mIndexInInserted(aOther.mIndexInInserted) {}

  nsIContent* GetNextChild();

  // Looks for aChildToFind respecting insertion points until aChildToFind is
  // found.  This version can take shortcuts that the two-argument version
  // can't, so can be faster (and in fact can be O(1) instead of O(N) in many
  // cases).
  bool Seek(const nsIContent* aChildToFind);

  // Looks for aChildToFind respecting insertion points until aChildToFind is
  // found. or aBound is found. If aBound is nullptr then the seek is unbounded.
  // Returns whether aChildToFind was found as an explicit child prior to
  // encountering aBound.
  bool Seek(const nsIContent* aChildToFind, nsIContent* aBound) {
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

  // If parent is a slot element, this points to the parent as HTMLSlotElement,
  // otherwise, it's null.
  const HTMLSlotElement* mParentAsSlot;

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

  // A flag to let us know that we haven't started iterating yet.
  bool mIsFirst;

  // If not zero, we're iterating inserted children for an insertion point. This
  // is an index into mChild's inserted children array (mChild must be an
  // nsXBLChildrenElement). The index is one past the "current" child (as
  // opposed to mChild which represents the "current" child).
  uint32_t mIndexInInserted;
};

// Iterates over the flattened children of a node, which accounts for anonymous
// children and nodes moved by insertion points. If a node has anonymous
// children, those are iterated over.  The iterator can be initialized to start
// at the end by providing false for aStartAtBeginning in order to start
// iterating in reverse from the last child.
class FlattenedChildIterator : public ExplicitChildIterator {
 public:
  explicit FlattenedChildIterator(const nsIContent* aParent,
                                  bool aStartAtBeginning = true)
      : ExplicitChildIterator(aParent, aStartAtBeginning),
        mOriginalContent(aParent) {
    Init(false);
  }

  FlattenedChildIterator(FlattenedChildIterator&& aOther)
      : ExplicitChildIterator(std::move(aOther)),
        mOriginalContent(aOther.mOriginalContent),
        mXBLInvolved(aOther.mXBLInvolved) {}

  FlattenedChildIterator(const FlattenedChildIterator& aOther)
      : ExplicitChildIterator(aOther),
        mOriginalContent(aOther.mOriginalContent),
        mXBLInvolved(aOther.mXBLInvolved) {}

  bool XBLInvolved() {
    if (mXBLInvolved.isNothing()) {
      mXBLInvolved = Some(ComputeWhetherXBLIsInvolved());
    }
    return *mXBLInvolved;
  }

  const nsIContent* Parent() const { return mOriginalContent; }

 private:
  bool ComputeWhetherXBLIsInvolved() const;

  void Init(bool aIgnoreXBL);

 protected:
  /**
   * This constructor is a hack to help AllChildrenIterator which sometimes
   * doesn't want to consider XBL.
   */
  FlattenedChildIterator(const nsIContent* aParent, uint32_t aFlags,
                         bool aStartAtBeginning = true)
      : ExplicitChildIterator(aParent, aStartAtBeginning),
        mOriginalContent(aParent) {
    bool ignoreXBL = aFlags & nsIContent::eAllButXBL;
    Init(ignoreXBL);
  }

  const nsIContent* mOriginalContent;

 private:
  // For certain optimizations, nsCSSFrameConstructor needs to know if the child
  // list of the element that we're iterating matches its .childNodes.
  //
  // This is lazily computed when asked for it.
  Maybe<bool> mXBLInvolved;
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
class AllChildrenIterator : private FlattenedChildIterator {
 public:
  AllChildrenIterator(const nsIContent* aNode, uint32_t aFlags,
                      bool aStartAtBeginning = true)
      : FlattenedChildIterator(aNode, aFlags, aStartAtBeginning),
        mAnonKidsIdx(aStartAtBeginning ? UINT32_MAX : 0),
        mFlags(aFlags),
        mPhase(aStartAtBeginning ? eAtBegin : eAtEnd) {}

  AllChildrenIterator(AllChildrenIterator&& aOther)
      : FlattenedChildIterator(std::move(aOther)),
        mAnonKids(std::move(aOther.mAnonKids)),
        mAnonKidsIdx(aOther.mAnonKidsIdx),
        mFlags(aOther.mFlags),
        mPhase(aOther.mPhase)
#ifdef DEBUG
        ,
        mMutationGuard(aOther.mMutationGuard)
#endif
  {
  }

  AllChildrenIterator& operator=(AllChildrenIterator&& aOther) {
    this->~AllChildrenIterator();
    new (this) AllChildrenIterator(std::move(aOther));
    return *this;
  }

#ifdef DEBUG
  ~AllChildrenIterator() { MOZ_ASSERT(!mMutationGuard.Mutated(0)); }
#endif

  // Returns the current target the iterator is at, or null if the iterator
  // doesn't point to any child node (either eAtBegin or eAtEnd phase).
  nsIContent* Get() const;

  // Seeks the given node in children of a parent element, starting from
  // the current iterator's position, and sets the iterator at the given child
  // node if it was found.
  bool Seek(const nsIContent* aChildToFind);

  nsIContent* GetNextChild();
  nsIContent* GetPreviousChild();

  enum IteratorPhase {
    eAtBegin,
    eAtMarkerKid,
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
class MOZ_NEEDS_MEMMOVABLE_MEMBERS StyleChildrenIterator
    : private AllChildrenIterator {
 public:
  static nsIContent* GetParent(const nsIContent& aContent) {
    nsINode* node = aContent.GetFlattenedTreeParentNodeForStyle();
    return node && node->IsContent() ? node->AsContent() : nullptr;
  }

  explicit StyleChildrenIterator(const nsIContent* aContent,
                                 bool aStartAtBeginning = true)
      : AllChildrenIterator(
            aContent,
            nsIContent::eAllChildren |
                nsIContent::eSkipDocumentLevelNativeAnonymousContent,
            aStartAtBeginning) {
    MOZ_COUNT_CTOR(StyleChildrenIterator);
  }

  StyleChildrenIterator(StyleChildrenIterator&& aOther)
      : AllChildrenIterator(std::move(aOther)) {
    MOZ_COUNT_CTOR(StyleChildrenIterator);
  }

  StyleChildrenIterator& operator=(StyleChildrenIterator&& aOther) {
    AllChildrenIterator::operator=(std::move(aOther));
    return *this;
  }

  ~StyleChildrenIterator() { MOZ_COUNT_DTOR(StyleChildrenIterator); }

  using AllChildrenIterator::GetNextChild;
  using AllChildrenIterator::GetPreviousChild;
  using AllChildrenIterator::Seek;
};

}  // namespace dom
}  // namespace mozilla

#endif
