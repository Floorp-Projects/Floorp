/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef mozilla_EditorDOMPoint_h
#define mozilla_EditorDOMPoint_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RangeBoundary.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsINode.h"

namespace mozilla {

template<typename ParentType, typename ChildType>
class EditorDOMPointBase;

/**
 * EditorDOMPoint and EditorRawDOMPoint are simple classes which refers
 * a point in the DOM tree at creating the instance or initializing the
 * instance with calling Set().
 *
 * EditorDOMPoint refers container node (and child node if it's already set)
 * with nsCOMPtr.  EditorRawDOMPoint refers them with raw pointer.
 * So, EditorRawDOMPoint is useful when you access the nodes only before
 * changing DOM tree since increasing refcount may appear in micro benchmark
 * if it's in a hot path.  On the other hand, if you need to refer them even
 * after changing DOM tree, you must use EditorDOMPoint.
 *
 * When initializing an instance only with child node or offset,  the instance
 * starts to refer the child node or offset in the container.  In this case,
 * the other information hasn't been initialized due to performance reason.
 * When you retrieve the other information with calling Offset() or
 * GetChildAtOffset(), the other information is computed with the current
 * DOM tree.  Therefore, e.g., in the following case, the other information
 * may be different:
 *
 * EditorDOMPoint pointA(container1, childNode1);
 * EditorDOMPoint pointB(container1, childNode1);
 * Unused << pointA.Offset(); // The offset is computed now.
 * container1->RemoveChild(childNode1->GetPreviousSibling());
 * Unused << pointB.Offset(); // Now, pointB.Offset() equals pointA.Offset() - 1
 *
 * similarly:
 *
 * EditorDOMPoint pointA(container1, 5);
 * EditorDOMPoint pointB(container1, 5);
 * Unused << pointA.GetChildAtOffset(); // The child is computed now.
 * container1->RemoveChild(childNode1->GetFirstChild());
 * Unused << pointB.GetChildAtOffset(); // Now, pointB.GetChildAtOffset() equals
 *                                      // pointA.GetChildAtOffset()->
 *                                      //          GetPreviousSibling().
 *
 * So, when you initialize an instance only with one information, you need to
 * be careful when you access the other information after changing the DOM tree.
 * When you need to lock the child node or offset and recompute the other
 * information with new DOM tree, you can use
 * AutoEditorDOMPointOffsetInvalidator and AutoEditorDOMPointChildInvalidator.
 */

typedef EditorDOMPointBase<nsCOMPtr<nsINode>,
                           nsCOMPtr<nsIContent>> EditorDOMPoint;
typedef EditorDOMPointBase<nsINode*, nsIContent*> EditorRawDOMPoint;

template<typename ParentType, typename ChildType>
class EditorDOMPointBase final
{
  typedef EditorDOMPointBase<ParentType, ChildType> SelfType;

public:
  EditorDOMPointBase()
    : mParent(nullptr)
    , mChild(nullptr)
    , mIsChildInitialized(false)
  {
  }

  EditorDOMPointBase(nsINode* aContainer,
                     int32_t aOffset)
    : mParent(aContainer)
    , mChild(nullptr)
    , mOffset(mozilla::Some(aOffset))
    , mIsChildInitialized(false)
  {
    NS_WARNING_ASSERTION(!mParent || mOffset.value() <= mParent->Length(),
      "The offset is larger than the length of aContainer or negative");
    if (!mParent) {
      mOffset.reset();
    }
  }

  EditorDOMPointBase(nsIDOMNode* aDOMContainer,
                     int32_t aOffset)
    : mIsChildInitialized(false)
  {
    nsCOMPtr<nsINode> container = do_QueryInterface(aDOMContainer);
    this->Set(container, aOffset);
  }

