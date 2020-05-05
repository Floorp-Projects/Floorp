/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of
 * https://dom.spec.whatwg.org/#concept-shadow-including-tree-order in iterator
 * form.  This can and should be used to avoid recursion on the stack and lots
 * of function calls during shadow-including tree iteration.
 */

#ifndef mozilla_dom_ShadowIncludingTreeIterator_h
#define mozilla_dom_ShadowIncludingTreeIterator_h

#include "nsINode.h"
#include "nsTArray.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"

namespace mozilla {
namespace dom {

class ShadowIncludingTreeIterator {
 public:
  /**
   * Initialize an iterator with aRoot.  After that it can be iterated with a
   * range-based for loop.  At the moment, that's the only supported form of use
   * for this iterator.
   */
  explicit ShadowIncludingTreeIterator(nsINode& aRoot) : mCurrent(&aRoot) {
    mRoots.AppendElement(&aRoot);
  }

#ifdef DEBUG
  ~ShadowIncludingTreeIterator() {
    MOZ_ASSERT(
        !mMutationGuard.Mutated(0),
        "Don't mutate the DOM while using a ShadowIncludingTreeIterator");
  }
#endif  // DEBUG

  // Basic support for range-based for loops.  This will modify the iterator as
  // it goes.
  ShadowIncludingTreeIterator& begin() { return *this; }

  std::nullptr_t end() const { return nullptr; }

  bool operator!=(std::nullptr_t) const { return !!mCurrent; }

  void operator++() { Next(); }

  nsINode* operator*() { return mCurrent; }

 private:
  void Next() {
    MOZ_ASSERT(mCurrent, "Don't call Next() after we have no current node");

    // We walk shadow roots immediately after their shadow host.
    if (Element* element = Element::FromNode(mCurrent)) {
      if (ShadowRoot* shadowRoot = element->GetShadowRoot()) {
        mCurrent = shadowRoot;
        mRoots.AppendElement(shadowRoot);
        return;
      }
    }

    mCurrent = mCurrent->GetNextNode(mRoots.LastElement());
    while (!mCurrent) {
      // Nothing left under this root.  Keep trying to pop the stack until we
      // find a node or run out of stack.
      nsINode* root = mRoots.PopLastElement();
      if (mRoots.IsEmpty()) {
        // No more roots to step out of; we're done.  mCurrent is already set to
        // null.
        return;
      }
      mCurrent =
          ShadowRoot::FromNode(root)->Host()->GetNextNode(mRoots.LastElement());
    }
  }

  // The current node we're at.
  nsINode* mCurrent;

  // Stack of roots that we're inside of right now.  An empty stack can only
  // happen when mCurrent is null (and hence we are done iterating).
  //
  // The default array size here is picked based on gut feeling.  We want at
  // least 1, since we will always add something to it in our constructor.
  // Having a few more entries probably makes sense, because this is commonly
  // used in cases when we know we have custom elements, and hence likely have
  // shadow DOMs.  But the exact value "4" was just picked because it sounded
  // not too big, not too small.  Feel free to replace it with something else
  // based on actual data.
  CopyableAutoTArray<nsINode*, 4> mRoots;

#ifdef DEBUG
  // Make sure no one mutates the DOM while we're walking over it.
  nsMutationGuard mMutationGuard;
#endif  // DEBUG
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ShadowIncludingTreeIterator_h
