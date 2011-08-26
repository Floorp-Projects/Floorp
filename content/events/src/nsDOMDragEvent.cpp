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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@gmail.com>
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

#include "nsDOMDragEvent.h"
#include "nsIServiceManager.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsDOMDataTransfer.h"
#include "nsIDragService.h"

nsDOMDragEvent::nsDOMDragEvent(nsPresContext* aPresContext,
                               nsInputEvent* aEvent)
  : nsDOMMouseEvent(aPresContext, aEvent ? aEvent :
                    new nsDragEvent(PR_FALSE, 0, nsnull))
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<nsMouseEvent*>(mEvent)->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

nsDOMDragEvent::~nsDOMDragEvent()
{
  if (mEventIsInternal) {
    if (mEvent->eventStructType == NS_DRAG_EVENT)
      delete static_cast<nsDragEvent*>(mEvent);
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMDragEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDragEvent, nsDOMMouseEvent)

DOMCI_DATA(DragEvent, nsDOMDragEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMDragEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DragEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
nsDOMDragEvent::InitDragEvent(const nsAString & aType,
                              PRBool aCanBubble, PRBool aCancelable,
                              nsIDOMWindow* aView, PRInt32 aDetail,
                              PRInt32 aScreenX, PRInt32 aScreenY,
                              PRInt32 aClientX, PRInt32 aClientY, 
                              PRBool aCtrlKey, PRBool aAltKey, PRBool aShiftKey,
                              PRBool aMetaKey, PRUint16 aButton,
                              nsIDOMEventTarget *aRelatedTarget,
                              nsIDOMDataTransfer* aDataTransfer)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                  aView, aDetail, aScreenX, aScreenY, aClientX, aClientY,
                  aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                  aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEventIsInternal && mEvent) {
    nsDragEvent* dragEvent = static_cast<nsDragEvent*>(mEvent);
    dragEvent->dataTransfer = aDataTransfer;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDragEvent::GetDataTransfer(nsIDOMDataTransfer** aDataTransfer)
{
  // the dataTransfer field of the event caches the DataTransfer associated
  // with the drag. It is initialized when an attempt is made to retrieve it
  // rather that when the event is created to avoid duplicating the data when
  // no listener ever uses it.
  *aDataTransfer = nsnull;

  if (!mEvent || mEvent->eventStructType != NS_DRAG_EVENT) {
    NS_WARNING("Tried to get dataTransfer from non-drag event!");
    return NS_OK;
  }

  nsDragEvent* dragEvent = static_cast<nsDragEvent*>(mEvent);
  // for synthetic events, just use the supplied data transfer object even if null
  if (!mEventIsInternal) {
    nsresult rv = nsContentUtils::SetDataTransferInEvent(dragEvent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aDataTransfer = dragEvent->dataTransfer);
  return NS_OK;
}

nsresult NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsDragEvent *aEvent) 
{
  nsDOMDragEvent* event = new nsDOMDragEvent(aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