  /**
   * Different from RangeBoundary, aPointedNode should be a child node
   * which you want to refer.
   */
  explicit EditorDOMPointBase(nsINode* aPointedNode)
    : mParent(aPointedNode && aPointedNode->IsContent() ?
                aPointedNode->GetParentNode() : nullptr)
    , mChild(aPointedNode && aPointedNode->IsContent() ?
                aPointedNode->AsContent() : nullptr)
    , mIsChildInitialized(false)
  {
    NS_WARNING_ASSERTION(IsSet(),
      "The child is nullptr or doesn't have its parent");
    NS_WARNING_ASSERTION(mChild && mChild->GetParentNode() == mParent,
      "Initializing RangeBoundary with invalid value");
    mIsChildInitialized = aPointedNode && mChild;
  }

  EditorDOMPointBase(nsINode* aContainer,
                     nsIContent* aPointedNode,
                     int32_t aOffset)
    : mParent(aContainer)
    , mChild(aPointedNode)
    , mOffset(mozilla::Some(aOffset))
    , mIsChildInitialized(true)
  {
    MOZ_RELEASE_ASSERT(aContainer,
      "This constructor shouldn't be used when pointing nowhere");
    MOZ_ASSERT(mOffset.value() <= mParent->Length());
    MOZ_ASSERT(mChild || mParent->Length() == mOffset.value());
    MOZ_ASSERT(!mChild || mParent == mChild->GetParentNode());
    MOZ_ASSERT(mParent->GetChildAt(mOffset.value()) == mChild);
  }

  template<typename PT, typename CT>
  explicit EditorDOMPointBase(const RangeBoundaryBase<PT, CT>& aOther)
    : mParent(aOther.mParent)
    , mChild(aOther.mRef ? aOther.mRef->GetNextSibling() :
                           (aOther.mParent ? aOther.mParent->GetFirstChild() :
                                             nullptr))
    , mOffset(aOther.mOffset)
    , mIsChildInitialized(aOther.mRef ||
                          (aOther.mOffset.isSome() && !aOther.mOffset.value()))
  {
  }

  template<typename PT, typename CT>
  explicit EditorDOMPointBase(const EditorDOMPointBase<PT, CT>& aOther)
    : mParent(aOther.mParent)
    , mChild(aOther.mChild)
    , mOffset(aOther.mOffset)
    , mIsChildInitialized(aOther.mIsChildInitialized)
  {
  }

  // Following methods are just copy of same methods of RangeBoudnaryBase.

  nsINode*
  Container() const
  {
    return mParent;
  }

  nsIContent*
  GetChildAtOffset() const
  {
    if (!mParent || !mParent->IsContainerNode()) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild;
  }

  /**
   * GetNextSiblingOfChildOffset() returns next sibling of a child at offset.
   * If this refers after the last child or the container cannot have children,
   * this returns nullptr with warning.
   */
  nsIContent*
  GetNextSiblingOfChildAtOffset() const
  {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild ? mChild->GetNextSibling() : nullptr;
    }
    MOZ_ASSERT(mOffset.isSome());
    if (NS_WARN_IF(mOffset.value() > mParent->Length())) {
      // If this has been set only offset and now the offset is invalid,
      // let's just return nullptr.
      return nullptr;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild ? mChild->GetNextSibling() : nullptr;
  }

  /**
   * GetPreviousSiblingOfChildAtOffset() returns previous sibling of a child
   * at offset.  If this refers the first child or the container cannot have
   * children, this returns nullptr with warning.
   */
  nsIContent*
  GetPreviousSiblingOfChildAtOffset() const
  {
    if (NS_WARN_IF(!mParent) || NS_WARN_IF(!mParent->IsContainerNode())) {
      return nullptr;
    }
    if (mIsChildInitialized) {
      return mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
    }
    MOZ_ASSERT(mOffset.isSome());
    if (NS_WARN_IF(mOffset.value() > mParent->Length())) {
      // If this has been set only offset and now the offset is invalid,
      // let's just return nullptr.
      return nullptr;
    }
    // Fix child node now.
    const_cast<SelfType*>(this)->EnsureChild();
    return mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
  }

