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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark (buster@netscape.com)
 *   Ilya Konstantinov (mozilla-code@future.shiny.co.il)
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

#include "nsCOMPtr.h"
#include "nsDOMUIEvent.h"
#include "nsIPresShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"

nsDOMUIEvent::nsDOMUIEvent(nsPresContext* aPresContext, nsGUIEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent ?
               static_cast<nsEvent *>(aEvent) :
               static_cast<nsEvent *>(new nsUIEvent(PR_FALSE, 0, 0)))
  , mClientPoint(0, 0), mLayerPoint(0, 0), mPagePoint(0, 0)
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
  }
  
  // Fill mDetail and mView according to the mEvent (widget-generated
  // event) we've got
  switch(mEvent->eventStructType)
  {
    case NS_UI_EVENT:
    {
      nsUIEvent *event = static_cast<nsUIEvent*>(mEvent);
      mDetail = event->detail;
      break;
    }

    case NS_SCROLLPORT_EVENT:
    {
      nsScrollPortEvent* scrollEvent = static_cast<nsScrollPortEvent*>(mEvent);
      mDetail = (PRInt32)scrollEvent->orient;
      break;
    }

    default:
      mDetail = 0;
      break;
  }

  mView = nsnull;
  if (mPresContext)
  {
    nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
    if (container)
    {
       nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(container);
       if (window)
          mView = do_QueryInterface(window);
    }
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMUIEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMUIEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mView)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMUIEvent, nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mView)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsDOMUIEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMUIEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateCompositionEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(UIEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsPoint nsDOMUIEvent::GetScreenPoint() {
  if (!mEvent ||
      (mEvent->eventStructType != NS_MOUSE_EVENT &&
       mEvent->eventStructType != NS_POPUP_EVENT &&
       mEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
       !NS_IS_DRAG_EVENT(mEvent))) {
    return nsPoint(0, 0);
  }

  if (!((nsGUIEvent*)mEvent)->widget ) {
    return mEvent->refPoint;
  }

  nsRect bounds(mEvent->refPoint, nsSize(1, 1));
  nsRect offset;
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  PRInt32 factor = mPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
  return nsPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                 nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
}

nsPoint nsDOMUIEvent::GetClientPoint() {
  if (!mEvent ||
      (mEvent->eventStructType != NS_MOUSE_EVENT &&
       mEvent->eventStructType != NS_POPUP_EVENT &&
       mEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
       !NS_IS_DRAG_EVENT(mEvent)) ||
      !mPresContext ||
      !((nsGUIEvent*)mEvent)->widget) {
    return mClientPoint;
  }

  nsPoint pt(0, 0);
  nsIFrame* rootFrame = mPresContext->PresShell()->GetRootFrame();
  if (rootFrame)
    pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent, rootFrame);

  return nsPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                 nsPresContext::AppUnitsToIntCSSPixels(pt.y));
}

