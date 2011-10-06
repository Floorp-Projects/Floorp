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

#ifndef nsIPrivateDOMEvent_h__
#define nsIPrivateDOMEvent_h__

#include "nsISupports.h"

#define NS_IPRIVATEDOMEVENT_IID \
{ 0x26f5f0e9, 0x2960, 0x4405, \
  { 0xa1, 0x7e, 0xd8, 0x9f, 0xb0, 0xd9, 0x4c, 0x71 } }

class nsIDOMEventTarget;
class nsIDOMEvent;
class nsEvent;
class nsCommandEvent;
class nsPresContext;
class nsInvalidateRequestList;
namespace IPC {
class Message;
}

class nsIPrivateDOMEvent : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATEDOMEVENT_IID)

  NS_IMETHOD DuplicatePrivateData() = 0;
  NS_IMETHOD SetTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD_(bool) IsDispatchStopped() = 0;
  NS_IMETHOD_(nsEvent*) GetInternalNSEvent() = 0;
  NS_IMETHOD SetTrusted(bool aTrusted) = 0;
  virtual void Serialize(IPC::Message* aMsg,
                         bool aSerializeInterfaceType) = 0;
  virtual bool Deserialize(const IPC::Message* aMsg, void** aIter) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateDOMEvent, NS_IPRIVATEDOMEVENT_IID)

nsresult
NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, nsEvent *aEvent);
nsresult
NS_NewDOMDataContainerEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, nsEvent *aEvent);
nsresult
NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsGUIEvent *aEvent);
nsresult
NS_NewDOMMouseEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsInputEvent *aEvent);
nsresult
NS_NewDOMMouseScrollEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsInputEvent *aEvent);
nsresult
NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsDragEvent *aEvent);
nsresult
NS_NewDOMKeyboardEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsKeyEvent *aEvent);
nsresult
NS_NewDOMCompositionEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsCompositionEvent *aEvent);
nsresult
NS_NewDOMMutationEvent(nsIDOMEvent** aResult NS_OUTPARAM, nsPresContext* aPresContext, class nsMutationEvent* aEvent);
nsresult
NS_NewDOMPopupBlockedEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, nsEvent* aEvent);
nsresult
NS_NewDOMDeviceOrientationEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, nsEvent* aEvent);
nsresult
NS_NewDOMDeviceMotionEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, nsEvent* aEvent);
nsresult
NS_NewDOMTextEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsTextEvent* aEvent);
nsresult
NS_NewDOMBeforeUnloadEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, nsEvent* aEvent);
nsresult
NS_NewDOMPageTransitionEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, nsEvent* aEvent);
nsresult
NS_NewDOMSVGEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMSVGZoomEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsGUIEvent* aEvent);
#ifdef MOZ_SMIL
nsresult
NS_NewDOMTimeEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsEvent* aEvent);
#endif // MOZ_SMIL
nsresult
NS_NewDOMXULCommandEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsInputEvent* aEvent);
nsresult
NS_NewDOMCommandEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, nsCommandEvent* aEvent);
nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMProgressEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsEvent* aEvent);
// This empties aInvalidateRequests.
nsresult
NS_NewDOMNotifyPaintEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext,
                          nsEvent* aEvent,
                          PRUint32 aEventType = 0,
                          nsInvalidateRequestList* aInvalidateRequests = nsnull);
nsresult
NS_NewDOMAudioAvailableEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext,
                             nsEvent* aEvent,
                             PRUint32 aEventType = 0,
                             float* aFrameBuffer = nsnull,
                             PRUint32 aFrameBufferLength = 0,
                             float aTime = 0);
nsresult
NS_NewDOMSimpleGestureEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsSimpleGestureEvent* aEvent);
nsresult
NS_NewDOMScrollAreaEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsScrollAreaEvent* aEvent);
nsresult
NS_NewDOMTransitionEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsTransitionEvent* aEvent);
nsresult
NS_NewDOMAnimationEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsAnimationEvent* aEvent);
nsresult
NS_NewDOMCloseEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMMozTouchEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsMozTouchEvent* aEvent);
nsresult
NS_NewDOMTouchEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsInputEvent *aEvent);
nsresult
NS_NewDOMCustomEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, nsEvent* aEvent);
#endif // nsIPrivateDOMEvent_h__
