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

#ifndef nsIDOMEventCapturer_h__
#define nsIDOMEventCapturer_h__

#include "nsISupports.h"
#include "nsAWritableString.h"
#include "nsIScriptContext.h"
#include "nsIDOMEventReceiver.h"


#define NS_IDOMEVENTCAPTURER_IID \
 { 0xa6cf906c, 0x15b3, 0x11d2, \
  { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } } 

class nsIDOMEventCapturer : public nsIDOMEventReceiver {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMEVENTCAPTURER_IID; return iid; }

  NS_IMETHOD    CaptureEvent(const nsAReadableString& aType)=0;

  NS_IMETHOD    ReleaseEvent(const nsAReadableString& aType)=0;
};


#define NS_DECL_IDOMEVENTCAPTURER   \
  NS_IMETHOD    CaptureEvent(const nsAReadableString& aType);  \
  NS_IMETHOD    ReleaseEvent(const nsAReadableString& aType);  \



#define NS_FORWARD_IDOMEVENTCAPTURER(_to)  \
  NS_IMETHOD    CaptureEvent(const nsAReadableString& aType) { return _to CaptureEvent(aType); }  \
  NS_IMETHOD    ReleaseEvent(const nsAReadableString& aType) { return _to ReleaseEvent(aType); }  \


#endif // nsIDOMEventCapturer_h__