NS_IMETHODIMP
nsDOMUIEvent::GetView(nsIDOMAbstractView** aView)
{
  *aView = mView;
  NS_IF_ADDREF(*aView);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetDetail(PRInt32* aDetail)
{
  *aDetail = mDetail;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::InitUIEvent(const nsAString & typeArg, PRBool canBubbleArg, PRBool cancelableArg, nsIDOMAbstractView *viewArg, PRInt32 detailArg)
{
  nsresult rv = nsDOMEvent::InitEvent(typeArg, canBubbleArg, cancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mDetail = detailArg;
  mView = viewArg;

  return NS_OK;
}

// ---- nsDOMNSUIEvent implementation -------------------
nsPoint
nsDOMUIEvent::GetPagePoint()
{
  if (((nsGUIEvent*)mEvent)->widget) {
    // Native event; calculate using presentation
    nsPoint pt(0, 0);
    nsIScrollableFrame* scrollframe =
            mPresContext->PresShell()->GetRootScrollFrameAsScrollable();
    if (scrollframe)
      pt += scrollframe->GetScrollPosition();
    nsIFrame* rootFrame = mPresContext->PresShell()->GetRootFrame();
    if (rootFrame)
      pt += nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent, rootFrame);
    return nsPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                   nsPresContext::AppUnitsToIntCSSPixels(pt.y));
  }

  return mPagePoint;
}



NS_IMETHODIMP
nsDOMUIEvent::GetPageX(PRInt32* aPageX)
{
  NS_ENSURE_ARG_POINTER(aPageX);
  *aPageX = GetPagePoint().x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetPageY(PRInt32* aPageY)
{
  NS_ENSURE_ARG_POINTER(aPageY);
  *aPageY = GetPagePoint().y;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetWhich(PRUint32* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);
  // Usually we never reach here, as this is reimplemented for mouse and keyboard events.
  *aWhich = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetRangeParent(nsIDOMNode** aRangeParent)
{
  NS_ENSURE_ARG_POINTER(aRangeParent);
  nsIFrame* targetFrame = nsnull;

  if (mPresContext) {
    mPresContext->EventStateManager()->GetEventTarget(&targetFrame);
  }

  *aRangeParent = nsnull;

  if (targetFrame) {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent,
                                                              targetFrame);
    nsCOMPtr<nsIContent> parent = targetFrame->GetContentOffsetsFromPoint(pt).content;
    if (parent) {
      return CallQueryInterface(parent, aRangeParent);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetRangeOffset(PRInt32* aRangeOffset)
{
  NS_ENSURE_ARG_POINTER(aRangeOffset);
  nsIFrame* targetFrame = nsnull;

  if (mPresContext) {
    mPresContext->EventStateManager()->GetEventTarget(&targetFrame);
  }

  if (targetFrame) {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent,
                                                              targetFrame);
    *aRangeOffset = targetFrame->GetContentOffsetsFromPoint(pt).offset;
    return NS_OK;
  }
  *aRangeOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetCancelBubble(PRBool* aCancelBubble)
{
  NS_ENSURE_ARG_POINTER(aCancelBubble);
  *aCancelBubble =
    (mEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::SetCancelBubble(PRBool aCancelBubble)
{
  if (aCancelBubble) {
    mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
  } else {
    mEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH;
  }
  return NS_OK;
}

nsPoint nsDOMUIEvent::GetLayerPoint() {
  if (!mEvent ||
      (mEvent->eventStructType != NS_MOUSE_EVENT &&
       mEvent->eventStructType != NS_MOUSE_SCROLL_EVENT) ||
      !mPresContext ||
      mEventIsInternal) {
    return mLayerPoint;
  }
  // XXX I'm not really sure this is correct; it's my best shot, though
  nsIFrame* targetFrame;
  mPresContext->EventStateManager()->GetEventTarget(&targetFrame);
  if (!targetFrame)
    return mLayerPoint;
  nsIFrame* layer = nsLayoutUtils::GetClosestLayer(targetFrame);
  nsPoint pt(nsLayoutUtils::GetEventCoordinatesRelativeTo(mEvent, layer));
  pt.x =  nsPresContext::AppUnitsToIntCSSPixels(pt.x);
  pt.y =  nsPresContext::AppUnitsToIntCSSPixels(pt.y);
  return pt;
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerX(PRInt32* aLayerX)
{
  NS_ENSURE_ARG_POINTER(aLayerX);
  *aLayerX = GetLayerPoint().x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerY(PRInt32* aLayerY)
{
  NS_ENSURE_ARG_POINTER(aLayerY);
  *aLayerY = GetLayerPoint().y;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetIsChar(PRBool* aIsChar)
{
  switch(mEvent->eventStructType)
  {
    case NS_KEY_EVENT:
      *aIsChar = ((nsKeyEvent*)mEvent)->isChar;
      return NS_OK;
    case NS_TEXT_EVENT:
      *aIsChar = ((nsTextEvent*)mEvent)->isChar;
      return NS_OK;
    default:
      *aIsChar = PR_FALSE;
      return NS_OK;
  }
}

NS_IMETHODIMP
nsDOMUIEvent::GetPreventDefault(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = mEvent && (mEvent->flags & NS_EVENT_FLAG_NO_DEFAULT);

  return NS_OK;
}

NS_METHOD nsDOMUIEvent::GetCompositionReply(nsTextEventReply** aReply)
{
  if((mEvent->eventStructType == NS_RECONVERSION_EVENT) ||
     (mEvent->message == NS_COMPOSITION_START) ||
     (mEvent->message == NS_COMPOSITION_QUERY))
  {
    *aReply = &(static_cast<nsCompositionEvent*>(mEvent)->theReply);
    return NS_OK;
  }
  *aReply = nsnull;
  return NS_ERROR_FAILURE;
}

NS_METHOD
nsDOMUIEvent::GetReconversionReply(nsReconversionEventReply** aReply)
{
  if (mEvent->eventStructType == NS_RECONVERSION_EVENT)
  {
    *aReply = &(static_cast<nsReconversionEvent*>(mEvent)->theReply);
    return NS_OK;
  }
  *aReply = nsnull;
  return NS_ERROR_FAILURE;
}

NS_METHOD
nsDOMUIEvent::GetQueryCaretRectReply(nsQueryCaretRectEventReply** aReply)
{
  if (mEvent->eventStructType == NS_QUERYCARETRECT_EVENT)
  {
    *aReply = &(static_cast<nsQueryCaretRectEvent*>(mEvent)->theReply);
    return NS_OK;
  }
  *aReply = nsnull;
  return NS_ERROR_FAILURE;
}

NS_METHOD
nsDOMUIEvent::DuplicatePrivateData()
{
  mClientPoint = GetClientPoint();
  mLayerPoint = GetLayerPoint();
  mPagePoint = GetPagePoint();
  // GetScreenPoint converts mEvent->refPoint to right coordinates.
  nsPoint screenPoint = GetScreenPoint();
  nsresult rv = nsDOMEvent::DuplicatePrivateData();
  if (NS_SUCCEEDED(rv)) {
    mEvent->refPoint = screenPoint;
  }
  return rv;
}

nsresult NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult,
                          nsPresContext* aPresContext,
                          nsGUIEvent *aEvent) 
{
  nsDOMUIEvent* it = new nsDOMUIEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
