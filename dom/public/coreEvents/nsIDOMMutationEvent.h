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

#ifndef nsIDOMMutationEvent_h__
#define nsIDOMMutationEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMEvent.h"

class nsIDOMNode;

#define NS_IDOMMUTATIONEVENT_IID \
 { 0x8e440d86, 0x886a, 0x4e76, { 0x9e, 0x59, 0xc1, 0x3b, 0x93, 0x9c, 0x9a, 0x4b } } 

class nsIDOMMutationEvent : public nsIDOMEvent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMMUTATIONEVENT_IID; return iid; }
  enum {
    MODIFICATION = 1,
    ADDITION = 2,
    REMOVAL = 3
  };

  NS_IMETHOD    GetRelatedNode(nsIDOMNode** aRelatedNode)=0;

  NS_IMETHOD    GetPrevValue(nsAWritableString& aPrevValue)=0;

  NS_IMETHOD    GetNewValue(nsAWritableString& aNewValue)=0;

  NS_IMETHOD    GetAttrName(nsAWritableString& aAttrName)=0;

  NS_IMETHOD    GetAttrChange(PRUint16* aAttrChange)=0;

  NS_IMETHOD    InitMutationEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAReadableString& aPrevValueArg, const nsAReadableString& aNewValueArg, const nsAReadableString& aAttrNameArg)=0;
};


#define NS_DECL_IDOMMUTATIONEVENT   \
  NS_IMETHOD    GetRelatedNode(nsIDOMNode** aRelatedNode);  \
  NS_IMETHOD    GetPrevValue(nsAWritableString& aPrevValue);  \
  NS_IMETHOD    GetNewValue(nsAWritableString& aNewValue);  \
  NS_IMETHOD    GetAttrName(nsAWritableString& aAttrName);  \
  NS_IMETHOD    GetAttrChange(PRUint16* aAttrChange);  \
  NS_IMETHOD    InitMutationEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAReadableString& aPrevValueArg, const nsAReadableString& aNewValueArg, const nsAReadableString& aAttrNameArg);  \



#define NS_FORWARD_IDOMMUTATIONEVENT(_to)  \
  NS_IMETHOD    GetRelatedNode(nsIDOMNode** aRelatedNode) { return _to GetRelatedNode(aRelatedNode); } \
  NS_IMETHOD    GetPrevValue(nsAWritableString& aPrevValue) { return _to GetPrevValue(aPrevValue); } \
  NS_IMETHOD    GetNewValue(nsAWritableString& aNewValue) { return _to GetNewValue(aNewValue); } \
  NS_IMETHOD    GetAttrName(nsAWritableString& aAttrName) { return _to GetAttrName(aAttrName); } \
  NS_IMETHOD    GetAttrChange(PRUint16* aAttrChange) { return _to GetAttrChange(aAttrChange); } \
  NS_IMETHOD    InitMutationEvent(const nsAReadableString& aTypeArg, PRBool aCanBubbleArg, PRBool aCancelableArg, nsIDOMNode* aRelatedNodeArg, const nsAReadableString& aPrevValueArg, const nsAReadableString& aNewValueArg, const nsAReadableString& aAttrNameArg) { return _to InitMutationEvent(aTypeArg, aCanBubbleArg, aCancelableArg, aRelatedNodeArg, aPrevValueArg, aNewValueArg, aAttrNameArg); }  \


extern "C" NS_DOM nsresult NS_InitMutationEventClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptMutationEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMMutationEvent_h__
