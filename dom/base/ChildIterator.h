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

class nsIContent;

namespace mozilla::dom {

// Iterates over the flattened children of a node, that is, the regular DOM
// child nodes of a given DOM node, with assigned nodes as slot children, and
// shadow host children replaced by their shadow root.
//
// The iterator can be initialized to start at the end by providing false for
// aStartAtBeginning in order to start iterating in reverse from the last child.
class FlattenedChildIterator {
 public:
  explicit FlattenedChildIterator(const nsIContent* aParent,
                                  bool aStartAtBeginning = true);

  nsIContent* GetNextChild();

  // Looks for aChildToFind respecting insertion points until aChildToFind is
  // found.  This can be O(1) instead of O(N) in many cases.
  bool Seek(const nsIContent* aChildToFind);

  // Returns the current target of this iterator (which might be an explicit
  // child of the node, or a node assigned to a slot.
  nsIContent* Get() const { return mChild; }

  // Returns the original parent we were initialized with.
  const nsIContent* Parent() const { return mOriginalParent; }

  // The inverse of GetNextChild. Properly steps in and out of insertion
  // points.
  nsIContent* GetPreviousChild();

  bool ShadowDOMInvolved() const { return mShadowDOMInvolved; }

 protected:
  // The parent of the children being iterated. For shadow hosts this will point
  // to its shadow root.
  const nsIContent* mParent;

  // If parent is a slot element with assigned slots, this points to the parent
  // as HTMLSlotElement, otherwise, it's null.
  const HTMLSlotElement* mParentAsSlot = nullptr;

  const nsIContent* mOriginalParent = nullptr;

  // The current child.
  nsIContent* mChild = nullptr;

  // A flag to let us know that we haven't started iterating yet.
  bool mIsFirst = false;

  // The index of the current element in the slot assigned nodes. One-past the
  // end to represent the last position.
  uint32_t mIndexInInserted = 0u;

  // For certain optimizations, nsCSSFrameConstructor needs to know if the child
  // list of the element that we're iterating matches its .childNodes.
  bool mShadowDOMInvolved = false;
};

/**
 * AllChildrenIterator traverses the children of an element including before /
 * after content and shadow DOM.  The iterator can be initialized to start at
 * the end by providing false for aStartAtBeginning in order to start iterating
 * in reverse from the last child.
 *
 * Note: it assumes that no mutation of the DOM or frame tree takes place during
 * iteration, and will break horribly if that is not true.
 */
class AllChildrenIterator : private FlattenedChildIterator {
 public:
  AllChildrenIterator(const nsIContent* aNode, uint32_t aFlags,
                      bool aStartAtBeginning = true)
      : FlattenedChildIterator(aNode, aStartAtBeginning),
        mAnonKidsIdx(aStartAtBeginning ? UINT32_MAX : 0),
        mFlags(aFlags),
        mPhase(aStartAtBeginning ? eAtBegin : eAtEnd) {}

#ifdef DEBUG
  AllChildrenIterator(AllChildrenIterator&&) = default;

  AllChildrenIterator& operator=(AllChildrenIterator&& aOther) {
    // Explicitly call the destructor to ensure the assertion in the destructor
    // is checked.
    this->~AllChildrenIterator();
    new (this) AllChildrenIterator(std::move(aOther));
    return *this;
  }

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
    eAtFlatTreeKids,
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

  StyleChildrenIterator& operator=(StyleChildrenIterator&& aOther) = default;

  MOZ_COUNTED_DTOR(StyleChildrenIterator)

  using AllChildrenIterator::GetNextChild;
  using AllChildrenIterator::GetPreviousChild;
  using AllChildrenIterator::Seek;
};

}  // namespace mozilla::dom

#endif
