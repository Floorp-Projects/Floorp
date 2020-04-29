/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RangeUtils_h
#define mozilla_RangeUtils_h

#include "Element.h"
#include "mozilla/RangeBoundary.h"
#include "nsIContent.h"
#include "nsINode.h"

namespace mozilla {

namespace dom {
class AbstractRange;
}  // namespace dom

class RangeUtils final {
  typedef dom::AbstractRange AbstractRange;

 public:
  /**
   * GetRawRangeBoundaryBefore() and GetRawRangeBoundaryAfter() retrieve
   * RawRangeBoundary which points before or after aNode.
   */
  static const RawRangeBoundary GetRawRangeBoundaryAfter(nsINode* aNode) {
    MOZ_ASSERT(aNode);

    if (NS_WARN_IF(!aNode->IsContent())) {
      return RawRangeBoundary();
    }

    nsINode* parentNode = aNode->GetParentNode();
    if (!parentNode) {
      return RawRangeBoundary();
    }
    RawRangeBoundary afterNode(parentNode, aNode->AsContent());
    // If aNode isn't in the child nodes of its parent node, we hit this case.
    // This may occur when we're called by a mutation observer while aNode is
    // removed from the parent node.
    if (NS_WARN_IF(
            !afterNode.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets))) {
      return RawRangeBoundary();
    }
    return afterNode;
  }

  static const RawRangeBoundary GetRawRangeBoundaryBefore(nsINode* aNode) {
    MOZ_ASSERT(aNode);

    if (NS_WARN_IF(!aNode->IsContent())) {
      return RawRangeBoundary();
    }

    nsINode* parentNode = aNode->GetParentNode();
    if (!parentNode) {
      return RawRangeBoundary();
    }
    // If aNode isn't in the child nodes of its parent node, we hit this case.
    // This may occur when we're called by a mutation observer while aNode is
    // removed from the parent node.
    int32_t indexInParent = parentNode->ComputeIndexOf(aNode);
    if (NS_WARN_IF(indexInParent < 0)) {
      return RawRangeBoundary();
    }
    return RawRangeBoundary(parentNode, indexInParent);
  }

  /**
   * Compute the root node of aNode for initializing range classes.
   * When aNode is in an anonymous subtree, this returns the shadow root or
   * binding parent.  Otherwise, the root node of the document or document
   * fragment.  If this returns nullptr, that means aNode can be neither the
   * start container nor end container of any range.
   */
  static nsINode* ComputeRootNode(nsINode* aNode);

  /**
   * XXX nsRange should accept 0 - UINT32_MAX as offset.  However, users of
   *     nsRange treat offset as int32_t.  Additionally, some other internal
   *     APIs like nsINode::ComputeIndexOf() use int32_t.  Therefore,
   *     nsRange should accept only 0 - INT32_MAX as valid offset for now.
   */
  static bool IsValidOffset(uint32_t aOffset) { return aOffset <= INT32_MAX; }

  /**
   * Return true if aStartContainer/aStartOffset and aEndContainer/aEndOffset
   * are valid start and end points for a range.  Otherwise, return false.
   */
  static bool IsValidPoints(nsINode* aStartContainer, uint32_t aStartOffset,
                            nsINode* aEndContainer, uint32_t aEndOffset) {
    return IsValidPoints(RawRangeBoundary(aStartContainer, aStartOffset),
                         RawRangeBoundary(aEndContainer, aEndOffset));
  }
  template <typename SPT, typename SRT, typename EPT, typename ERT>
  static bool IsValidPoints(const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
                            const RangeBoundaryBase<EPT, ERT>& aEndBoundary);

  /**
   * Utility routine to detect if a content node starts before a range and/or
   * ends after a range.  If neither it is contained inside the range.
   * Note that callers responsibility to ensure node in same doc as range.
   */
  static nsresult CompareNodeToRange(nsINode* aNode,
                                     AbstractRange* aAbstractRange,
                                     bool* aNodeIsBeforeRange,
                                     bool* aNodeIsAfterRange);
};

}  // namespace mozilla

#endif  // #ifndef mozilla_RangeUtils_h
