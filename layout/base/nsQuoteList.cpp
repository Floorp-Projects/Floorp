/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of quotes for the CSS 'content' property */

#include "nsQuoteList.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIFrameInlines.h"
#include "nsContainerFrame.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Text.h"
#include "mozilla/intl/Quotes.h"

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
  int32_t depth = Depth();
  MOZ_ASSERT(depth >= -1);

  if (depth < 0) {
    return result;
  }

  const auto& quotesProp = mPseudoFrame->StyleList()->mQuotes;

  if (quotesProp.IsAuto()) {
    // Look up CLDR-derived quotation marks for the language of the context.
    const nsIFrame* frame = mPseudoFrame->GetInFlowParent();
    // Parent of the pseudo is the element around which the quotes are applied;
    // we want lang from *its* parent, unless it is the root.
    // XXX Are there other cases where we shouldn't look up to the parent?
    if (!frame->Style()->IsRootElementStyle()) {
      if (const nsIFrame* parent = frame->GetInFlowParent()) {
        frame = parent;
      }
    }
    const intl::Quotes* quotes =
        intl::QuotesForLang(frame->StyleFont()->mLanguage);
    // If we don't have quote-mark data for the language, use built-in
    // defaults.
    if (!quotes) {
      static const intl::Quotes sDefaultQuotes = {
          {0x201c, 0x201d, 0x2018, 0x2019}};
      quotes = &sDefaultQuotes;
    }
    size_t index = (depth == 0 ? 0 : 2);  // select first or second pair
    index += (mType == StyleContentType::OpenQuote ? 0 : 1);  // open or close
    result.Append(quotes->mChars[index]);
    return result;
  }

  MOZ_ASSERT(quotesProp.IsQuoteList());
  const Span<const StyleQuotePair> quotes = quotesProp.AsQuoteList().AsSpan();

  // Reuse the last pair when the depth is greater than the number of
  // pairs of quotes.  (Also make 'quotes: none' and close-quote from
  // a depth of 0 equivalent for the next test.)
  if (depth >= static_cast<int32_t>(quotes.Length())) {
    depth = static_cast<int32_t>(quotes.Length()) - 1;
  }

  if (depth == -1) {
    // close-quote from a depth of 0 or 'quotes: none'
    return result;
  }

  const StyleQuotePair& pair = quotes[depth];
  const StyleOwnedStr& quote =
      mType == StyleContentType::OpenQuote ? pair.opening : pair.closing;
  result.Assign(NS_ConvertUTF8toUTF16(quote.AsString()));
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
  using StyleContentType = nsQuoteNode::StyleContentType;

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
