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
#include "nsIEventStateManager.h"
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

NS_INTERFACE_MAP_BEGIN(nsDOMDragEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(DragEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
nsDOMDragEvent::InitDragEvent(const nsAString & aType,
                              PRBool aCanBubble, PRBool aCancelable,
                              nsIDOMAbstractView* aView, PRInt32 aDetail,
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
nsDOMDragEvent::InitDragEventNS(const nsAString & aNamespaceURIArg,
                                const nsAString & aType,
                                PRBool aCanBubble, PRBool aCancelable,
                                nsIDOMAbstractView* aView, PRInt32 aDetail,
                                PRInt32 aScreenX, PRInt32 aScreenY,
                                PRInt32 aClientX, PRInt32 aClientY, 
                                PRBool aCtrlKey, PRBool aAltKey, PRBool aShiftKey,
                                PRBool aMetaKey, PRUint16 aButton,
                                nsIDOMEventTarget *aRelatedTarget,
                                nsIDOMDataTransfer* aDataTransfer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMDragEvent::GetDataTransfer(nsIDOMDataTransfer** aDataTransfer)
{
  *aDataTransfer = nsnull;

  if (!mEvent || mEvent->eventStructType != NS_DRAG_EVENT) {
    NS_WARNING("Tried to get dataTransfer from non-drag event!");
    return NS_OK;
  }

  // the dataTransfer field of the event caches the DataTransfer associated
  // with the drag. It is initialized when an attempt is made to retrieve it
  // rather that when the event is created to avoid duplicating the data when
  // no listener ever uses it.
  nsDragEvent* dragEvent = static_cast<nsDragEvent*>(mEvent);
  if (dragEvent->dataTransfer) {
    CallQueryInterface(dragEvent->dataTransfer, aDataTransfer);
    return NS_OK;
  }

  // for synthetic events, just use the supplied data transfer object
  if (mEventIsInternal) {
    NS_IF_ADDREF(*aDataTransfer = dragEvent->dataTransfer);
    return NS_OK;
  }

  // For draggesture and dragstart events, the data transfer object is
  // created before the event fires, so it should already be set. For other
  // drag events, get the object from the drag session.
  NS_ASSERTION(mEvent->message != NS_DRAGDROP_GESTURE &&
               mEvent->message != NS_DRAGDROP_START,
               "draggesture event created without a dataTransfer");

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  NS_ENSURE_TRUE(dragSession, NS_OK); // no drag in progress

  nsCOMPtr<nsIDOMDataTransfer> initialDataTransfer;
  dragSession->GetDataTransfer(getter_AddRefs(initialDataTransfer));
  if (!initialDataTransfer) {
    // A dataTransfer won't exist when a drag was started by some other
    // means, for instance calling the drag service directly, or a drag
    // from another application. In either case, a new dataTransfer should
    // be created that reflects the data. Pass true to the constructor for
    // the aIsExternal argument, so that only system access is allowed.
    PRUint32 action = 0;
    dragSession->GetDragAction(&action);
    initialDataTransfer =
      new nsDOMDataTransfer(mEvent->message, action);
    NS_ENSURE_TRUE(initialDataTransfer, NS_ERROR_OUT_OF_MEMORY);

    // now set it in the drag session so we don't need to create it again
    dragSession->SetDataTransfer(initialDataTransfer);
  }

  // each event should use a clone of the original dataTransfer.
  nsCOMPtr<nsIDOMNSDataTransfer> initialDataTransferNS =
    do_QueryInterface(initialDataTransfer);
  NS_ENSURE_TRUE(initialDataTransferNS, NS_ERROR_FAILURE);
  initialDataTransferNS->Clone(mEvent->message,
                               getter_AddRefs(dragEvent->dataTransfer));
  NS_ENSURE_TRUE(dragEvent->dataTransfer, NS_ERROR_OUT_OF_MEMORY);

  // for the dragenter and dragover events, initialize the drop effect
  // from the drop action, which platform specific widget code sets before
  // the event is fired based on the keyboard state.
  if (mEvent->message == NS_DRAGDROP_ENTER ||
      mEvent->message == NS_DRAGDROP_OVER) {
    nsCOMPtr<nsIDOMNSDataTransfer> newDataTransfer =
      do_QueryInterface(dragEvent->dataTransfer);
    NS_ENSURE_TRUE(newDataTransfer, NS_ERROR_FAILURE);

    PRUint32 action, effectAllowed;
    dragSession->GetDragAction(&action);
    newDataTransfer->GetEffectAllowedInt(&effectAllowed);
    newDataTransfer->SetDropEffectInt(FilterDropEffect(action, effectAllowed));
  }
  else if (mEvent->message == NS_DRAGDROP_DROP ||
           mEvent->message == NS_DRAGDROP_DRAGDROP ||
           mEvent->message == NS_DRAGDROP_END) {
    // For the drop and dragend events, set the drop effect based on the
    // last value that the dropEffect had. This will have been set in
    // nsEventStateManager::PostHandleEvent for the last dragenter or
    // dragover event.
    nsCOMPtr<nsIDOMNSDataTransfer> newDataTransfer =
      do_QueryInterface(dragEvent->dataTransfer);
    NS_ENSURE_TRUE(newDataTransfer, NS_ERROR_FAILURE);

    PRUint32 dropEffect;
    initialDataTransferNS->GetDropEffectInt(&dropEffect);
    newDataTransfer->SetDropEffectInt(dropEffect);
  }

  NS_IF_ADDREF(*aDataTransfer = dragEvent->dataTransfer);
  return NS_OK;
}

// static
PRUint32
nsDOMDragEvent::FilterDropEffect(PRUint32 aAction, PRUint32 aEffectAllowed)
{
  // It is possible for the drag action to include more than one action, but
  // the widget code which sets the action from the keyboard state should only
  // be including one. If multiple actions were set, we just consider them in
  //  the following order:
  //   copy, link, move
  if (aAction & nsIDragService::DRAGDROP_ACTION_COPY)
    aAction = nsIDragService::DRAGDROP_ACTION_COPY;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_LINK)
    aAction = nsIDragService::DRAGDROP_ACTION_LINK;
  else if (aAction & nsIDragService::DRAGDROP_ACTION_MOVE)
    aAction = nsIDragService::DRAGDROP_ACTION_MOVE;

  // Filter the action based on the effectAllowed. If the effectAllowed
  // doesn't include the action, then that action cannot be done, so adjust
  // the action to something that is allowed. For a copy, adjust to move or
  // link. For a move, adjust to copy or link. For a link, adjust to move or
  // link. Otherwise, use none.
  if (aAction & aEffectAllowed ||
      aEffectAllowed == nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
    return aAction;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_MOVE)
    return nsIDragService::DRAGDROP_ACTION_MOVE;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_COPY)
    return nsIDragService::DRAGDROP_ACTION_COPY;
  if (aEffectAllowed & nsIDragService::DRAGDROP_ACTION_LINK)
    return nsIDragService::DRAGDROP_ACTION_LINK;
  return nsIDragService::DRAGDROP_ACTION_NONE;
}

nsresult NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsDragEvent *aEvent) 
{
  nsDOMDragEvent* event = new nsDOMDragEvent(aPresContext, aEvent);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(event, aInstancePtrResult);
}
