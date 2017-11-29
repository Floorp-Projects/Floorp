/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RangeBoundary_h
#define mozilla_RangeBoundary_h

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "mozilla/Maybe.h"

class nsRange;

namespace mozilla {

template<typename T, typename U>
class EditorDOMPointBase;

// This class will maintain a reference to the child immediately
// before the boundary's offset. We try to avoid computing the
// offset as much as possible and just ensure mRef points to the
// correct child.
//
// mParent
//    |
// [child0] [child1] [child2]
//            /      |
//         mRef    mOffset=2
//
// If mOffset == 0, mRef is null.
// For text nodes, mRef will always be null and the offset will
// be kept up-to-date.

template<typename ParentType, typename RefType>
class RangeBoundaryBase;

typedef RangeBoundaryBase<nsCOMPtr<nsINode>, nsCOMPtr<nsIContent>> RangeBoundary;
typedef RangeBoundaryBase<nsINode*, nsIContent*> RawRangeBoundary;

// This class has two specializations, one using reference counting
// pointers and one using raw pointers. This helps us avoid unnecessary
// AddRef/Release calls.
template<typename ParentType, typename RefType>
class RangeBoundaryBase
{
  template<typename T, typename U>
  friend class RangeBoundaryBase;
  template<typename T, typename U>
  friend class EditorDOMPointBase;

  // nsRange needs to use InvalidOffset() which requires mRef initialized
  // before it's called.
  friend class ::nsRange;

  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback&,
                                          RangeBoundary&, const char*,
                                          uint32_t);
  friend void ImplCycleCollectionUnlink(RangeBoundary&);

public:
  RangeBoundaryBase(nsINode* aContainer, nsIContent* aRef)
    : mParent(aContainer)
    , mRef(aRef)
  {
    if (!mRef) {
      mOffset = mozilla::Some(0);
    } else {
      NS_WARNING_ASSERTION(mRef->GetParentNode() == mParent,
                           "Initializing RangeBoundary with invalid value");
      mOffset.reset();
    }
  }

  RangeBoundaryBase(nsINode* aContainer, int32_t aOffset)
    : mParent(aContainer)
    , mRef(nullptr)
    , mOffset(mozilla::Some(aOffset))
  {
    if (!mParent) {
      mOffset.reset();
    }
  }

protected:
  RangeBoundaryBase(nsINode* aContainer, nsIContent* aRef, int32_t aOffset)
    : mParent(aContainer)
    , mRef(aRef)
    , mOffset(mozilla::Some(aOffset))
  {
    MOZ_RELEASE_ASSERT(aContainer,
      "This constructor shouldn't be used when pointing nowhere");
    if (!mRef) {
      MOZ_ASSERT(!mParent->IsContainerNode() || mOffset.value() == 0);
      return;
    }
    MOZ_ASSERT(mOffset.value() > 0);
    MOZ_ASSERT(mParent == mRef->GetParentNode());
    MOZ_ASSERT(mParent->GetChildAt(mOffset.value() - 1) == mRef);
  }

public:
  RangeBoundaryBase()
    : mParent(nullptr)
    , mRef(nullptr)
  {
  }

  // Needed for initializing RawRangeBoundary from an existing RangeBoundary.
  template<typename PT, typename RT>
  explicit RangeBoundaryBase(const RangeBoundaryBase<PT, RT>& aOther)
    : mParent(aOther.mParent)
    , mRef(aOther.mRef)
    , mOffset(aOther.mOffset)
  {
  }

