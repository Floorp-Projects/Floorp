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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMUIEvent_h__
#define nsIDOMUIEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMEvent.h"

class nsIDOMAbstractView;

#define NS_IDOMUIEVENT_IID \
 { 0xa6cf90c3, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMUIEvent : public nsIDOMEvent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMUIEVENT_IID; return iid; }

  NS_IMETHOD    GetView(nsIDOMAbstractView** aView)=0;

  NS_IMETHOD    GetDetail(PRInt32* aDetail)=0;

  NS_IMETHOD    InitUIEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg)=0;
};


#define NS_DECL_IDOMUIEVENT   \
  NS_IMETHOD    GetView(nsIDOMAbstractView** aView);  \
  NS_IMETHOD    GetDetail(PRInt32* aDetail);  \
  NS_IMETHOD    InitUIEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg);  \



#define NS_FORWARD_IDOMUIEVENT(_to)  \
  NS_IMETHOD    GetView(nsIDOMAbstractView** aView) { return _to GetView(aView); } \
  NS_IMETHOD    GetDetail(PRInt32* aDetail) { return _to GetDetail(aDetail); } \
  NS_IMETHOD    InitUIEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMAbstractView* aViewArg, PRInt32 aDetailArg) { return _to InitUIEvent(aTypeArg, aCanBubbleArg, aCancelableArg, aViewArg, aDetailArg); }  \


extern "C" NS_DOM nsresult NS_InitUIEventClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptUIEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMUIEvent_h__
