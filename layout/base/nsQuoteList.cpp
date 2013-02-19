/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of quotes for the CSS 'content' property */

#include "nsQuoteList.h"
#include "nsReadableUtils.h"

bool
nsQuoteNode::InitTextFrame(nsGenConList* aList, nsIFrame* aPseudoFrame,
                           nsIFrame* aTextFrame)
{
  nsGenConNode::InitTextFrame(aList, aPseudoFrame, aTextFrame);

  nsQuoteList* quoteList = static_cast<nsQuoteList*>(aList);
  bool dirty = false;
  quoteList->Insert(this);
  if (quoteList->IsLast(this))
    quoteList->Calc(this);
  else
    dirty = true;

  // Don't set up text for 'no-open-quote' and 'no-close-quote'.
  if (IsRealQuote()) {
    aTextFrame->GetContent()->SetText(*Text(), false);
  }
  return dirty;
}

const nsString*
nsQuoteNode::Text()
{
  NS_ASSERTION(mType == eStyleContentType_OpenQuote ||
               mType == eStyleContentType_CloseQuote,
               "should only be called when mText should be non-null");
  const nsStyleQuotes* styleQuotes = mPseudoFrame->StyleQuotes();
  int32_t quotesCount = styleQuotes->QuotesCount(); // 0 if 'quotes:none'
  int32_t quoteDepth = Depth();

  // Reuse the last pair when the depth is greater than the number of
  // pairs of quotes.  (Also make 'quotes: none' and close-quote from
  // a depth of 0 equivalent for the next test.)
  if (quoteDepth >= quotesCount)
    quoteDepth = quotesCount - 1;

  const nsString *result;
  if (quoteDepth == -1) {
    // close-quote from a depth of 0 or 'quotes: none' (we want a node
    // with the empty string so dynamic changes are easier to handle)
    result = & EmptyString();
  } else {
    result = eStyleContentType_OpenQuote == mType
               ? styleQuotes->OpenQuoteAt(quoteDepth)
               : styleQuotes->CloseQuoteAt(quoteDepth);
  }
  return result;
}

void
nsQuoteList::Calc(nsQuoteNode* aNode)
{
  if (aNode == FirstNode()) {
    aNode->mDepthBefore = 0;
  } else {
    aNode->mDepthBefore = Prev(aNode)->DepthAfter();
  }
}

void
nsQuoteList::RecalcAll()
{
  nsQuoteNode *node = FirstNode();
  if (!node)
    return;

  do {
    int32_t oldDepth = node->mDepthBefore;
    Calc(node);

    if (node->mDepthBefore != oldDepth && node->mText && node->IsRealQuote())
      node->mText->SetData(*node->Text());

    // Next node
    node = Next(node);
  } while (node != FirstNode());
}

#ifdef DEBUG
void
nsQuoteList::PrintChain()
{
  printf("Chain: \n");
  if (!FirstNode()) {
    return;
  }
  nsQuoteNode* node = FirstNode();
  do {
    printf("  %p %d - ", static_cast<void*>(node), node->mDepthBefore);
    switch(node->mType) {
        case (eStyleContentType_OpenQuote):
          printf("open");
          break;
        case (eStyleContentType_NoOpenQuote):
          printf("noOpen");
          break;
        case (eStyleContentType_CloseQuote):
          printf("close");
          break;
        case (eStyleContentType_NoCloseQuote):
          printf("noClose");
          break;
        default:
          printf("unknown!!!");
    }
    printf(" %d - %d,", node->Depth(), node->DepthAfter());
    if (node->mText) {
      nsAutoString data;
      node->mText->GetData(data);
      printf(" \"%s\",", NS_ConvertUTF16toUTF8(data).get());
    }
    printf("\n");
    node = Next(node);
  } while (node != FirstNode());
}
#endif
