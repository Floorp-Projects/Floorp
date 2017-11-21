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

template<typename ParentType, typename RefType>
class EditorDOMPointBase;

typedef EditorDOMPointBase<nsCOMPtr<nsINode>,
                           nsCOMPtr<nsIContent>> EditorDOMPoint;
typedef EditorDOMPointBase<nsINode*, nsIContent*> EditorRawDOMPoint;

template<typename ParentType, typename RefType>
class MOZ_STACK_CLASS EditorDOMPointBase final
                        : public RangeBoundaryBase<ParentType, RefType>
{
public:
  EditorDOMPointBase()
    : RangeBoundaryBase<ParentType, RefType>()
  {
  }

  EditorDOMPointBase(nsINode* aConatiner,
                     int32_t aOffset)
    : RangeBoundaryBase<ParentType, RefType>(aConatiner,
                                             aOffset)
  {
  }

  EditorDOMPointBase(nsIDOMNode* aDOMContainer,
                     int32_t aOffset)
    : RangeBoundaryBase<ParentType, RefType>()
  {
    nsCOMPtr<nsINode> container = do_QueryInterface(aDOMContainer);
    this->Set(container, aOffset);
  }

  /**
   * Different from RangeBoundary, aPointedNode should be a child node
   * which you want to refer.  So, set non-nullptr if offset is
   * 0 - Length() - 1.  Otherwise, set nullptr, i.e., if offset is same as
   * Length().
   */
  explicit EditorDOMPointBase(nsINode* aPointedNode)
    : RangeBoundaryBase<ParentType, RefType>(
        aPointedNode && aPointedNode->IsContent() ?
          aPointedNode->GetParentNode() : nullptr,
        aPointedNode && aPointedNode->IsContent() ?
          GetRef(aPointedNode->GetParentNode(),
                 aPointedNode->AsContent()) : nullptr)
  {
  }

  EditorDOMPointBase(nsINode* aContainer,
                     nsIContent* aPointedNode,
                     int32_t aOffset)
    : RangeBoundaryBase<ParentType, RefType>(aContainer,
                                             GetRef(aContainer, aPointedNode),
                                             aOffset)
  {
  }

  template<typename PT, typename RT>
  explicit EditorDOMPointBase(const RangeBoundaryBase<PT, RT>& aOther)
    : RangeBoundaryBase<ParentType, RefType>(aOther)
  {
  }

  explicit EditorDOMPointBase(const RawRangeBoundary& aRawRangeBoundary)
    : RangeBoundaryBase<ParentType, RefType>(aRawRangeBoundary)
  {
  }

  EditorDOMPointBase<nsINode*, nsIContent*>
  AsRaw() const
  {
    return EditorDOMPointBase<nsINode*, nsIContent*>(*this);
  }

  template<typename A, typename B>
  EditorDOMPointBase& operator=(const EditorDOMPointBase<A, B>& aOther)
  {
    RangeBoundaryBase<ParentType, RefType>::operator=(aOther);
    return *this;
  }

  template<typename A, typename B>
  EditorDOMPointBase& operator=(const RangeBoundaryBase<A, B>& aOther)
  {
    RangeBoundaryBase<ParentType, RefType>::operator=(aOther);
    return *this;
  }

private:
  static nsIContent* GetRef(nsINode* aContainerNode, nsIContent* aPointedNode)
  {
    // If referring one of a child of the container, the 'ref' should be the
    // previous sibling of the referring child.
    if (aPointedNode) {
      return aPointedNode->GetPreviousSibling();
    }
    // If referring after the last child, the 'ref' should be the last child.
    if (aContainerNode && aContainerNode->IsContainerNode()) {
      return aContainerNode->GetLastChild();
    }
    return nullptr;
  }
};

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
