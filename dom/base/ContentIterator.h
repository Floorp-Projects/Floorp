/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentIterator_h
#define mozilla_ContentIterator_h

#include "mozilla/RangeBoundary.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsRange.h"
#include "nsTArray.h"

namespace mozilla {

/**
 * ContentIteratorBase is a base class of PostContentIterator,
 * PreContentIterator and ContentSubtreeIterator.  Making each concrete
 * classes "final", compiler can avoid virtual calls if they are treated
 * by the users directly.
 */
class ContentIteratorBase {
 public:
  ContentIteratorBase() = delete;
  ContentIteratorBase(const ContentIteratorBase&) = delete;
  ContentIteratorBase& operator=(const ContentIteratorBase&) = delete;
  virtual ~ContentIteratorBase() = default;

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ContentIteratorBase)

  virtual nsresult Init(nsINode* aRoot);
  virtual nsresult Init(nsRange* aRange);
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset);
  virtual nsresult Init(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd);

  virtual void First();
  virtual void Last();
  virtual void Next();
  virtual void Prev();

  virtual nsINode* GetCurrentNode();

  virtual bool IsDone();

  virtual nsresult PositionAt(nsINode* aCurNode);

 protected:
  explicit ContentIteratorBase(bool aPre);

  /**
   * Callers must guarantee that:
   * - Neither aStartContainer nor aEndContainer is nullptr.
   * - aStartOffset and aEndOffset are valid for its container.
   * - The start point and the end point are in document order.
   */
  nsresult InitInternal(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd);

  // Recursively get the deepest first/last child of aRoot.  This will return
  // aRoot itself if it has no children.
  nsINode* GetDeepFirstChild(nsINode* aRoot);
  nsIContent* GetDeepFirstChild(nsIContent* aRoot);
  nsINode* GetDeepLastChild(nsINode* aRoot);
  nsIContent* GetDeepLastChild(nsIContent* aRoot);

  // Get the next/previous sibling of aNode, or its parent's, or grandparent's,
  // etc.  Returns null if aNode and all its ancestors have no next/previous
  // sibling.
  nsIContent* GetNextSibling(nsINode* aNode);
  nsIContent* GetPrevSibling(nsINode* aNode);

  nsINode* NextNode(nsINode* aNode);
  nsINode* PrevNode(nsINode* aNode);

  void MakeEmpty();

  nsCOMPtr<nsINode> mCurNode;
  nsCOMPtr<nsINode> mFirst;
  nsCOMPtr<nsINode> mLast;
  nsCOMPtr<nsINode> mCommonParent;

  bool mIsDone;
  bool mPre;
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          ContentIteratorBase&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(ContentIteratorBase&);
};

// Each concreate class of ContentIteratorBase may be owned by another class
// which may be owned by JS.  Therefore, all of them should be in the cycle
// collection.  However, we cannot make non-refcountable classes only with the
// macros.  So, we need to make them cycle collectable without the macros.
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, ContentIteratorBase& aField,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mCurNode, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mFirst, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mLast, aName, aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mCommonParent, aName, aFlags);
}

inline void ImplCycleCollectionUnlink(ContentIteratorBase& aField) {
  ImplCycleCollectionUnlink(aField.mCurNode);
  ImplCycleCollectionUnlink(aField.mFirst);
  ImplCycleCollectionUnlink(aField.mLast);
  ImplCycleCollectionUnlink(aField.mCommonParent);
}

/**
 * A simple iterator class for traversing the content in "close tag" order.
 */
class PostContentIterator final : public ContentIteratorBase {
 public:
  PostContentIterator() : ContentIteratorBase(false) {}
  PostContentIterator(const PostContentIterator&) = delete;
  PostContentIterator& operator=(const PostContentIterator&) = delete;
  virtual ~PostContentIterator() = default;
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          PostContentIterator&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(PostContentIterator&);
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, PostContentIterator& aField,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(
      aCallback, static_cast<ContentIteratorBase&>(aField), aName, aFlags);
}

inline void ImplCycleCollectionUnlink(PostContentIterator& aField) {
  ImplCycleCollectionUnlink(static_cast<ContentIteratorBase&>(aField));
}

/**
 * A simple iterator class for traversing the content in "start tag" order.
 */
class PreContentIterator final : public ContentIteratorBase {
 public:
  PreContentIterator() : ContentIteratorBase(true) {}
  PreContentIterator(const PreContentIterator&) = delete;
  PreContentIterator& operator=(const PreContentIterator&) = delete;
  virtual ~PreContentIterator() = default;
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          PreContentIterator&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(PreContentIterator&);
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, PreContentIterator& aField,
    const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(
      aCallback, static_cast<ContentIteratorBase&>(aField), aName, aFlags);
}

inline void ImplCycleCollectionUnlink(PreContentIterator& aField) {
  ImplCycleCollectionUnlink(static_cast<ContentIteratorBase&>(aField));
}

/**
 *  A simple iterator class for traversing the content in "top subtree" order.
 */
class ContentSubtreeIterator final : public ContentIteratorBase {
 public:
  ContentSubtreeIterator() : ContentIteratorBase(true) {}
  ContentSubtreeIterator(const ContentSubtreeIterator&) = delete;
  ContentSubtreeIterator& operator=(const ContentSubtreeIterator&) = delete;
  virtual ~ContentSubtreeIterator() = default;

  virtual nsresult Init(nsINode* aRoot) override;
  virtual nsresult Init(nsRange* aRange) override;
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override;
  virtual nsresult Init(const RawRangeBoundary& aStartBoundary,
                        const RawRangeBoundary& aEndBoundary) override;

  virtual void Next() override;
  virtual void Prev() override;
  // Must override these because we don't do PositionAt
  virtual void First() override;
  // Must override these because we don't do PositionAt
  virtual void Last() override;

  virtual nsresult PositionAt(nsINode* aCurNode) override;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          ContentSubtreeIterator&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(ContentSubtreeIterator&);

 protected:
  /**
   * Callers must guarantee that mRange isn't nullptr and is positioned.
   */
  nsresult InitWithRange();

  // Returns the highest inclusive ancestor of aNode that's in the range
  // (possibly aNode itself).  Returns null if aNode is null, or is not itself
  // in the range.  A node is in the range if (node, 0) comes strictly after
  // the range endpoint, and (node, node.length) comes strictly before it, so
  // the range's start and end nodes will never be considered "in" it.
  nsIContent* GetTopAncestorInRange(nsINode* aNode);

  RefPtr<nsRange> mRange;

  AutoTArray<nsIContent*, 8> mEndNodes;
};

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    ContentSubtreeIterator& aField, const char* aName, uint32_t aFlags = 0) {
  ImplCycleCollectionTraverse(aCallback, aField.mRange, aName, aFlags);
  ImplCycleCollectionTraverse(
      aCallback, static_cast<ContentIteratorBase&>(aField), aName, aFlags);
}

inline void ImplCycleCollectionUnlink(ContentSubtreeIterator& aField) {
  ImplCycleCollectionUnlink(aField.mRange);
  ImplCycleCollectionUnlink(static_cast<ContentIteratorBase&>(aField));
}

}  // namespace mozilla

#endif  // #ifndef mozilla_ContentIterator_h
