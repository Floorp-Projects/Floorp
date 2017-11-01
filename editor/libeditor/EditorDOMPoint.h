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
   * Different from RangeBoundary, aReferenceChild should be a child node
   * which you want to refer.  So, set non-nullptr if offset is
   * 0 - Length() - 1.  Otherwise, set nullptr, i.e., if offset is same as
   * Length().
   */
  EditorDOMPointBase(nsINode* aContainer,
                     nsIContent* aPointedNode)
    : RangeBoundaryBase<ParentType, RefType>(aContainer,
                                             GetRef(aPointedNode))
  {
  }

  EditorDOMPointBase(nsINode* aConatiner,
                     nsIContent* aPointedNode,
                     int32_t aOffset)
    : RangeBoundaryBase<ParentType, RefType>(aConatiner,
                                             GetRef(aPointedNode),
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

private:
  static nsIContent* GetRef(nsIContent* aPointedNode)
  {
    MOZ_ASSERT(aPointedNode);
    return aPointedNode ? aPointedNode->GetPreviousSibling() : nullptr;
  }
};

} // namespace mozilla

#endif // #ifndef mozilla_EditorDOMPoint_h
