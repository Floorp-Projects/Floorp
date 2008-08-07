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

#include "nsIContent.h"
#include "nsStyleStruct.h"
#include "nsStyleContext.h"
#include "prclist.h"
#include "nsIDOMCharacterData.h"
#include "nsCSSPseudoElements.h"

struct nsGenConNode : public PRCList {
  // The content associated with this node. This is never a generated
  // content element. When mPseudoType is non-null, this is the element
  // for which we generated the anonymous content. If mPseudoType is null,
  // this is the element associated with a counter reset or increment.  
  nsIContent* mParentContent;
  // nsGkAtoms::before, nsGkAtoms::after, or null
  nsIAtom*    mPseudoType;

  // Index within the list of things specified by the 'content' property,
  // which is needed to do 'content: open-quote open-quote' correctly,
  // and needed for similar cases for counters.
  const PRInt32 mContentIndex;

  // null for 'content:no-open-quote', 'content:no-close-quote' and for
  // counter nodes for increments and resets (rather than uses)
  nsCOMPtr<nsIDOMCharacterData> mText;

  static nsIAtom* ToGeneratedContentType(nsIAtom* aPseudoType)
  {
    if (aPseudoType == nsCSSPseudoElements::before ||
        aPseudoType == nsCSSPseudoElements::after)
      return aPseudoType;
    return nsnull;
  }
  
  nsGenConNode(nsIContent* aParentContent, nsStyleContext* aStyleContext,
               PRInt32 aContentIndex)
    : mParentContent(aParentContent)
    , mPseudoType(ToGeneratedContentType(aStyleContext->GetPseudoType()))
    , mContentIndex(aContentIndex)
  {
    NS_ASSERTION(aContentIndex <
                 PRInt32(aStyleContext->GetStyleContent()->ContentCount()),
                 "index out of range");
    // We allow negative values of mContentIndex for 'counter-reset' and
    // 'counter-increment'.
    NS_ASSERTION(aContentIndex < 0 || mPseudoType,
                 "not :before/:after generated content and not counter change");
  }

  virtual ~nsGenConNode() {} // XXX Avoid, perhaps?
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
  /**
   * Destroy all nodes whose aContent/aPseudo match.
   * @return true if some nodes were destroyed
   */
  PRBool DestroyNodesFor(nsIContent* aParentContent, nsIAtom* aPseudo);

  // Return true if |aNode1| is after |aNode2|.
  static PRBool NodeAfter(const nsGenConNode* aNode1,
                          const nsGenConNode* aNode2);

  void Remove(nsGenConNode* aNode) { PR_REMOVE_LINK(aNode); mSize--; }
  PRBool IsLast(nsGenConNode* aNode) { return (Next(aNode) == mFirstNode); }
};

#endif /* nsGenConList_h___ */