  uint32_t
  Offset() const
  {
    if (mOffset.isSome()) {
      MOZ_ASSERT(mOffset.isSome());
      return mOffset.value();
    }
    if (!mParent) {
      MOZ_ASSERT(!mChild);
      return 0;
    }
    MOZ_ASSERT(mParent->IsContainerNode(),
      "If the container cannot have children, mOffset.isSome() should be true");
    if (!mChild) {
      // We're referring after the last child.  Fix offset now.
      const_cast<SelfType*>(this)->mOffset = mozilla::Some(mParent->Length());
      return mOffset.value();
    }
    MOZ_ASSERT(mChild->GetParentNode() == mParent);
    // Fix offset now.
    if (mChild == mParent->GetFirstChild()) {
      const_cast<SelfType*>(this)->mOffset = mozilla::Some(0);
    } else {
      const_cast<SelfType*>(this)->mOffset =
        mozilla::Some(mParent->IndexOf(mChild));
    }
    return mOffset.value();
  }

  /**
   * Set() sets a point to aOffset or aChild.
   * If it's set with aOffset, mChild is invalidated.  If it's set with aChild,
   * mOffset may be invalidated.
   */
  void
  Set(nsINode* aContainer, int32_t aOffset)
  {
    mParent = aContainer;
    mChild = nullptr;
    mOffset = mozilla::Some(aOffset);
    mIsChildInitialized = false;
    NS_WARNING_ASSERTION(!mParent || mOffset.value() <= mParent->Length(),
      "The offset is out of bounds");
  }
  void
  Set(const nsINode* aChild)
  {
    MOZ_ASSERT(aChild);
    if (NS_WARN_IF(!aChild->IsContent())) {
      Clear();
      return;
    }
    mParent = aChild->GetParentNode();
    mChild = const_cast<nsIContent*>(aChild->AsContent());
    mOffset.reset();
    mIsChildInitialized = true;
  }

  /**
   * Clear() makes the instance not point anywhere.
   */
  void
  Clear()
  {
    mParent = nullptr;
    mChild = nullptr;
    mOffset.reset();
    mIsChildInitialized = false;
  }

  /**
   * AdvanceOffset() tries to refer next sibling of mChild and/of next offset.
   * If the container can have children and there is no next sibling or the
   * offset reached the length of the container, this outputs warning and does
   * nothing.  So, callers need to check if there is next sibling which you
   * need to refer.
   *
   * @return            true if there is a next DOM point to refer.
   */
  bool
  AdvanceOffset()
  {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    // If only mOffset is available, just compute the offset.
    if ((mOffset.isSome() && !mIsChildInitialized) ||
        !mParent->IsContainerNode()) {
      MOZ_ASSERT(mOffset.isSome());
      MOZ_ASSERT(!mChild);
      if (NS_WARN_IF(mOffset.value() >= mParent->Length())) {
        // We're already referring the start of the container.
        return false;
      }
      mOffset = mozilla::Some(mOffset.value() + 1);
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized);
    MOZ_ASSERT(!mOffset.isSome() || mOffset.isSome());
    if (NS_WARN_IF(!mParent->HasChildren()) ||
        NS_WARN_IF(!mChild) ||
        NS_WARN_IF(mOffset.isSome() && mOffset.value() >= mParent->Length())) {
      // We're already referring the end of the container (or outside).
      return false;
    }

    if (mOffset.isSome()) {
      MOZ_ASSERT(mOffset.isSome());
      mOffset = mozilla::Some(mOffset.value() + 1);
    }
    mChild = mChild->GetNextSibling();
    return true;
  }

