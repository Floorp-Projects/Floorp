/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Esben Mose Hansen.
 *
 * Contributor(s):
 *   Esben Mose Hansen <esben@oek.dk> (original author)
 *   L. David Baron <dbaron@dbaron.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    return PR_FALSE;
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
