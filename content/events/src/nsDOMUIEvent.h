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

#ifndef nsDOMUIEvent_h
#define nsDOMUIEvent_h

#include "nsIDOMUIEvent.h"
#include "nsDOMEvent.h"
#include "nsLayoutUtils.h"
#include "nsEvent.h"

class nsDOMUIEvent : public nsDOMEvent,
                     public nsIDOMUIEvent
{
public:
  nsDOMUIEvent(nsPresContext* aPresContext, nsGUIEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMUIEvent, nsDOMEvent)

  // nsIDOMUIEvent Interface
  NS_DECL_NSIDOMUIEVENT

  // nsIPrivateDOMEvent interface
  NS_IMETHOD DuplicatePrivateData();
  virtual void Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType);
  virtual bool Deserialize(const IPC::Message* aMsg, void** aIter);
  
  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  NS_FORWARD_NSIDOMNSEVENT(nsDOMEvent::)

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);

  static nsIntPoint CalculateScreenPoint(nsPresContext* aPresContext,
                                         nsEvent* aEvent)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_POPUP_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_MOZTOUCH_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT)) {
      return nsIntPoint(0, 0);
    }

    if (!((nsGUIEvent*)aEvent)->widget ) {
      return aEvent->refPoint;
    }

    nsIntPoint offset = aEvent->refPoint +
                        ((nsGUIEvent*)aEvent)->widget->WidgetToScreenOffset();
    nscoord factor = aPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                      nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
  }

  static nsIntPoint CalculateClientPoint(nsPresContext* aPresContext,
                                         nsEvent* aEvent,
                                         nsIntPoint* aDefaultClientPoint)
  {
    if (!aEvent ||
        (aEvent->eventStructType != NS_MOUSE_EVENT &&
         aEvent->eventStructType != NS_POPUP_EVENT &&
         aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
         aEvent->eventStructType != NS_MOZTOUCH_EVENT &&
         aEvent->eventStructType != NS_DRAG_EVENT &&
         aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
        !aPresContext ||
        !((nsGUIEvent*)aEvent)->widget) {
      return (nsnull == aDefaultClientPoint ? nsIntPoint(0, 0) :
        nsIntPoint(aDefaultClientPoint->x, aDefaultClientPoint->y));
    }

    nsPoint pt(0, 0);
    nsIPresShell* shell = aPresContext->GetPresShell();
    if (!shell) {
      return nsIntPoint(0, 0);
    }
    nsIFrame* rootFrame = shell->GetRootFrame();
    if (rootFrame) {
      pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, rootFrame);
    }

    return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                      nsPresContext::AppUnitsToIntCSSPixels(pt.y));
  }

protected:
  // Internal helper functions
  nsIntPoint GetScreenPoint();
  nsIntPoint GetClientPoint();
  nsIntPoint GetMovementPoint();
  nsIntPoint GetLayerPoint();
  nsIntPoint GetPagePoint();

  // Allow specializations.
  virtual nsresult Which(PRUint32* aWhich)
  {
    NS_ENSURE_ARG_POINTER(aWhich);
    // Usually we never reach here, as this is reimplemented for mouse and keyboard events.
    *aWhich = 0;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> mView;
  PRInt32 mDetail;
  nsIntPoint mClientPoint;
  // Screenpoint is mEvent->refPoint.
  nsIntPoint mLayerPoint;
  nsIntPoint mPagePoint;
  nsIntPoint mMovement;
  bool mIsPointerLocked;
  nsIntPoint mLastScreenPoint;
  nsIntPoint mLastClientPoint;

  typedef mozilla::widget::Modifiers Modifiers;
  static Modifiers ComputeModifierState(const nsAString& aModifiersList);
  bool GetModifierStateInternal(const nsAString& aKey);
};

#define NS_FORWARD_TO_NSDOMUIEVENT \
  NS_FORWARD_NSIDOMUIEVENT(nsDOMUIEvent::) \
  NS_FORWARD_TO_NSDOMEVENT

#endif // nsDOMUIEvent_h
