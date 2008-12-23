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

class nsPresContext;

/*
 * Event listener manager interface.
 */
#define NS_IPRIVATEDOMEVENT_IID \
{ /* 25e6626c-8e54-409b-87b5-2beceaac399e */ \
0x25e6626c, 0x8e54, 0x409b, \
{0x87, 0xb5, 0x2b, 0xec, 0xea, 0xac, 0x39, 0x9e} }

class nsIDOMEventTarget;
class nsIDOMEvent;
class nsEvent;
class nsCommandEvent;

class nsIPrivateDOMEvent : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATEDOMEVENT_IID)

  NS_IMETHOD DuplicatePrivateData() = 0;
  NS_IMETHOD SetTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD SetCurrentTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD SetOriginalTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD_(PRBool) IsDispatchStopped() = 0;
  NS_IMETHOD_(nsEvent*) GetInternalNSEvent() = 0;
  NS_IMETHOD_(PRBool) HasOriginalTarget() = 0;
  NS_IMETHOD SetTrusted(PRBool aTrusted) = 0;
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
NS_NewDOMMutationEvent(nsIDOMEvent** aResult NS_OUTPARAM, nsPresContext* aPresContext, class nsMutationEvent* aEvent);
nsresult
NS_NewDOMPopupBlockedEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsPopupBlockedEvent* aEvent);
nsresult
NS_NewDOMTextEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsTextEvent* aEvent);
nsresult
NS_NewDOMBeforeUnloadEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsBeforePageUnloadEvent* aEvent);
nsresult
NS_NewDOMPageTransitionEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsPageTransitionEvent* aEvent);
#ifdef MOZ_SVG
nsresult
NS_NewDOMSVGEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMSVGZoomEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsGUIEvent* aEvent);
#endif // MOZ_SVG
nsresult
NS_NewDOMXULCommandEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsXULCommandEvent* aEvent);
nsresult
NS_NewDOMCommandEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, nsCommandEvent* aEvent);
nsresult
NS_NewDOMMessageEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMProgressEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsEvent* aEvent);
nsresult
NS_NewDOMNotifyPaintEvent(nsIDOMEvent** aResult, nsPresContext* aPresContext, class nsNotifyPaintEvent* aEvent);
nsresult
NS_NewDOMSimpleGestureEvent(nsIDOMEvent** aInstancePtrResult, nsPresContext* aPresContext, class nsSimpleGestureEvent* aEvent);
#endif // nsIPrivateDOMEvent_h__
