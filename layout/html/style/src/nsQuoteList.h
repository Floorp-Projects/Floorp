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
#ifndef nsQuoteList_h___
#define nsQuoteList_h___

#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "prclist.h"
#include "nsIDOMCharacterData.h"
#include "nsCSSPseudoElements.h"

struct nsQuoteListNode : public PRCList {
  // open-quote, close-quote, no-open-quote, or no-close-quote
  const nsStyleContentType mType;

  // Quote depth before this quote, which is always non-negative.
  PRInt32 mDepthBefore;

  // Index within the list of things specified by the 'content' property,
  // which is needed to do 'content: open-quote open-quote' correctly.
  const PRUint32 mContentIndex;

  // null for 'content:no-open-quote', 'content:no-close-quote'
  nsCOMPtr<nsIDOMCharacterData> mText;

  // The wrapper frame for all of the pseudo-element's content.  This
  // frame has useful style data and has the NS_FRAME_GENERATED_CONTENT
  // bit set (so we use it to track removal).
  nsIFrame* const mPseudoFrame;


  nsQuoteListNode(nsStyleContentType& aType, nsIFrame* aPseudoFrame,
                  PRUint32 aContentIndex)
    : mType(aType)
    , mDepthBefore(0)
    , mContentIndex(aContentIndex)
    , mPseudoFrame(aPseudoFrame)
  {
    NS_ASSERTION(aType == eStyleContentType_OpenQuote ||
                 aType == eStyleContentType_CloseQuote ||
                 aType == eStyleContentType_NoOpenQuote ||
                 aType == eStyleContentType_NoCloseQuote,
                 "incorrect type");
    NS_ASSERTION(aPseudoFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT,
                 "not generated content");
    NS_ASSERTION(aPseudoFrame->GetStyleContext()->GetPseudoType() ==
                   nsCSSPseudoElements::before ||
                 aPseudoFrame->GetStyleContext()->GetPseudoType() ==
                   nsCSSPseudoElements::after,
                 "not :before/:after generated content");
    NS_ASSERTION(aContentIndex <
                   aPseudoFrame->GetStyleContent()->ContentCount(),
                 "index out of range");
  }

  // is this 'open-quote' or 'no-open-quote'?
  PRBool IsOpenQuote() {
    return mType == eStyleContentType_OpenQuote ||
           mType == eStyleContentType_NoOpenQuote;
  }

  // is this 'close-quote' or 'no-close-quote'?
  PRBool IsCloseQuote() {
    return !IsOpenQuote();
  }

  // is this 'open-quote' or 'close-quote'?
  PRBool IsRealQuote() {
    return mType == eStyleContentType_OpenQuote ||
           mType == eStyleContentType_CloseQuote;
  }

  // is this 'no-open-quote' or 'no-close-quote'?
  PRBool IsHiddenQuote() {
    return !IsRealQuote();
  }

  // Depth of the quote for *this* node.  Either non-negative or -1.
  // -1 means this is a closing quote that tried to decrement the
  // counter below zero (which means no quote should be rendered).
  PRInt32 Depth() {
    return IsOpenQuote() ? mDepthBefore : mDepthBefore - 1;
  }

  // always non-negative
  PRInt32 DepthAfter() {
    return IsOpenQuote() ? mDepthBefore + 1
                         : (mDepthBefore == 0 ? 0 : mDepthBefore - 1);
  }

  // The text that should be displayed for this quote.
  const nsString* Text();
};

class nsQuoteList {
private:
  nsQuoteListNode* mFirstNode;
  PRUint32 mSize;
  // assign the correct |mDepthBefore| value to a node that has been inserted
  void Calc(nsQuoteListNode* aNode);
public:
  nsQuoteList() : mFirstNode(nsnull), mSize(0) {}
  ~nsQuoteList() { Clear(); }
  void Clear();
  nsQuoteListNode* Next(nsQuoteListNode* aNode) {
    return NS_STATIC_CAST(nsQuoteListNode*, PR_NEXT_LINK(aNode));
  }
  nsQuoteListNode* Prev(nsQuoteListNode* aNode) {
    return NS_STATIC_CAST(nsQuoteListNode*, PR_PREV_LINK(aNode));
  }
  void Insert(nsQuoteListNode* aNode);
  // returns whether any nodes have been destroyed
  PRBool DestroyNodesFor(nsIFrame* aFrame); //destroy all nodes with aFrame as parent
  void Remove(nsQuoteListNode* aNode) { PR_REMOVE_LINK(aNode); mSize--; }
  void RecalcAll();
  PRBool IsLast(nsQuoteListNode* aNode) { return (Next(aNode) == mFirstNode); }
#ifdef DEBUG
  void PrintChain();
#endif
};

#endif /* nsQuoteList_h___ */
