/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RangeUtils.h"

#include "mozilla/dom/AbstractRange.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsContentUtils.h"

namespace mozilla {

using namespace dom;

template bool RangeUtils::IsValidPoints(const RangeBoundary& aStartBoundary,
                                        const RangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RangeBoundary& aStartBoundary,
                                        const RawRangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RawRangeBoundary& aStartBoundary,
                                        const RangeBoundary& aEndBoundary);
template bool RangeUtils::IsValidPoints(const RawRangeBoundary& aStartBoundary,
                                        const RawRangeBoundary& aEndBoundary);

// static
nsINode* RangeUtils::ComputeRootNode(nsINode* aNode) {
  if (!aNode) {
    return nullptr;
  }

  if (aNode->IsContent()) {
    if (aNode->NodeInfo()->NameAtom() == nsGkAtoms::documentTypeNodeName) {
      return nullptr;
    }

    nsIContent* content = aNode->AsContent();

    // If the node is in a shadow tree then the ShadowRoot is the root.
    if (ShadowRoot* containingShadow = content->GetContainingShadow()) {
      return containingShadow;
    }

    // If the node has a binding parent, that should be the root.
    // XXXbz maybe only for native anonymous content?
    if (nsINode* root = content->GetBindingParent()) {
      return root;
    }
  }

  // Elements etc. must be in document or in document fragment,
  // text nodes in document, in document fragment or in attribute.
  if (nsINode* root = aNode->GetUncomposedDoc()) {
    return root;
  }

  NS_ASSERTION(!aNode->SubtreeRoot()->IsDocument(),
               "GetUncomposedDoc should have returned a doc");

  // We allow this because of backward compatibility.
  return aNode->SubtreeRoot();
}

// static
template <typename SPT, typename SRT, typename EPT, typename ERT>
bool RangeUtils::IsValidPoints(
    const RangeBoundaryBase<SPT, SRT>& aStartBoundary,
    const RangeBoundaryBase<EPT, ERT>& aEndBoundary) {
  // Use NS_WARN_IF() only for the cases where the arguments are unexpected.
  if (NS_WARN_IF(!aStartBoundary.IsSet()) ||
      NS_WARN_IF(!aEndBoundary.IsSet()) ||
      NS_WARN_IF(!IsValidOffset(aStartBoundary)) ||
      NS_WARN_IF(!IsValidOffset(aEndBoundary))) {
    return false;
  }

  // Otherwise, don't use NS_WARN_IF() for preventing to make console messy.
  // Instead, check one by one since it is easier to catch the error reason
  // with debugger.

  if (ComputeRootNode(aStartBoundary.Container()) !=
      ComputeRootNode(aEndBoundary.Container())) {
    return false;
  }

  bool disconnected = false;
  int32_t order = nsContentUtils::ComparePoints(aStartBoundary, aEndBoundary,
                                                &disconnected);
  if (NS_WARN_IF(disconnected)) {
    return false;
  }
  return order != 1;
}

// Utility routine to detect if a content node is completely contained in a
// range If outNodeBefore is returned true, then the node starts before the
// range does. If outNodeAfter is returned true, then the node ends after the
// range does. Note that both of the above might be true. If neither are true,
// the node is contained inside of the range.
// XXX - callers responsibility to ensure node in same doc as range!

// static
nsresult RangeUtils::CompareNodeToRange(nsINode* aNode,
                                        AbstractRange* aAbstractRange,
                                        bool* aNodeIsBeforeRange,
                                        bool* aNodeIsAfterRange) {
  MOZ_ASSERT(aNodeIsBeforeRange);
  MOZ_ASSERT(aNodeIsAfterRange);

  if (NS_WARN_IF(!aNode) || NS_WARN_IF(!aAbstractRange) ||
      NS_WARN_IF(!aAbstractRange->IsPositioned())) {
    return NS_ERROR_INVALID_ARG;
  }

  // create a pair of dom points that expresses location of node:
  //     NODE(start), NODE(end)
  // Let incoming range be:
  //    {RANGE(start), RANGE(end)}
  // if (RANGE(start) <= NODE(start))  and (RANGE(end) => NODE(end))
  // then the Node is contained (completely) by the Range.

  // gather up the dom point info
  int32_t nodeStart, nodeEnd;
  nsINode* parent = aNode->GetParentNode();
  if (!parent) {
    // can't make a parent/offset pair to represent start or
    // end of the root node, because it has no parent.
    // so instead represent it by (node,0) and (node,numChildren)
    parent = aNode;
    nodeStart = 0;
    uint32_t childCount = aNode->GetChildCount();
    MOZ_ASSERT(childCount <= INT32_MAX,
               "There shouldn't be over INT32_MAX children");
    nodeEnd = static_cast<int32_t>(childCount);
  } else {
    nodeStart = parent->ComputeIndexOf(aNode);
    nodeEnd = nodeStart + 1;
    MOZ_ASSERT(nodeStart < nodeEnd, "nodeStart shouldn't be INT32_MAX");
  }

  // XXX nsContentUtils::ComparePoints() may be expensive.  If some callers
  //     just want one of aNodeIsBeforeRange or aNodeIsAfterRange, we can
  //     skip the other comparison.

  // In the ComparePoints calls below we use a container & offset instead of
  // a range boundary because the range boundary constructor warns if you pass
  // in a -1 offset and the ComputeIndexOf call above can return -1 if aNode
  // is native anonymous content. ComparePoints has comments about offsets
  // being -1 and it seems to deal with it, or at least we aren't aware of any
  // problems arising because of it. We don't have a better idea how to get
  // rid of the warning without much larger changes so we do this just to
  // silence the warning. (Bug 1438996)

  // is RANGE(start) <= NODE(start) ?
  bool disconnected = false;
  *aNodeIsBeforeRange =
      nsContentUtils::ComparePoints(aAbstractRange->StartRef().Container(),
                                    aAbstractRange->StartRef().Offset(), parent,
                                    nodeStart, &disconnected) > 0;
  if (NS_WARN_IF(disconnected)) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }

  // is RANGE(end) >= NODE(end) ?
  *aNodeIsAfterRange =
      nsContentUtils::ComparePoints(aAbstractRange->EndRef().Container(),
                                    aAbstractRange->EndRef().Offset(), parent,
                                    nodeEnd, &disconnected) < 0;
  if (NS_WARN_IF(disconnected)) {
    return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
  }
  return NS_OK;
}

}  // namespace mozilla
