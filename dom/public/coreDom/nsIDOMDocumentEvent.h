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

#ifndef nsIDOMDocumentEvent_h__
#define nsIDOMDocumentEvent_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMEvent;

#define NS_IDOMDOCUMENTEVENT_IID \
 { 0x46b91d66, 0x28e2, 0x11d4, \
  { 0xab, 0x1e, 0x00, 0x10, 0x83, 0x01, 0x23, 0xb4 } } 

class nsIDOMDocumentEvent : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOCUMENTEVENT_IID; return iid; }

  NS_IMETHOD    CreateEvent(const nsAReadableString& aEventType, nsIDOMEvent** aReturn)=0;
};


#define NS_DECL_IDOMDOCUMENTEVENT   \
  NS_IMETHOD    CreateEvent(const nsAReadableString& aEventType, nsIDOMEvent** aReturn);  \



#define NS_FORWARD_IDOMDOCUMENTEVENT(_to)  \
  NS_IMETHOD    CreateEvent(const nsAReadableString& aEventType, nsIDOMEvent** aReturn) { return _to CreateEvent(aEventType, aReturn); }  \


#endif // nsIDOMDocumentEvent_h__
