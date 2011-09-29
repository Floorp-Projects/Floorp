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

/* implementation of quotes for the CSS 'content' property */

#ifndef nsQuoteList_h___
#define nsQuoteList_h___

#include "nsGenConList.h"

struct nsQuoteNode : public nsGenConNode {
  // open-quote, close-quote, no-open-quote, or no-close-quote
  const nsStyleContentType mType;

  // Quote depth before this quote, which is always non-negative.
  PRInt32 mDepthBefore;

  nsQuoteNode(nsStyleContentType& aType, PRUint32 aContentIndex)
    : nsGenConNode(aContentIndex)
    , mType(aType)
    , mDepthBefore(0)
  {
    NS_ASSERTION(aType == eStyleContentType_OpenQuote ||
                 aType == eStyleContentType_CloseQuote ||
                 aType == eStyleContentType_NoOpenQuote ||
                 aType == eStyleContentType_NoCloseQuote,
                 "incorrect type");
    NS_ASSERTION(aContentIndex <= PR_INT32_MAX, "out of range");
  }

  virtual bool InitTextFrame(nsGenConList* aList, 
          nsIFrame* aPseudoFrame, nsIFrame* aTextFrame);

  // is this 'open-quote' or 'no-open-quote'?
  bool IsOpenQuote() {
    return mType == eStyleContentType_OpenQuote ||
           mType == eStyleContentType_NoOpenQuote;
  }

  // is this 'close-quote' or 'no-close-quote'?
  bool IsCloseQuote() {
    return !IsOpenQuote();
  }

  // is this 'open-quote' or 'close-quote'?
  bool IsRealQuote() {
    return mType == eStyleContentType_OpenQuote ||
           mType == eStyleContentType_CloseQuote;
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

class nsQuoteList : public nsGenConList {
private:
  nsQuoteNode* FirstNode() { return static_cast<nsQuoteNode*>(mFirstNode); }
public:
  // assign the correct |mDepthBefore| value to a node that has been inserted
  // Should be called immediately after calling |Insert|.
  void Calc(nsQuoteNode* aNode);

  nsQuoteNode* Next(nsQuoteNode* aNode) {
    return static_cast<nsQuoteNode*>(nsGenConList::Next(aNode));
  }
  nsQuoteNode* Prev(nsQuoteNode* aNode) {
    return static_cast<nsQuoteNode*>(nsGenConList::Prev(aNode));
  }
  
  void RecalcAll();
#ifdef DEBUG
  void PrintChain();
#endif
};

#endif /* nsQuoteList_h___ */