  nsIContent*
  Ref() const
  {
    EnsureRef();
    return mRef;
  }

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
    EnsureRef();
    if (!mRef) {
      MOZ_ASSERT(Offset() == 0, "invalid RangeBoundary");
      return mParent->GetFirstChild();
    }
    MOZ_ASSERT(mParent->GetChildAt(Offset()) == mRef->GetNextSibling());
    return mRef->GetNextSibling();
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
    EnsureRef();
    if (NS_WARN_IF(!mRef->GetNextSibling())) {
      // Already referring the end of the container.
      return nullptr;
    }
    return mRef->GetNextSibling()->GetNextSibling();
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
    EnsureRef();
    if (NS_WARN_IF(!mRef)) {
      // Already referring the start of the container.
      return nullptr;
    }
    return mRef;
  }

  uint32_t
  Offset() const
  {
    if (mOffset.isSome()) {
      return mOffset.value();
    }
    if (!mParent) {
      MOZ_ASSERT(!mRef);
      return 0;
    }
    MOZ_ASSERT(mParent->IsContainerNode(),
      "If the container cannot have children, mOffset.isSome() should be true");
    MOZ_ASSERT(mRef);
    MOZ_ASSERT(mRef->GetParentNode() == mParent);
    if (!mRef->GetPreviousSibling()) {
      mOffset = mozilla::Some(1);
      return mOffset.value();
    }
    if (!mRef->GetNextSibling()) {
      mOffset = mozilla::Some(mParent->GetChildCount());
      return mOffset.value();
    }
    // Use nsINode::IndexOf() as the last resort due to being expensive.
    mOffset = mozilla::Some(mParent->IndexOf(mRef) + 1);
    return mOffset.value();
  }

  /**
   * Set() sets a point to aOffset or aChild.
   * If it's set with offset, mRef is invalidated.  If it's set with aChild,
   * mOffset may be invalidated unless the offset can be computed simply.
   */
  void
  Set(nsINode* aContainer, int32_t aOffset)
  {
    mParent = aContainer;
    mRef = nullptr;
    mOffset = mozilla::Some(aOffset);
  }
  void
  Set(const nsINode* aChild)
  {
    MOZ_ASSERT(aChild);
    if (!aChild->IsContent()) {
      Clear();
      return;
    }
    mParent = aChild->GetParentNode();
    mRef = aChild->GetPreviousSibling();
    if (!mRef) {
      mOffset = mozilla::Some(0);
    } else {
      mOffset.reset();
    }
  }

  /**
   * Clear() makes the instance not point anywhere.
   */
  void
  Clear()
  {
    mParent = nullptr;
    mRef = nullptr;
    mOffset.reset();
  }

  /**
   * AdvanceOffset() tries to reference next sibling of mRef if its container
   * can have children or increments offset if the container is a text node or
   * something.
   * If the container can have children and there is no next sibling, this
   * outputs warning and does nothing.  So, callers need to check if there is
   * next sibling which you need to refer.
   *
   * @return            true if there is a next sibling to refer.
   */
  bool
  AdvanceOffset()
  {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    EnsureRef();
    if (!mRef) {
      if (!mParent->IsContainerNode()) {
        // In text node or something, just increment the offset.
        MOZ_ASSERT(mOffset.isSome());
        if (NS_WARN_IF(mOffset.value() == mParent->Length())) {
          // Already referring the end of the node.
          return false;
        }
        mOffset = mozilla::Some(mOffset.value() + 1);
        return true;
      }
      mRef = mParent->GetFirstChild();
      if (NS_WARN_IF(!mRef)) {
        // No children in the container.
        mOffset = mozilla::Some(0);
        return false;
      }
      mOffset = mozilla::Some(1);
      return true;
    }

    nsIContent* nextSibling = mRef->GetNextSibling();
    if (NS_WARN_IF(!nextSibling)) {
      // Already referring the end of the container.
      return false;
    }
    mRef = nextSibling;
    if (mOffset.isSome()) {
      mOffset = mozilla::Some(mOffset.value() + 1);
    }
    return true;
  }

  /**
   * RewindOffset() tries to reference next sibling of mRef if its container
   * can have children or decrements offset if the container is a text node or
   * something.
   * If the container can have children and there is no next previous, this
   * outputs warning and does nothing.  So, callers need to check if there is
   * previous sibling which you need to refer.
   *
   * @return            true if there is a previous sibling to refer.
   */
  bool
  RewindOffset()
  {
    if (NS_WARN_IF(!mParent)) {
      return false;
    }
    EnsureRef();
    if (!mRef) {
      if (NS_WARN_IF(mParent->IsContainerNode())) {
        // Already referring the start of the container
        mOffset = mozilla::Some(0);
        return false;
      }
      // In text node or something, just decrement the offset.
      MOZ_ASSERT(mOffset.isSome());
      if (NS_WARN_IF(mOffset.value() == 0)) {
        // Already referring the start of the node.
        return false;
      }
      mOffset = mozilla::Some(mOffset.value() - 1);
      return true;
    }

    mRef = mRef->GetPreviousSibling();
    if (mOffset.isSome()) {
      mOffset = mozilla::Some(mOffset.value() - 1);
    }
    return true;
  }

  void
  SetAfterRef(nsINode* aParent, nsIContent* aRef)
  {
    mParent = aParent;
    mRef = aRef;
    if (!mRef) {
      mOffset = mozilla::Some(0);
    } else {
      mOffset.reset();
    }
  }

  bool
  IsSet() const
  {
    return mParent && (mRef || mOffset.isSome());
  }

  bool
  IsSetAndValid() const
  {
    if (!IsSet()) {
      return false;
    }

    if (mRef && mRef->GetParentNode() != mParent) {
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
    // We're at the first point in the container if we don't have a reference,
    // and our offset is 0. If we don't have a Ref, we should already have an
    // offset, so we can just directly fetch it.
    return !Ref() && mOffset.value() == 0;
  }

  bool
  IsEndOfContainer() const
  {
    // We're at the last point in the container if Ref is a pointer to the last
    // child in Container(), or our Offset() is the same as the length of our
    // container. If we don't have a Ref, then we should already have an offset,
    // so we can just directly fetch it.
    return Ref()
      ? !Ref()->GetNextSibling()
      : mOffset.value() == Container()->Length();
  }

  // Convenience methods for switching between the two types
  // of RangeBoundary.
  RangeBoundaryBase<nsINode*, nsIContent*>
  AsRaw() const
  {
    return RangeBoundaryBase<nsINode*, nsIContent*>(*this);
  }

  template<typename A, typename B>
  RangeBoundaryBase& operator=(const RangeBoundaryBase<A,B>& aOther)
  {
    mParent = aOther.mParent;
    mRef = aOther.mRef;
    mOffset = aOther.mOffset;
    return *this;
  }

  template<typename A, typename B>
  bool operator==(const RangeBoundaryBase<A, B>& aOther) const
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
      if (mRef == aOther.mRef) {
        return true;
      }
      if (NS_WARN_IF(mRef && aOther.mRef)) {
        // In this case, relation between mRef and mOffset of one of or both of
        // them doesn't match with current DOM tree since the DOM tree might
        // have been changed after computing mRef or mOffset.
        return false;
      }
      // If one of mRef hasn't been computed yet, we should compare them only
      // with mOffset.  Perhaps, we shouldn't copy mRef from non-nullptr one to
      // nullptr one since if we copy it here, it may be unexpected behavior for
      // some callers.
      return true;
    }

    if (mOffset.isSome() && !mRef &&
        !aOther.mOffset.isSome() && aOther.mRef) {
      // If this has only mOffset and the other has only mRef, this needs to
      // compute mRef now.
      EnsureRef();
      return mRef == aOther.mRef;
    }

    if (!mOffset.isSome() && mRef &&
        aOther.mOffset.isSome() && !aOther.mRef) {
      // If this has only mRef and the other has only mOffset, the other needs
      // to compute mRef now.
      aOther.EnsureRef();
      return mRef == aOther.mRef;
    }

    // If mOffset of one of them hasn't been computed from mRef yet, we should
    // compare only with mRef.  Perhaps, we shouldn't copy mOffset from being
    // some one to not being some one since if we copy it here, it may be
    // unexpected behavior for some callers.
    return mRef == aOther.mRef;
  }

  template<typename A, typename B>
  bool operator!=(const RangeBoundaryBase<A, B>& aOther) const
  {
    return !(*this == aOther);
  }

