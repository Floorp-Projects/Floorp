/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inDeepTreeWalker_h___
#define __inDeepTreeWalker_h___

#include "inIDeepTreeWalker.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsTArray.h"

class nsINodeList;
class inIDOMUtils;

class inDeepTreeWalker final : public inIDeepTreeWalker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INIDEEPTREEWALKER

  inDeepTreeWalker();

  nsresult SetCurrentNode(nsIDOMNode* aCurrentNode,
                          nsINodeList* aSiblings);
protected:
  virtual ~inDeepTreeWalker();

  already_AddRefed<nsIDOMNode> GetParent();
  nsresult EdgeChild(nsIDOMNode** _retval, bool aReverse);

  bool mShowAnonymousContent;
  bool mShowSubDocuments;
  bool mShowDocumentsAsNodes;

  // The root node. previousNode and parentNode will return
  // null from here.
  nsCOMPtr<nsIDOMNode> mRoot;
  nsCOMPtr<nsIDOMNode> mCurrentNode;
  nsCOMPtr<inIDOMUtils> mDOMUtils;

  // We cache the siblings of mCurrentNode as a list of nodes.
  // Notes: normally siblings are all the children of the parent
  // of mCurrentNode (that are interesting for use for the walk)
  // and mCurrentIndex is the index of mCurrentNode in that list
  // But if mCurrentNode is a (sub) document then instead of
  // storing a list that has only one element (the document)
  // and setting mCurrentIndex to null, we set mSibilings to null.
  // The reason for this is purely technical, since nsINodeList is
  // nsIContent based hence we cannot use it to store a document node.
  nsCOMPtr<nsINodeList> mSiblings;

  // Index of mCurrentNode in the mSiblings list.
  int32_t mCurrentIndex;

  // Currently unused. Should be a filter for nodes.
  uint32_t mWhatToShow;
};

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID \
{ 0xbfcb82c2, 0x5611, 0x4318, { 0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 } }

#endif // __inDeepTreeWalker_h___
