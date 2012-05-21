/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTextEvent.h"
#include "nsContentUtils.h"
#include "nsPrivateTextRange.h"

nsDOMTextEvent::nsDOMTextEvent(nsPresContext* aPresContext,
                               nsTextEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsTextEvent(false, 0, nsnull))
{
  NS_ASSERTION(mEvent->eventStructType == NS_TEXT_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }

  //
  // extract the IME composition string
  //
  nsTextEvent *te = static_cast<nsTextEvent*>(mEvent);
  mText = te->theText;

  //
  // build the range list -- ranges need to be DOM-ified since the
  // IME transaction will hold a ref, the widget representation
  // isn't persistent
  //
  mTextRange = new nsPrivateTextRangeList(te->rangeCount);
  if (mTextRange) {
    PRUint16 i;

    for(i = 0; i < te->rangeCount; i++) {
      nsRefPtr<nsPrivateTextRange> tempPrivateTextRange = new
        nsPrivateTextRange(te->rangeArray[i]);

      if (tempPrivateTextRange) {
        mTextRange->AppendTextRange(tempPrivateTextRange);
      }
    }
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMTextEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTextEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMTextEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateTextEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_METHOD nsDOMTextEvent::GetText(nsString& aText)
{
  aText = mText;
  return NS_OK;
}

NS_METHOD_(already_AddRefed<nsIPrivateTextRangeList>) nsDOMTextEvent::GetInputRange()
{
  if (mEvent->message == NS_TEXT_TEXT) {
    nsRefPtr<nsPrivateTextRangeList> textRange = mTextRange;
    nsPrivateTextRangeList *textRangePtr = nsnull;
    textRange.swap(textRangePtr);
    return textRangePtr;
  }
  return nsnull;
}

nsresult NS_NewDOMTextEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsTextEvent *aEvent)
{
  nsDOMTextEvent* it = new nsDOMTextEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
