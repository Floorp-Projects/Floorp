/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIPrivateDOMEvent_h__
#define nsIPrivateDOMEvent_h__

#include "nsGUIEvent.h"
#include "nsISupports.h"

class nsIPresContext;

/*
 * Event listener manager interface.
 */
#define NS_IPRIVATEDOMEVENT_IID \
{ /* 80a98c80-2036-11d2-bd89-00805f8ae3f4 */ \
0x80a98c80, 0x2036, 0x11d2, \
{0xbd, 0x89, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsIDOMEventTarget;
class nsIDOMEvent;

class nsIPrivateDOMEvent : public nsISupports {

public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATEDOMEVENT_IID; return iid; }

  NS_IMETHOD DuplicatePrivateData() = 0;
  NS_IMETHOD SetTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD SetCurrentTarget(nsIDOMEventTarget* aTarget) = 0;
  NS_IMETHOD IsDispatchStopped(PRBool* aIsDispatchPrevented) = 0;
  NS_IMETHOD GetInternalNSEvent(nsEvent** aNSEvent) = 0;
  NS_IMETHOD GetRealTarget(nsIDOMEventTarget** aRealTarget) = 0;
};

extern nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult, nsIPresContext* aPresContext, nsEvent *aEvent);
extern nsresult NS_NewDOMUIEvent(nsIDOMEvent** aInstancePtrResult,
                                 nsIPresContext* aPresContext,
                                 const nsAReadableString& aEventType,
                                 nsEvent *aEvent);

#endif // nsIPrivateDOMEvent_h__
