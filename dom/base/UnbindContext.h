/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* State that is passed down to UnbindToTree. */

#ifndef mozilla_dom_UnbindContext_h__
#define mozilla_dom_UnbindContext_h__

#include "mozilla/Attributes.h"
#include "nsINode.h"

namespace mozilla::dom {

struct MOZ_STACK_CLASS UnbindContext final {
  // The root of the subtree being unbound.
  nsINode& Root() const { return mRoot; }
  // Whether we're the root of the subtree being unbound.
  bool IsUnbindRoot(const nsINode* aNode) const { return &mRoot == aNode; }
  // The parent node of the subtree we're unbinding from.
  nsINode* GetOriginalSubtreeParent() const { return mOriginalParent; }

  explicit UnbindContext(nsINode& aRoot)
      : mRoot(aRoot), mOriginalParent(aRoot.GetParentNode()) {}

 private:
  nsINode& mRoot;
  nsINode* const mOriginalParent;
};

}  // namespace mozilla::dom

#endif
