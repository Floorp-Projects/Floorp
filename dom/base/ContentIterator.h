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
#include "nsRange.h"
#include "nsTArray.h"

class nsIContent;
class nsINode;

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
  virtual ~ContentIteratorBase();

  /**
   * Allows to iterate over the inclusive descendants
   * (https://dom.spec.whatwg.org/#concept-tree-inclusive-descendant) of
   * aRoot.
   */
  virtual nsresult Init(nsINode* aRoot);

  virtual nsresult Init(dom::AbstractRange* aRange);
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset);
  virtual nsresult Init(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd);

  virtual void First();
  virtual void Last();
  virtual void Next();
  virtual void Prev();

  nsINode* GetCurrentNode() const { return mCurNode; }

  bool IsDone() const { return !mCurNode; }

  virtual nsresult PositionAt(nsINode* aCurNode);

 protected:
  enum class Order {
    Pre, /*!< <https://en.wikipedia.org/wiki/Tree_traversal#Pre-order_(NLR)>.
          */
    Post /*!< <https://en.wikipedia.org/wiki/Tree_traversal#Post-order_(LRN)>.
          */
  };

  explicit ContentIteratorBase(Order aOrder);

  class Initializer;

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
  static nsINode* GetDeepFirstChild(nsINode* aRoot);
  static nsIContent* GetDeepFirstChild(nsIContent* aRoot);
  static nsINode* GetDeepLastChild(nsINode* aRoot);
  static nsIContent* GetDeepLastChild(nsIContent* aRoot);

  // Get the next/previous sibling of aNode, or its parent's, or grandparent's,
  // etc.  Returns null if aNode and all its ancestors have no next/previous
  // sibling.
  static nsIContent* GetNextSibling(nsINode* aNode);
  static nsIContent* GetPrevSibling(nsINode* aNode);

  nsINode* NextNode(nsINode* aNode);
  nsINode* PrevNode(nsINode* aNode);

  void SetEmpty();

  nsCOMPtr<nsINode> mCurNode;
  nsCOMPtr<nsINode> mFirst;
  nsCOMPtr<nsINode> mLast;
  // See <https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor>.
  nsCOMPtr<nsINode> mClosestCommonInclusiveAncestor;

  const Order mOrder;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          ContentIteratorBase&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(ContentIteratorBase&);
};

/**
 * A simple iterator class for traversing the content in "close tag" order.
 */
class PostContentIterator final : public ContentIteratorBase {
 public:
  PostContentIterator() : ContentIteratorBase(Order::Post) {}
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
  PreContentIterator() : ContentIteratorBase(Order::Pre) {}
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
  ContentSubtreeIterator() : ContentIteratorBase(Order::Pre) {}
  ContentSubtreeIterator(const ContentSubtreeIterator&) = delete;
  ContentSubtreeIterator& operator=(const ContentSubtreeIterator&) = delete;
  virtual ~ContentSubtreeIterator() = default;

  /**
   * Not supported.
   */
  virtual nsresult Init(nsINode* aRoot) override;

  virtual nsresult Init(dom::AbstractRange* aRange) override;
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override;
  virtual nsresult Init(const RawRangeBoundary& aStartBoundary,
                        const RawRangeBoundary& aEndBoundary) override;

  void Next() override;
  void Prev() override;
  // Must override these because we don't do PositionAt
  void First() override;
  // Must override these because we don't do PositionAt
  void Last() override;

  nsresult PositionAt(nsINode* aCurNode) override;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          ContentSubtreeIterator&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(ContentSubtreeIterator&);

 private:
  /**
   * See <https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor>.
   */
  void CacheInclusiveAncestorsOfEndContainer();

  /**
   * @return may be nullptr.
   */
  nsIContent* DetermineCandidateForFirstContent() const;

  /**
   * @return may be nullptr.
   */
  nsIContent* DetermineCandidateForLastContent() const;

  /**
   * @return may be nullptr.
   */
  nsIContent* DetermineFirstContent() const;

  /**
   * @return may be nullptr.
   */
  nsIContent* DetermineLastContent() const;

  /**
   * Callers must guarantee that mRange isn't nullptr and is positioned.
   */
  nsresult InitWithRange();

  // Returns the highest inclusive ancestor of aNode that's in the range
  // (possibly aNode itself).  Returns null if aNode is null, or is not itself
  // in the range.  A node is in the range if (node, 0) comes strictly after
  // the range endpoint, and (node, node.length) comes strictly before it, so
  // the range's start and end nodes will never be considered "in" it.
  nsIContent* GetTopAncestorInRange(nsINode* aNode) const;

  RefPtr<dom::AbstractRange> mRange;

  // See <https://dom.spec.whatwg.org/#concept-tree-inclusive-ancestor>.
  AutoTArray<nsIContent*, 8> mInclusiveAncestorsOfEndContainer;
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
