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
 * Steve Clark (buster@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsDOMUIEvent.h"
#include "nsIPresShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMNode.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIWidget.h"
#include "nsIPresShell.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"

nsDOMUIEvent::nsDOMUIEvent(nsPresContext* aPresContext, nsGUIEvent* aEvent)
: nsDOMEvent(aPresContext, aEvent)
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
    mEventIsTrusted = PR_TRUE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEventIsTrusted = PR_FALSE;
    mEvent->time = PR_Now();
  }
  
  // Fill mDetail and mView according to the mEvent (widget-generated event) we've got
  switch(mEvent->eventStructType)
  {
    case NS_UI_EVENT:
    {
      nsUIEvent *event = NS_STATIC_CAST(nsUIEvent*, mEvent);
      mDetail = event->detail;
      break;
    }

    case NS_SCROLLPORT_EVENT:
    {
      nsScrollPortEvent* scrollEvent = NS_STATIC_CAST(nsScrollPortEvent*, mEvent);
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

NS_IMPL_ADDREF_INHERITED(nsDOMUIEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMUIEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMUIEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSUIEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(UIEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

nsPoint nsDOMUIEvent::GetScreenPoint() {
  if (!mEvent || 
       (mEvent->eventStructType != NS_MOUSE_EVENT &&
        mEvent->eventStructType != NS_POPUP_EVENT &&
        !NS_IS_DRAG_EVENT(mEvent))) {
    return nsPoint(0, 0);
  }

  if (!((nsGUIEvent*)mEvent)->widget ) {
    return mEvent->refPoint;
  }
    
  nsRect bounds(mEvent->refPoint, nsSize(1, 1));
  nsRect offset;
  ((nsGUIEvent*)mEvent)->widget->WidgetToScreen ( bounds, offset );
  return offset.TopLeft();
}

nsPoint nsDOMUIEvent::GetClientPoint() {
  if (!mEvent || 
       (mEvent->eventStructType != NS_MOUSE_EVENT &&
        mEvent->eventStructType != NS_POPUP_EVENT &&
        !NS_IS_DRAG_EVENT(mEvent)) ||
      !mPresContext) {
    return nsPoint(0, 0);
  }

  if (!((nsGUIEvent*)mEvent)->widget ) {
    return mEvent->point;
  }

  //My god, man, there *must* be a better way to do this.
  nsCOMPtr<nsIWidget> docWidget;
  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    nsIViewManager* vm = presShell->GetViewManager();
    if (vm) {
      vm->GetWidget(getter_AddRefs(docWidget));
    }
  }

  nsPoint pt = mEvent->refPoint;

  nsCOMPtr<nsIWidget> eventWidget = ((nsGUIEvent*)mEvent)->widget;
  while (eventWidget && docWidget != eventWidget) {
    nsWindowType windowType;
    eventWidget->GetWindowType(windowType);
    if (windowType == eWindowType_popup)
      break;

    nsRect bounds;
    eventWidget->GetBounds(bounds);
    pt += bounds.TopLeft();
    eventWidget = dont_AddRef(eventWidget->GetParent());
  }
  
  if (eventWidget != docWidget) {
    // docWidget wasn't on the chain from the event widget to the root
    // of the widget tree (or the nearest popup). OK, so now pt is
    // relative to eventWidget; to get it relative to docWidget, we
    // need to subtract docWidget's offset from eventWidget.
    while (docWidget && docWidget != eventWidget) {
      nsWindowType windowType;
      docWidget->GetWindowType(windowType);
      if (windowType == eWindowType_popup) {
        // oh dear. the doc and the event were in different popups?
        // That shouldn't happen.
        NS_NOTREACHED("doc widget and event widget are in different popups. That's dumb.");
        break;
      }
      
      nsRect bounds;
      docWidget->GetBounds(bounds);
      pt -= bounds.TopLeft();
      docWidget = docWidget->GetParent();
    }
  }
  
  return pt;
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

NS_IMETHODIMP
nsDOMUIEvent::GetPageX(PRInt32* aPageX)
{
  NS_ENSURE_ARG_POINTER(aPageX);
  nsresult ret = NS_OK;
  PRInt32 scrollX = 0;
  nsIScrollableView* view = nsnull;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);
  if(view) {
    nscoord xPos, yPos;
    ret = view->GetScrollPosition(xPos, yPos);
    scrollX = NSTwipsToIntPixels(xPos, t2p);
  }

  if (NS_SUCCEEDED(ret)) {
    *aPageX = GetClientPoint().x + scrollX;
  }

  return ret;
}

NS_IMETHODIMP
nsDOMUIEvent::GetPageY(PRInt32* aPageY)
{
  NS_ENSURE_ARG_POINTER(aPageY);
  nsresult ret = NS_OK;
  PRInt32 scrollY = 0;
  nsIScrollableView* view = nsnull;
  float p2t, t2p;

  GetScrollInfo(&view, &p2t, &t2p);
  if(view) {
    nscoord xPos, yPos;
    ret = view->GetScrollPosition(xPos, yPos);
    scrollY = NSTwipsToIntPixels(yPos, t2p);
  }

  if (NS_SUCCEEDED(ret)) {
    *aPageY = GetClientPoint().y + scrollY;
  }

  return ret;
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
    nsCOMPtr<nsIContent> parent;
    PRInt32 offset, endOffset;
    PRBool beginOfContent;
    if (NS_SUCCEEDED(targetFrame->GetContentAndOffsetsFromPoint(mPresContext, 
                                              mEvent->point,
                                              getter_AddRefs(parent),
                                              offset,
                                              endOffset,
                                              beginOfContent))) {
      if (parent) {
        return CallQueryInterface(parent, aRangeParent);
      }
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
    nsIContent* parent = nsnull;
    PRInt32 endOffset;
    PRBool beginOfContent;
    if (NS_SUCCEEDED(targetFrame->GetContentAndOffsetsFromPoint(mPresContext, 
                                              mEvent->point,
                                              &parent,
                                              *aRangeOffset,
                                              endOffset,
                                              beginOfContent))) {
      NS_IF_RELEASE(parent);
      return NS_OK;
    }
  }
  *aRangeOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetCancelBubble(PRBool* aCancelBubble)
{
  NS_ENSURE_ARG_POINTER(aCancelBubble);
  if (mEvent->flags & NS_EVENT_FLAG_BUBBLE || mEvent->flags & NS_EVENT_FLAG_INIT) {
    *aCancelBubble = (mEvent->flags &= NS_EVENT_FLAG_STOP_DISPATCH) ? PR_TRUE : PR_FALSE;
  }
  else {
    *aCancelBubble = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::SetCancelBubble(PRBool aCancelBubble)
{
  if (mEvent->flags & NS_EVENT_FLAG_BUBBLE || mEvent->flags & NS_EVENT_FLAG_INIT) {
    if (aCancelBubble) {
      mEvent->flags |= NS_EVENT_FLAG_STOP_DISPATCH;
    }
    else {
      mEvent->flags &= ~NS_EVENT_FLAG_STOP_DISPATCH;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerX(PRInt32* aLayerX)
{
  NS_ENSURE_ARG_POINTER(aLayerX);
  if (!mEvent || (mEvent->eventStructType != NS_MOUSE_EVENT) ||
      !mPresContext) {
    *aLayerX = 0;
    return NS_OK;
  }

  float t2p;
  t2p = mPresContext->TwipsToPixels();
  *aLayerX = NSTwipsToIntPixels(mEvent->point.x, t2p);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMUIEvent::GetLayerY(PRInt32* aLayerY)
{
  NS_ENSURE_ARG_POINTER(aLayerY);
  if (!mEvent || (mEvent->eventStructType != NS_MOUSE_EVENT) ||
      !mPresContext) {
    *aLayerY = 0;
    return NS_OK;
  }

  float t2p;
  t2p = mPresContext->TwipsToPixels();
  *aLayerY = NSTwipsToIntPixels(mEvent->point.y, t2p);
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
  *aReturn = mEvent ?
    ((mEvent->flags & NS_EVENT_FLAG_NO_DEFAULT) ? PR_TRUE : PR_FALSE) :
    PR_FALSE;
  return NS_OK;
}

nsresult
nsDOMUIEvent::GetScrollInfo(nsIScrollableView** aScrollableView,
                            float* aP2T, float* aT2P)
{
  NS_ENSURE_ARG_POINTER(aScrollableView);
  NS_ENSURE_ARG_POINTER(aP2T);
  NS_ENSURE_ARG_POINTER(aT2P);
  if (!mPresContext) {
    *aScrollableView = nsnull;
    return NS_ERROR_FAILURE;
  }

  *aP2T = mPresContext->PixelsToTwips();
  *aT2P = mPresContext->TwipsToPixels();

  nsIPresShell *presShell = mPresContext->GetPresShell();
  if (presShell) {
    nsIViewManager* vm = presShell->GetViewManager();
    if(vm) {
      return vm->GetRootScrollableView(aScrollableView);
    }
  }
  return NS_OK;
}

NS_METHOD nsDOMUIEvent::GetCompositionReply(nsTextEventReply** aReply)
{
  if((mEvent->eventStructType == NS_RECONVERSION_EVENT) ||
     (mEvent->message == NS_COMPOSITION_START) ||
     (mEvent->message == NS_COMPOSITION_QUERY))
  {
    *aReply = &(NS_STATIC_CAST(nsCompositionEvent*, mEvent)->theReply);
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
    *aReply = &(NS_STATIC_CAST(nsReconversionEvent*, mEvent)->theReply);
    return NS_OK;
  }
  aReply = nsnull;
  return NS_ERROR_FAILURE;
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
