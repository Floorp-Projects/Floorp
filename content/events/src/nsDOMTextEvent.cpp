/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
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

#include "nsDOMTextEvent.h"
#include "nsContentUtils.h"
#include "nsPrivateTextRange.h"

nsDOMTextEvent::nsDOMTextEvent(nsPresContext* aPresContext, nsTextEvent* aEvent)
: nsDOMUIEvent(aPresContext, aEvent ? aEvent : new nsTextEvent())
{
  NS_ASSERTION(mEvent->eventStructType == NS_TEXT_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = PR_FALSE;
    mEventIsTrusted = PR_TRUE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEventIsTrusted = PR_FALSE;
    mEvent->time = PR_Now();
  }

  //
  // extract the IME composition string
  //
  nsTextEvent *te = NS_STATIC_CAST(nsTextEvent*, aEvent);
  mText = te->theText;

  //
  // build the range list -- ranges need to be DOM-ified since the
  // IME transaction will hold a ref, the widget representation
  // isn't persistent
  //
  nsIPrivateTextRange** tempTextRangeList = new nsIPrivateTextRange*[te->rangeCount];
  if (tempTextRangeList) {
      PRUint16 i;

      for(i = 0; i < te->rangeCount; i++) {
        nsPrivateTextRange* tempPrivateTextRange = new
          nsPrivateTextRange(te->rangeArray[i].mStartOffset,
                             te->rangeArray[i].mEndOffset,
                             te->rangeArray[i].mRangeType);

        if (tempPrivateTextRange) {
          NS_ADDREF(tempPrivateTextRange);

          tempTextRangeList[i] = (nsIPrivateTextRange*)tempPrivateTextRange;
        }
      }
  }

  // We need to create mTextRange even rangeCount is 0. 
  // If rangeCount is 0, mac carbon will return 0 for new and
  // tempTextRangeList will be null. but we should still create
  // mTextRange, otherwise, we will crash it later when some code
  // call GetInputRange and AddRef to the result
  mTextRange = new nsPrivateTextRangeList(te->rangeCount ,tempTextRangeList);
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

NS_METHOD nsDOMTextEvent::GetInputRange(nsIPrivateTextRangeList** aInputRange)
{
  if (mEvent->message == NS_TEXT_TEXT)
  {
    *aInputRange = mTextRange;
    return NS_OK;
  }
  aInputRange = 0;
  return NS_ERROR_FAILURE;
}

NS_METHOD nsDOMTextEvent::GetEventReply(nsTextEventReply** aReply)
{
  if (mEvent->message == NS_TEXT_TEXT)
  {
     *aReply = &(NS_STATIC_CAST(nsTextEvent*, mEvent)->theReply);
     return NS_OK;
  }
  aReply = 0;
  return NS_ERROR_FAILURE;
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
