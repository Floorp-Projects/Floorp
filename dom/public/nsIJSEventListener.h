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

#ifndef nsIJSEventListener_h__
#define nsIJSEventListener_h__

#include "nsISupports.h"
#include "jsapi.h"

class nsIScriptContext;
class nsIScriptObjectOwner;
class nsIDOMEventListener;
class nsString;

#define NS_IJSEVENTLISTENER_IID     \
{ 0xa6cf9118, 0x15b3, 0x11d2,       \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Implemented by JS event listeners. Used to retrieve the
// JSObject corresponding to the event target.
class nsIJSEventListener : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJSEVENTLISTENER_IID)

  NS_IMETHOD GetEventTarget(nsIScriptContext** aContext, nsIScriptObjectOwner** aOwner) = 0;
  NS_IMETHOD SetEventName(nsIAtom* aName) = 0;
};

extern "C" NS_DOM nsresult NS_NewJSEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, nsIScriptObjectOwner* aOwner);

#endif // nsIJSEventListener_h__
