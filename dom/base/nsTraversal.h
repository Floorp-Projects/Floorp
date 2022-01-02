/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of common traversal methods for TreeWalker and NodeIterator.
 */

#ifndef nsTraversal_h___
#define nsTraversal_h___

#include "nsCOMPtr.h"
#include "mozilla/dom/CallbackObject.h"
#include "mozilla/dom/NodeFilterBinding.h"

class nsINode;

namespace mozilla {
class ErrorResult;
}

class nsTraversal {
 public:
  nsTraversal(nsINode* aRoot, uint32_t aWhatToShow,
              mozilla::dom::NodeFilter* aFilter);
  virtual ~nsTraversal();

 protected:
  nsCOMPtr<nsINode> mRoot;
  uint32_t mWhatToShow;
  RefPtr<mozilla::dom::NodeFilter> mFilter;
  bool mInAcceptNode;

  /*
   * Tests if and how a node should be filtered. Uses mWhatToShow and
   * mFilter to test the node.
   * @param aNode          Node to test
   * @param aResult        Whether we succeeded
   * @param aUnskippedNode If non-null is passed, set to aNode if node isn't
   *                       filtered out by mWhatToShow.
   * @returns              Filtervalue. See NodeFilter.webidl
   */
  int16_t TestNode(nsINode* aNode, mozilla::ErrorResult& aResult,
                   nsCOMPtr<nsINode>* aUnskippedNode = nullptr);
};

#endif
