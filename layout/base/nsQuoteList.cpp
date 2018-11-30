/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of quotes for the CSS 'content' property */

#include "nsQuoteList.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Text.h"

using namespace mozilla;

bool nsQuoteNode::InitTextFrame(nsGenConList* aList, nsIFrame* aPseudoFrame,
                                nsIFrame* aTextFrame) {
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
    aTextFrame->GetContent()->AsText()->SetText(Text(), false);
  }
  return dirty;
}

nsString nsQuoteNode::Text() {
  NS_ASSERTION(mType == StyleContentType::OpenQuote ||
                   mType == StyleContentType::CloseQuote,
               "should only be called when mText should be non-null");

  nsString result;
  Servo_Quotes_GetQuote(mPseudoFrame->StyleList()->mQuotes.get(), Depth(),
                        mType, &result);
  return result;
}

void nsQuoteList::Calc(nsQuoteNode* aNode) {
  if (aNode == FirstNode()) {
    aNode->mDepthBefore = 0;
  } else {
    aNode->mDepthBefore = Prev(aNode)->DepthAfter();
  }
}

void nsQuoteList::RecalcAll() {
  for (nsQuoteNode* node = FirstNode(); node; node = Next(node)) {
    int32_t oldDepth = node->mDepthBefore;
    Calc(node);

    if (node->mDepthBefore != oldDepth && node->mText && node->IsRealQuote())
      node->mText->SetData(node->Text(), IgnoreErrors());
  }
}

#ifdef DEBUG
void nsQuoteList::PrintChain() {
  printf("Chain: \n");
  for (nsQuoteNode* node = FirstNode(); node; node = Next(node)) {
    printf("  %p %d - ", static_cast<void*>(node), node->mDepthBefore);
    switch (node->mType) {
      case StyleContentType::OpenQuote:
        printf("open");
        break;
      case StyleContentType::NoOpenQuote:
        printf("noOpen");
        break;
      case StyleContentType::CloseQuote:
        printf("close");
        break;
      case StyleContentType::NoCloseQuote:
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
  }
}
#endif