  /**
   * RewindOffset() tries to refer previous sibling of mChild and/or previous
   * offset.  If the container can have children and there is no next previous
   * or the offset is 0, this outputs warning and does nothing.  So, callers
   * need to check if there is previous sibling which you need to refer.
   *
   * @return            true if there is a previous DOM point to refer.
   */
  bool
  RewindOffset()
  {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    // If only mOffset is available, just compute the offset.
    if ((mOffset.isSome() && !mIsChildInitialized) ||
        !mParent->IsContainerNode()) {
      MOZ_ASSERT(mOffset.isSome());
      MOZ_ASSERT(!mChild);
      if (NS_WARN_IF(!mOffset.value()) ||
          NS_WARN_IF(mOffset.value() >= mParent->Length())) {
        // We're already referring the start of the container.
        return false;
      }
      mOffset = mozilla::Some(mOffset.value() - 1);
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized);
    MOZ_ASSERT(!mOffset.isSome() || mOffset.isSome());
    if (NS_WARN_IF(!mParent->HasChildren()) ||
        NS_WARN_IF(mChild && !mChild->GetPreviousSibling()) ||
        NS_WARN_IF(mOffset.isSome() && !mOffset.value())) {
      // We're already referring the start of the container (or the child has
      // been moved from the container?).
      return false;
    }

    nsIContent* previousSibling =
        mChild ? mChild->GetPreviousSibling() : mParent->GetLastChild();
    if (NS_WARN_IF(!previousSibling)) {
      // We're already referring the first child of the container.
      return false;
    }

    if (mOffset.isSome()) {
      mOffset = mozilla::Some(mOffset.value() - 1);
    }
    mChild = previousSibling;
    return true;
  }

  bool
  IsSet() const
  {
    return mParent && (mIsChildInitialized || mOffset.isSome());
  }

  bool
  IsSetAndValid() const
  {
    if (!IsSet()) {
      return false;
    }

    if (mChild && mChild->GetParentNode() != mParent) {
      return false;
    }
    if (mOffset.isSome() && mOffset.value() > mParent->Length()) {
      return false;
    }
    return true;
  }

  bool
  IsStartOfContainer() const
  {
    // If we're referring the first point in the container:
    //   If mParent is not a container like a text node, mOffset is 0.
    //   If mChild is initialized and it's first child of mParent.
    //   If mChild isn't initialized and the offset is 0.
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (!mParent->IsContainerNode()) {
      return !mOffset.value();
    }
    if (mIsChildInitialized) {
      NS_WARNING_ASSERTION(!mOffset.isSome() || !mOffset.value(),
        "If offset was initialized, mOffset should be 0");
      return mParent->GetFirstChild() == mChild;
    }
    MOZ_ASSERT(mOffset.isSome());
    return !mOffset.value();
  }

  bool
  IsEndOfContainer() const
  {
    // If we're referring after the last point of the container:
    //   If mParent is not a container like text node, mOffset is same as the
    //   length of the container.
    //   If mChild is initialized and it's nullptr.
    //   If mChild isn't initialized and mOffset is same as the length of the
    //   container.
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    if (mIsChildInitialized) {
      NS_WARNING_ASSERTION(!mOffset.isSome() ||
                           mOffset.value() == mParent->Length(),
        "If offset was initialized, mOffset should be length of the container");
      return !mChild;
    }
    MOZ_ASSERT(mOffset.isSome());
    return mOffset.value() == mParent->Length();
  }

  // Convenience methods for switching between the two types
  // of EditorDOMPointBase.
  EditorDOMPointBase<nsINode*, nsIContent*>
  AsRaw() const
  {
    return EditorRawDOMPoint(*this);
  }

  template<typename A, typename B>
  EditorDOMPointBase& operator=(const RangeBoundaryBase<A,B>& aOther)
  {
    mParent = aOther.mParent;
    mChild =
      aOther.mRef ? aOther.mRef->GetNextSibling() :
                    (aOther.mParent && aOther.mParent->IsContainerNode() ?
                       aOther.mParent->GetFirstChild() : nullptr);
    mOffset = aOther.mOffset;
    mIsChildInitialized =
      aOther.mRef ||
      (aOther.mParent && !aOther.mParent->IsContainerNode()) ||
      (aOther.mOffset.isSome() && !aOther.mOffset.value());
    return *this;
  }

  template<typename A, typename B>
  EditorDOMPointBase& operator=(const EditorDOMPointBase<A, B>& aOther)
  {
    mParent = aOther.mParent;
    mChild = aOther.mChild;
    mOffset = aOther.mOffset;
    mIsChildInitialized = aOther.mIsChildInitialized;
    return *this;
  }

