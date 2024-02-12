/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TreeOrderedArray_h
#define mozilla_dom_TreeOrderedArray_h

#include "nsTArray.h"

class nsINode;
template <typename T>
class RefPtr;

namespace mozilla::dom {

// A sorted tree-ordered list of pointers (either raw or RefPtr) to nodes.
template <typename NodePointer>
class TreeOrderedArray {
  template <typename T>
  struct RawTypeExtractor {};

  template <typename T>
  struct RawTypeExtractor<T*> {
    using type = T;
  };

  template <typename T>
  struct RawTypeExtractor<RefPtr<T>> {
    using type = T;
  };

  using Node = typename RawTypeExtractor<NodePointer>::type;

 public:
  operator const nsTArray<NodePointer>&() const { return mList; }
  const nsTArray<NodePointer>& AsList() const { return mList; }
  const nsTArray<NodePointer>* operator->() const { return &mList; }

  // Inserts a node into the list, and returns the new index in the array.
  //
  // All the nodes in the list should be in the same subtree, and debug builds
  // assert this.
  //
  // It's also forbidden to call Insert() with the same node multiple times, and
  // it will assert as well.
  //
  // You can provide a potential common ancestor to speed up comparisons, see
  // nsContentUtils::CompareTreePosition. That's only a hint.
  inline size_t Insert(Node&, nsINode* aCommonAncestor = nullptr);

  bool RemoveElement(Node& aNode) { return mList.RemoveElement(&aNode); }
  void RemoveElementAt(size_t aIndex) { mList.RemoveElementAt(aIndex); }

  void Clear() { mList.Clear(); }

 private:
  AutoTArray<NodePointer, 1> mList;
};

}  // namespace mozilla::dom

#endif
