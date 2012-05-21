/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccEvent.h"
#include "nsAccUtils.h"
#include "nsDocAccessible.h"

////////////////////////////////////////////////////////////////////////////////
// nsAccEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsAccEvent, nsIAccessibleEvent)

NS_IMETHODIMP
nsAccEvent::GetIsFromUserInput(bool* aIsFromUserInput)
{
  *aIsFromUserInput = mEvent->IsFromUserInput();
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetEventType(PRUint32* aEventType)
{
  *aEventType = mEvent->GetEventType();
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetAccessible(nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  NS_IF_ADDREF(*aAccessible = mEvent->GetAccessible());
  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetDOMNode(nsIDOMNode** aDOMNode)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nsnull;

  nsINode* node = mEvent->GetNode();
  if (node)
    CallQueryInterface(node, aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
nsAccEvent::GetAccessibleDocument(nsIAccessibleDocument** aDocAccessible)
{
  NS_ENSURE_ARG_POINTER(aDocAccessible);

  NS_IF_ADDREF(*aDocAccessible = mEvent->GetDocAccessible());
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccStateChangeEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccStateChangeEvent, nsAccEvent,
                             nsIAccessibleStateChangeEvent)

NS_IMETHODIMP
nsAccStateChangeEvent::GetState(PRUint32* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  PRUint32 state1 = 0, state2 = 0;
  PRUint64 state = static_cast<AccStateChangeEvent*>(mEvent.get())->GetState();
  nsAccUtils::To32States(state, &state1, &state2);

  *aState = state1 | state2; // only one state is not 0
  return NS_OK;
}

NS_IMETHODIMP
nsAccStateChangeEvent::IsExtraState(bool* aIsExtraState)
{
  NS_ENSURE_ARG_POINTER(aIsExtraState);

  PRUint32 state1 = 0, state2 = 0;
  PRUint64 state = static_cast<AccStateChangeEvent*>(mEvent.get())->GetState();
  nsAccUtils::To32States(state, &state1, &state2);

  *aIsExtraState = (state2 != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsAccStateChangeEvent::IsEnabled(bool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  *aIsEnabled = static_cast<AccStateChangeEvent*>(mEvent.get())->IsStateEnabled();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccTextChangeEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccTextChangeEvent, nsAccEvent,
                             nsIAccessibleTextChangeEvent)

NS_IMETHODIMP
nsAccTextChangeEvent::GetStart(PRInt32* aStart)
{
  NS_ENSURE_ARG_POINTER(aStart);
  *aStart = static_cast<AccTextChangeEvent*>(mEvent.get())->GetStartOffset();
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);
  *aLength = static_cast<AccTextChangeEvent*>(mEvent.get())->GetLength();
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::IsInserted(bool* aIsInserted)
{
  NS_ENSURE_ARG_POINTER(aIsInserted);
  *aIsInserted = static_cast<AccTextChangeEvent*>(mEvent.get())->IsTextInserted();
  return NS_OK;
}

NS_IMETHODIMP
nsAccTextChangeEvent::GetModifiedText(nsAString& aModifiedText)
{
  static_cast<AccTextChangeEvent*>(mEvent.get())->GetModifiedText(aModifiedText);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccHideEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccHideEvent, nsAccEvent,
                             nsIAccessibleHideEvent)

NS_IMETHODIMP
nsAccHideEvent::GetTargetParent(nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);

  NS_IF_ADDREF(*aAccessible =
    static_cast<AccHideEvent*>(mEvent.get())->TargetParent());
  return NS_OK;
}

NS_IMETHODIMP
nsAccHideEvent::GetTargetNextSibling(nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);

  NS_IF_ADDREF(*aAccessible =
    static_cast<AccHideEvent*>(mEvent.get())->TargetNextSibling());
  return NS_OK;
}

NS_IMETHODIMP
nsAccHideEvent::GetTargetPrevSibling(nsIAccessible** aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);

  NS_IF_ADDREF(*aAccessible =
    static_cast<AccHideEvent*>(mEvent.get())->TargetPrevSibling());
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsAccCaretMoveEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccCaretMoveEvent, nsAccEvent,
                             nsIAccessibleCaretMoveEvent)

NS_IMETHODIMP
nsAccCaretMoveEvent::GetCaretOffset(PRInt32 *aCaretOffset)
{
  NS_ENSURE_ARG_POINTER(aCaretOffset);

  *aCaretOffset = static_cast<AccCaretMoveEvent*>(mEvent.get())->GetCaretOffset();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccTableChangeEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccTableChangeEvent, nsAccEvent,
                             nsIAccessibleTableChangeEvent)

NS_IMETHODIMP
nsAccTableChangeEvent::GetRowOrColIndex(PRInt32 *aRowOrColIndex)
{
  NS_ENSURE_ARG_POINTER(aRowOrColIndex);

  *aRowOrColIndex =
    static_cast<AccTableChangeEvent*>(mEvent.get())->GetIndex();
  return NS_OK;
}

NS_IMETHODIMP
nsAccTableChangeEvent::GetNumRowsOrCols(PRInt32* aNumRowsOrCols)
{
  NS_ENSURE_ARG_POINTER(aNumRowsOrCols);

  *aNumRowsOrCols = static_cast<AccTableChangeEvent*>(mEvent.get())->GetCount();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccVirtualCursorChangeEvent
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED1(nsAccVirtualCursorChangeEvent, nsAccEvent,
                             nsIAccessibleVirtualCursorChangeEvent)

NS_IMETHODIMP
nsAccVirtualCursorChangeEvent::GetOldAccessible(nsIAccessible** aOldAccessible)
{
  NS_ENSURE_ARG_POINTER(aOldAccessible);

  *aOldAccessible =
    static_cast<AccVCChangeEvent*>(mEvent.get())->OldAccessible();
  NS_IF_ADDREF(*aOldAccessible);
  return NS_OK;
}

NS_IMETHODIMP
nsAccVirtualCursorChangeEvent::GetOldStartOffset(PRInt32* aOldStartOffset)
{
  NS_ENSURE_ARG_POINTER(aOldStartOffset);

  *aOldStartOffset =
    static_cast<AccVCChangeEvent*>(mEvent.get())->OldStartOffset();
  return NS_OK;
}

NS_IMETHODIMP
nsAccVirtualCursorChangeEvent::GetOldEndOffset(PRInt32* aOldEndOffset)
{
  NS_ENSURE_ARG_POINTER(aOldEndOffset);

  *aOldEndOffset =
    static_cast<AccVCChangeEvent*>(mEvent.get())->OldEndOffset();
  return NS_OK;
}