  template<typename A, typename B>
  bool operator==(const EditorDOMPointBase<A, B>& aOther) const
  {
    if (mParent != aOther.mParent) {
      return false;
    }

    if (mOffset.isSome() && aOther.mOffset.isSome()) {
      // If both mOffset are set, we need to compare both mRef too because
      // the relation of mRef and mOffset have already broken by DOM tree
      // changes.
      if (mOffset != aOther.mOffset) {
        return false;
      }
      if (mChild == aOther.mChild) {
        return true;
      }
      if (NS_WARN_IF(mIsChildInitialized && aOther.mIsChildInitialized)) {
        // In this case, relation between mChild and mOffset of one of or both
        // of them doesn't match with current DOM tree since the DOM tree might
        // have been changed after computing mChild or mOffset.
        return false;
      }
      // If one of mChild hasn't been computed yet, we should compare them only
      // with mOffset.  Perhaps, we shouldn't copy mChild from non-nullptr one
      // to the other since if we copy it here, it may be unexpected behavior
      // for some callers.
      return true;
    }

    MOZ_ASSERT(mIsChildInitialized || aOther.mIsChildInitialized);

    if (mOffset.isSome() && !mIsChildInitialized &&
        !aOther.mOffset.isSome() && aOther.mIsChildInitialized) {
      // If this has only mOffset and the other has only mChild, this needs to
      // compute mChild now.
      const_cast<SelfType*>(this)->EnsureChild();
      return mChild == aOther.mChild;
    }

    if (!mOffset.isSome() && mIsChildInitialized &&
        aOther.mOffset.isSome() && !aOther.mIsChildInitialized) {
      // If this has only mChild and the other has only mOffset, the other needs
      // to compute mChild now.
      const_cast<EditorDOMPointBase<A, B>&>(aOther).EnsureChild();
      return mChild == aOther.mChild;
    }

    // If mOffset of one of them hasn't been computed from mChild yet, we should
    // compare only with mChild.  Perhaps, we shouldn't copy mOffset from being
    // some one to not being some one since if we copy it here, it may be
    // unexpected behavior for some callers.
    return mChild == aOther.mChild;
  }

  template<typename A, typename B>
  bool operator!=(const EditorDOMPointBase<A, B>& aOther) const
  {
    return !(*this == aOther);
  }

  /**
   * This operator should be used if API of other modules take RawRangeBoundary,
   * e.g., methods of Selection and nsRange.
   */
  operator const RawRangeBoundary() const
  {
    if (!IsSet() ||
        NS_WARN_IF(!mIsChildInitialized && !mOffset.isSome())) {
      return RawRangeBoundary();
    }
    if (!mParent->IsContainerNode()) {
      // If the container is a data node like a text node, we need to create
      // RangeBoundaryBase instance only with mOffset because mChild is always
      // nullptr.
      return RawRangeBoundary(mParent, mOffset.value());
    }
    if (mIsChildInitialized && mOffset.isSome()) {
      // If we've already set both child and offset, we should use both of them
      // to create RangeBoundaryBase instance because the constructor will
      // validate the relation in debug build.
      if (mChild) {
        return RawRangeBoundary(mParent, mChild->GetPreviousSibling(),
                                mOffset.value());
      }
      return RawRangeBoundary(mParent, mParent->GetLastChild(),
                              mOffset.value());
    }
    // Otherwise, we should create RangeBoundaryBase only with available
    // information.
    if (mOffset.isSome()) {
      return RawRangeBoundary(mParent, mOffset.value());
    }
    if (mChild) {
      return RawRangeBoundary(mParent, mChild->GetPreviousSibling());
    }
    return RawRangeBoundary(mParent, mParent->GetLastChild());
  }

private:
  void
  EnsureChild()
  {
    if (mIsChildInitialized) {
      return;
    }
    if (!mParent) {
      MOZ_ASSERT(!mOffset.isSome());
      return;
    }
    MOZ_ASSERT(mOffset.isSome());
    MOZ_ASSERT(mOffset.value() <= mParent->Length());
    mIsChildInitialized = true;
    if (!mParent->IsContainerNode()) {
      return;
    }
    mChild = mParent->GetChildAt(mOffset.value());
    MOZ_ASSERT(mChild || mOffset.value() == mParent->Length());
  }

