/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
    dirty = PR_TRUE;

  // Don't set up text for 'no-open-quote' and 'no-close-quote'.
  if (IsRealQuote()) {
    aTextFrame->GetContent()->SetText(*Text(), PR_FALSE);
  }
  return dirty;
}

const nsString*
nsQuoteNode::Text()
{
  NS_ASSERTION(mType == eStyleContentType_OpenQuote ||
               mType == eStyleContentType_CloseQuote,
               "should only be called when mText should be non-null");
  const nsStyleQuotes* styleQuotes = mPseudoFrame->GetStyleQuotes();
  PRInt32 quotesCount = styleQuotes->QuotesCount(); // 0 if 'quotes:none'
  PRInt32 quoteDepth = Depth();

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
    PRInt32 oldDepth = node->mDepthBefore;
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
