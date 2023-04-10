/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inDeepTreeWalker_h___
#define __inDeepTreeWalker_h___

#include "inIDeepTreeWalker.h"

#include "nsCOMPtr.h"
#include "nsINode.h"
#include "nsTArray.h"

class nsINodeList;

class inDeepTreeWalker final : public inIDeepTreeWalker {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIDEEPTREEWALKER

  inDeepTreeWalker();

  using ChildList = AutoTArray<RefPtr<nsINode>, 8>;
  void GetChildren(nsINode& aParent, ChildList&);

 protected:
  virtual ~inDeepTreeWalker();

  already_AddRefed<nsINode> GetParent();
  nsresult EdgeChild(nsINode** _retval, bool aReverse);

  bool mShowAnonymousContent = false;
  bool mShowSubDocuments = false;
  bool mShowDocumentsAsNodes = false;

  // The root node. previousNode and parentNode will return
  // null from here.
  nsCOMPtr<nsINode> mRoot;
  nsCOMPtr<nsINode> mCurrentNode;

  // We cache the siblings of mCurrentNode as a list of nodes.
  // Notes: normally siblings are all the children of the parent
  // of mCurrentNode (that are interesting for use for the walk)
  // and mCurrentIndex is the index of mCurrentNode in that list
  ChildList mSiblings;

  // Index of mCurrentNode in the mSiblings list.
  int32_t mCurrentIndex = -1;
};

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID                        \
  {                                                  \
    0xbfcb82c2, 0x5611, 0x4318, {                    \
      0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 \
    }                                                \
  }

#endif  // __inDeepTreeWalker_h___
