/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TreeOrderedArrayInlines_h
#define mozilla_dom_TreeOrderedArrayInlines_h

#include "mozilla/dom/TreeOrderedArray.h"
#include "mozilla/BinarySearch.h"
#include "nsContentUtils.h"
#include <type_traits>

namespace mozilla {
namespace dom {

template <typename Node>
size_t TreeOrderedArray<Node>::Insert(Node& aNode) {
  static_assert(std::is_base_of<nsINode, Node>::value, "Should be a node");

#ifdef DEBUG
  for (Node* n : mList) {
    MOZ_ASSERT(n->SubtreeRoot() == aNode.SubtreeRoot(),
               "Should only insert nodes on the same subtree");
  }
#endif

  if (mList.IsEmpty()) {
    mList.AppendElement(&aNode);
    return 0;
  }

  struct PositionComparator {
    Node& mNode;
    explicit PositionComparator(Node& aNode) : mNode(aNode) {}

    int operator()(void* aNode) const {
      auto* curNode = static_cast<Node*>(aNode);
      MOZ_DIAGNOSTIC_ASSERT(curNode != &mNode,
                            "Tried to insert a node already in the list");
      if (nsContentUtils::PositionIsBefore(&mNode, curNode)) {
        return -1;
      }
      return 1;
    }
  };

  size_t idx;
  BinarySearchIf(mList, 0, mList.Length(), PositionComparator(aNode), &idx);
  mList.InsertElementAt(idx, &aNode);
  return idx;
}

}  // namespace dom
}  // namespace mozilla
#endif
