/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inDeepTreeWalker_h___
#define __inDeepTreeWalker_h___

#include "inIDeepTreeWalker.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsTArray.h"

class inIDOMUtils;

////////////////////////////////////////////////////

struct DeepTreeStackItem
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNodeList> kids;
  PRUint32 lastIndex; // Index one bigger than the index of whatever
                      // kid we're currently at in |kids|.
};

////////////////////////////////////////////////////

class inDeepTreeWalker : public inIDeepTreeWalker
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIDOMTREEWALKER
	NS_DECL_INIDEEPTREEWALKER

  inDeepTreeWalker();
  virtual ~inDeepTreeWalker();

protected:
  void PushNode(nsIDOMNode* aNode);

  bool mShowAnonymousContent;
  bool mShowSubDocuments;
  nsCOMPtr<nsIDOMNode> mRoot;
  nsCOMPtr<nsIDOMNode> mCurrentNode;
  PRUint32 mWhatToShow;
  
  nsAutoTArray<DeepTreeStackItem, 8> mStack;
  nsCOMPtr<inIDOMUtils> mDOMUtils;
};

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID \
{ 0xbfcb82c2, 0x5611, 0x4318, { 0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 } }

#endif // __inDeepTreeWalker_h___