protected:
  /**
   * InvalidOffset() is error prone method, unfortunately.  If somebody
   * needs to call this method, it needs to call EnsureRef() before changing
   * the position of the referencing point.
   */
  void
  InvalidateOffset()
  {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsContainerNode(),
               "Range is positioned on a text node!");
    MOZ_ASSERT(mRef || (mOffset.isSome() && mOffset.value() == 0),
               "mRef should be computed before a call of InvalidateOffset()");

    if (!mRef) {
      return;
    }
    mOffset.reset();
  }

  void
  EnsureRef() const
  {
    if (mRef) {
      return;
    }
    if (!mParent) {
      MOZ_ASSERT(!mOffset.isSome());
      return;
    }
    MOZ_ASSERT(mOffset.isSome());
    MOZ_ASSERT(mOffset.value() <= mParent->Length());
    if (!mParent->IsContainerNode() ||
        mOffset.value() == 0) {
      return;
    }
    mRef = mParent->GetChildAt(mOffset.value() - 1);
    MOZ_ASSERT(mRef);
  }

private:
  ParentType mParent;
  mutable RefType mRef;

  mutable mozilla::Maybe<uint32_t> mOffset;
};

inline void
ImplCycleCollectionUnlink(RangeBoundary& aField)
{
  ImplCycleCollectionUnlink(aField.mParent);
  ImplCycleCollectionUnlink(aField.mRef);
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            RangeBoundary& aField,
                            const char* aName,
                            uint32_t aFlags)
{
  ImplCycleCollectionTraverse(aCallback, aField.mParent, "mParent", 0);
  ImplCycleCollectionTraverse(aCallback, aField.mRef, "mRef", 0);
}

} // namespace mozilla

#endif // defined(mozilla_RangeBoundary_h)