  ParentType mParent;
  ChildType mChild;

  mozilla::Maybe<uint32_t> mOffset;

  bool mIsChildInitialized;

  template<typename PT, typename CT>
  friend class EditorDOMPointBase;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          EditorDOMPoint&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(EditorDOMPoint&);
};

inline void
ImplCycleCollectionUnlink(EditorDOMPoint& aField)
{
  ImplCycleCollectionUnlink(aField.mParent);
  ImplCycleCollectionUnlink(aField.mChild);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            EditorDOMPoint& aField,
                            const char* aName,
                            uint32_t aFlags)
{
  ImplCycleCollectionTraverse(aCallback, aField.mParent, "mParent", 0);
  ImplCycleCollectionTraverse(aCallback, aField.mChild, "mChild", 0);
}

/**
 * AutoEditorDOMPointOffsetInvalidator is useful if DOM tree will be changed
 * when EditorDOMPoint instance is available and keeps referring same child
 * node.
 *
 * This class automatically guarantees that given EditorDOMPoint instance
 * stores the child node and invalidates its offset when the instance is
 * destroyed.  Additionally, users of this class can invalidate the offset
 * manually when they need.
 */
class MOZ_STACK_CLASS AutoEditorDOMPointOffsetInvalidator final
{
public:
  explicit AutoEditorDOMPointOffsetInvalidator(EditorDOMPoint& aPoint)
    : mPoint(aPoint)
  {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    mChild = mPoint.GetChildAtOffset();
  }

  ~AutoEditorDOMPointOffsetInvalidator()
  {
    InvalidateOffset();
  }

  /**
   * Manually, invalidate offset of the given point.
   */
  void InvalidateOffset()
  {
    if (mChild) {
      mPoint.Set(mChild);
    } else {
      // If the point referred after the last child, let's keep referring
      // after current last node of the old container.
      mPoint.Set(mPoint.Container(), mPoint.Container()->Length());
    }
  }

private:
  EditorDOMPoint& mPoint;
  // Needs to store child node by ourselves because EditorDOMPoint stores
  // child node with mRef which is previous sibling of current child node.
  // Therefore, we cannot keep referring it if it's first child.
  nsCOMPtr<nsIContent> mChild;

  AutoEditorDOMPointOffsetInvalidator() = delete;
  AutoEditorDOMPointOffsetInvalidator(
    const AutoEditorDOMPointOffsetInvalidator& aOther) = delete;
  const AutoEditorDOMPointOffsetInvalidator& operator=(
    const AutoEditorDOMPointOffsetInvalidator& aOther) = delete;
};

/**
 * AutoEditorDOMPointChildInvalidator is useful if DOM tree will be changed
 * when EditorDOMPoint instance is available and keeps referring same container
 * and offset in it.
 *
 * This class automatically guarantees that given EditorDOMPoint instance
 * stores offset and invalidates its child node when the instance is destroyed.
 * Additionally, users of this class can invalidate the child manually when
 * they need.
 */
class MOZ_STACK_CLASS AutoEditorDOMPointChildInvalidator final
{
public:
  explicit AutoEditorDOMPointChildInvalidator(EditorDOMPoint& aPoint)
    : mPoint(aPoint)
  {
    MOZ_ASSERT(aPoint.IsSetAndValid());
    Unused << mPoint.Offset();
  }

  ~AutoEditorDOMPointChildInvalidator()
  {
    InvalidateChild();
  }

  /**
   * Manually, invalidate child of the given point.
   */
  void InvalidateChild()
  {
    mPoint.Set(mPoint.Container(), mPoint.Offset());
  }

private:
  EditorDOMPoint& mPoint;

  AutoEditorDOMPointChildInvalidator() = delete;
  AutoEditorDOMPointChildInvalidator(
    const AutoEditorDOMPointChildInvalidator& aOther) = delete;
  const AutoEditorDOMPointChildInvalidator& operator=(
    const AutoEditorDOMPointChildInvalidator& aOther) = delete;
};

} // namespace mozilla

#endif // #ifndef mozilla_EditorDOMPoint_h
