/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TreeOrderedArray_h
#define mozilla_dom_TreeOrderedArray_h

#include "nsTArray.h"

namespace mozilla {
namespace dom {

// A sorted tree-ordered list of raw pointers to nodes.
template <typename Node>
class TreeOrderedArray {
 public:
  operator const nsTArray<Node*>&() const { return mList; }

  const nsTArray<Node*>* operator->() const { return &mList; }

  // Inserts a node into the list, and returns the new index in the array.
  //
  // All the nodes in the list should be in the same subtree, and debug builds
  // assert this.
  //
  // It's also forbidden to call Insert() with the same node multiple times, and
  // it will assert as well.
  inline size_t Insert(Node&);

  bool RemoveElement(Node& aNode) { return mList.RemoveElement(&aNode); }

 private:
  AutoTArray<Node*, 1> mList;
};

}  // namespace dom
}  // namespace mozilla

#endif
