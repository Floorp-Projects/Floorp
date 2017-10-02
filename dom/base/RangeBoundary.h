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

namespace mozilla {

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
      mOffset.reset();
    }
  }

  RangeBoundaryBase(nsINode* aContainer, int32_t aOffset)
    : mParent(aContainer)
    , mRef(nullptr)
    , mOffset(mozilla::Some(aOffset))
  {
    if (mParent && mParent->IsContainerNode()) {
      // Find a reference node
      if (aOffset == static_cast<int32_t>(aContainer->GetChildCount())) {
        mRef = aContainer->GetLastChild();
      } else if (aOffset != 0) {
        mRef = mParent->GetChildAt(aOffset - 1);
        MOZ_ASSERT(mRef);
      }

      MOZ_ASSERT_IF(!mRef, aOffset == 0);
    }

    MOZ_ASSERT_IF(mRef, mRef->GetParentNode() == mParent);
  }

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
    if (!mRef) {
      MOZ_ASSERT(Offset() == 0);
      return mParent->GetFirstChild();
    }
    MOZ_ASSERT(mParent->GetChildAt(Offset()) == mRef->GetNextSibling());
    return mRef->GetNextSibling();
  }

  uint32_t
  Offset() const
  {
    if (mOffset.isSome()) {
      return mOffset.value();
    }

    if (!mParent) {
      return 0;
    }

    MOZ_ASSERT(mRef);
    MOZ_ASSERT(mRef->GetParentNode() == mParent);
    mOffset = mozilla::Some(mParent->IndexOf(mRef) + 1);

    return mOffset.value();
  }

  void
  InvalidateOffset()
  {
    MOZ_ASSERT(mParent);
    MOZ_ASSERT(mParent->IsContainerNode(), "Range is positioned on a text node!");

    if (!mRef) {
      MOZ_ASSERT(mOffset.isSome() && mOffset.value() == 0);
      return;
    }
    mOffset.reset();
  }

  void
  Set(nsINode* aContainer, int32_t aOffset)
  {
    mParent = aContainer;
    if (mParent && mParent->IsContainerNode()) {
      // Find a reference node
      if (aOffset == static_cast<int32_t>(aContainer->GetChildCount())) {
        mRef = aContainer->GetLastChild();
      } else if (aOffset == 0) {
        mRef = nullptr;
      } else {
        mRef = mParent->GetChildAt(aOffset - 1);
        MOZ_ASSERT(mRef);
      }

      MOZ_ASSERT_IF(!mRef, aOffset == 0);
    } else {
      mRef = nullptr;
    }

    mOffset = mozilla::Some(aOffset);
    MOZ_ASSERT_IF(mRef, mRef->GetParentNode() == mParent);
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

    if (Ref()) {
      return Ref()->GetParentNode() == Container();
    }
    return Offset() <= Container()->Length();
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
    return mParent == aOther.mParent &&
      (mRef ? mRef == aOther.mRef : mOffset == aOther.mOffset);
  }

  template<typename A, typename B>
  bool operator!=(const RangeBoundaryBase<A, B>& aOther) const
  {
    return !(*this == aOther);
  }

private:
  ParentType mParent;
  RefType mRef;

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
