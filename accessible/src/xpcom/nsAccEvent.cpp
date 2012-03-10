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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
