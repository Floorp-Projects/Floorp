/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of a generic filtered iterator over nodes.
 */

#ifndef mozilla_dom_FilteredNodeIterator_h
#define mozilla_dom_FilteredNodeIterator_h

#include "nsINode.h"

namespace mozilla::dom {

template <typename T, typename Iter>
class FilteredNodeIterator : public Iter {
 public:
  explicit FilteredNodeIterator(const nsINode& aNode) : Iter(aNode) {
    EnsureValid();
  }

  FilteredNodeIterator& begin() { return *this; }
  using Iter::end;

  void operator++() {
    Iter::operator++();
    EnsureValid();
  }

  using Iter::operator!=;

  T* operator*() {
    nsINode* node = Iter::operator*();
    MOZ_ASSERT(!node || T::FromNode(node));
    return static_cast<T*>(node);
  }

 private:
  void EnsureValid() {
    while (true) {
      nsINode* node = Iter::operator*();
      if (!node || T::FromNode(node)) {
        return;
      }
      Iter::operator++();
    }
  }

  FilteredNodeIterator() : Iter() {}
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_FilteredNodeIterator.h
