/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of quotes for the CSS 'content' property */

#ifndef nsQuoteList_h___
#define nsQuoteList_h___

#include "mozilla/Attributes.h"
#include "nsGenConList.h"

struct nsQuoteNode : public nsGenConNode {
  // open-quote, close-quote, no-open-quote, or no-close-quote
  const StyleContentType mType;

  // Quote depth before this quote, which is always non-negative.
  int32_t mDepthBefore;

  nsQuoteNode(StyleContentType aType, uint32_t aContentIndex)
      : nsGenConNode(aContentIndex), mType(aType), mDepthBefore(0) {
    NS_ASSERTION(aType == StyleContentType::OpenQuote ||
                     aType == StyleContentType::CloseQuote ||
                     aType == StyleContentType::NoOpenQuote ||
                     aType == StyleContentType::NoCloseQuote,
                 "incorrect type");
    NS_ASSERTION(aContentIndex <= INT32_MAX, "out of range");
  }

  virtual bool InitTextFrame(nsGenConList* aList, nsIFrame* aPseudoFrame,
                             nsIFrame* aTextFrame) override;

  // is this 'open-quote' or 'no-open-quote'?
  bool IsOpenQuote() {
    return mType == StyleContentType::OpenQuote ||
           mType == StyleContentType::NoOpenQuote;
  }

  // is this 'close-quote' or 'no-close-quote'?
  bool IsCloseQuote() { return !IsOpenQuote(); }

  // is this 'open-quote' or 'close-quote'?
  bool IsRealQuote() {
    return mType == StyleContentType::OpenQuote ||
           mType == StyleContentType::CloseQuote;
  }

  // Depth of the quote for *this* node.  Either non-negative or -1.
  // -1 means this is a closing quote that tried to decrement the
  // counter below zero (which means no quote should be rendered).
  int32_t Depth() { return IsOpenQuote() ? mDepthBefore : mDepthBefore - 1; }

  // always non-negative
  int32_t DepthAfter() {
    return IsOpenQuote() ? mDepthBefore + 1
                         : (mDepthBefore == 0 ? 0 : mDepthBefore - 1);
  }

  // The text that should be displayed for this quote.
  nsString Text();
};

class nsQuoteList : public nsGenConList {
 private:
  nsQuoteNode* FirstNode() {
    return static_cast<nsQuoteNode*>(mList.getFirst());
  }

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
