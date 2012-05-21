/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for nsCounterList and nsQuoteList */

#ifndef nsGenConList_h___
#define nsGenConList_h___

#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "prclist.h"
#include "nsIDOMCharacterData.h"
#include "nsCSSPseudoElements.h"

class nsGenConList;

struct nsGenConNode : public PRCList {
  // The wrapper frame for all of the pseudo-element's content.  This
  // frame generally has useful style data and has the
  // NS_FRAME_GENERATED_CONTENT bit set (so we use it to track removal),
  // but does not necessarily for |nsCounterChangeNode|s.
  nsIFrame* mPseudoFrame;

  // Index within the list of things specified by the 'content' property,
  // which is needed to do 'content: open-quote open-quote' correctly,
  // and needed for similar cases for counters.
  const PRInt32 mContentIndex;

  // null for 'content:no-open-quote', 'content:no-close-quote' and for
  // counter nodes for increments and resets (rather than uses)
  nsCOMPtr<nsIDOMCharacterData> mText;

  nsGenConNode(PRInt32 aContentIndex)
    : mPseudoFrame(nsnull)
    , mContentIndex(aContentIndex)
  {
  }

  /**
   * Finish initializing the generated content node once we know the
   * relevant text frame. This must be called just after
   * the textframe has been initialized. This need not be called at all
   * for nodes that don't generate text. This will generally set the
   * mPseudoFrame, insert the node into aList, and set aTextFrame up
   * with the correct text.
   * @param aList the list the node belongs to
   * @param aPseudoFrame the :before or :after frame
   * @param aTextFrame the textframe where the node contents will render
   * @return true iff this marked the list dirty
   */
  virtual bool InitTextFrame(nsGenConList* aList, nsIFrame* aPseudoFrame,
                               nsIFrame* aTextFrame)
  {
    mPseudoFrame = aPseudoFrame;
    CheckFrameAssertions();
    return false;
  }

  virtual ~nsGenConNode() {} // XXX Avoid, perhaps?

protected:
  void CheckFrameAssertions() {
    NS_ASSERTION(mContentIndex <
                   PRInt32(mPseudoFrame->GetStyleContent()->ContentCount()),
                 "index out of range");
      // We allow negative values of mContentIndex for 'counter-reset' and
      // 'counter-increment'.

    NS_ASSERTION(mContentIndex < 0 ||
                 mPseudoFrame->GetStyleContext()->GetPseudo() ==
                   nsCSSPseudoElements::before ||
                 mPseudoFrame->GetStyleContext()->GetPseudo() ==
                   nsCSSPseudoElements::after,
                 "not :before/:after generated content and not counter change");
    NS_ASSERTION(mContentIndex < 0 ||
                 mPseudoFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT,
                 "not generated content and not counter change");
  }
};

class nsGenConList {
protected:
  nsGenConNode* mFirstNode;
  PRUint32 mSize;
public:
  nsGenConList() : mFirstNode(nsnull), mSize(0) {}
  ~nsGenConList() { Clear(); }
  void Clear();
  static nsGenConNode* Next(nsGenConNode* aNode) {
    return static_cast<nsGenConNode*>(PR_NEXT_LINK(aNode));
  }
  static nsGenConNode* Prev(nsGenConNode* aNode) {
    return static_cast<nsGenConNode*>(PR_PREV_LINK(aNode));
  }
  void Insert(nsGenConNode* aNode);
  // returns whether any nodes have been destroyed
  bool DestroyNodesFor(nsIFrame* aFrame); //destroy all nodes with aFrame as parent

  // Return true if |aNode1| is after |aNode2|.
  static bool NodeAfter(const nsGenConNode* aNode1,
                          const nsGenConNode* aNode2);

  void Remove(nsGenConNode* aNode) { PR_REMOVE_LINK(aNode); mSize--; }
  bool IsLast(nsGenConNode* aNode) { return (Next(aNode) == mFirstNode); }
};

#endif /* nsGenConList_h___ */
