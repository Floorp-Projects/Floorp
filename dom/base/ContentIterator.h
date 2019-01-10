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
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsTArray.h"

namespace mozilla {

/**
 * ContentIteratorBase is a base class of PostContentIterator,
 * PreContentIterator and ContentSubtreeIterator.  Making each concrete
 * classes "final", compiler can avoid virtual calls if they are treated
 * by the users directly.
 */
class ContentIteratorBase : public nsIContentIterator {
 public:
  ContentIteratorBase() = delete;
  ContentIteratorBase(const ContentIteratorBase&) = delete;
  ContentIteratorBase& operator=(const ContentIteratorBase&) = delete;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ContentIteratorBase)

  virtual nsresult Init(nsINode* aRoot) override;
  virtual nsresult Init(nsRange* aRange) override;
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override;
  virtual nsresult Init(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd) override;

  virtual void First() override;
  virtual void Last() override;
  virtual void Next() override;
  virtual void Prev() override;

  virtual nsINode* GetCurrentNode() override;

  virtual bool IsDone() override;

  virtual nsresult PositionAt(nsINode* aCurNode) override;

 protected:
  explicit ContentIteratorBase(bool aPre);
  virtual ~ContentIteratorBase() = default;

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

  virtual void LastRelease();

  nsCOMPtr<nsINode> mCurNode;
  nsCOMPtr<nsINode> mFirst;
  nsCOMPtr<nsINode> mLast;
  nsCOMPtr<nsINode> mCommonParent;

  bool mIsDone;
  bool mPre;
};

/**
 * A simple iterator class for traversing the content in "close tag" order.
 */
class PostContentIterator final : public ContentIteratorBase {
 public:
  PostContentIterator() : ContentIteratorBase(false) {}
  PostContentIterator(const PostContentIterator&) = delete;
  PostContentIterator& operator=(const PostContentIterator&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PostContentIterator,
                                           ContentIteratorBase)

 protected:
  virtual ~PostContentIterator() = default;
};

/**
 * A simple iterator class for traversing the content in "start tag" order.
 */
class PreContentIterator final : public ContentIteratorBase {
 public:
  PreContentIterator() : ContentIteratorBase(true) {}
  PreContentIterator(const PreContentIterator&) = delete;
  PreContentIterator& operator=(const PreContentIterator&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PreContentIterator,
                                           ContentIteratorBase)

 protected:
  virtual ~PreContentIterator() = default;
};

/**
 *  A simple iterator class for traversing the content in "top subtree" order.
 */
class ContentSubtreeIterator final : public ContentIteratorBase {
 public:
  ContentSubtreeIterator() : ContentIteratorBase(true) {}
  ContentSubtreeIterator(const ContentSubtreeIterator&) = delete;
  ContentSubtreeIterator& operator=(const ContentSubtreeIterator&) = delete;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ContentSubtreeIterator,
                                           ContentIteratorBase)

  virtual nsresult Init(nsINode* aRoot) override;
  virtual nsresult Init(nsRange* aRange) override;
  virtual nsresult Init(nsINode* aStartContainer, uint32_t aStartOffset,
                        nsINode* aEndContainer, uint32_t aEndOffset) override;
  virtual nsresult Init(const RawRangeBoundary& aStart,
                        const RawRangeBoundary& aEnd) override;

  virtual void Next() override;
  virtual void Prev() override;
  // Must override these because we don't do PositionAt
  virtual void First() override;
  // Must override these because we don't do PositionAt
  virtual void Last() override;

  virtual nsresult PositionAt(nsINode* aCurNode) override;

 protected:
  virtual ~ContentSubtreeIterator() = default;

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

  virtual void LastRelease() override;

  RefPtr<nsRange> mRange;

  AutoTArray<nsIContent*, 8> mEndNodes;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_ContentIterator_h
