/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsJSEventListener_h__
#define nsJSEventListener_h__

#include "nsIDOMKeyEvent.h"
#include "nsIScriptEventListener.h"
#include "nsIJSEventListener.h"
#include "nsIDOMMouseListener.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"

//nsIDOMMouseListener interface
class nsJSEventListener : public nsIDOMEventListener,
                          public nsIJSEventListener
{
public:
  nsJSEventListener(nsIScriptContext *aContext, nsIScriptObjectOwner* aOwner);
  virtual ~nsJSEventListener();

  NS_DECL_ISUPPORTS

  //nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  //nsIJSEventListener interface
  NS_IMETHOD GetEventTarget(nsIScriptContext** aContext, nsIScriptObjectOwner** aOwner);
  
  NS_IMETHOD SetEventName(nsIAtom* aName);

protected:
  nsIScriptContext* mContext;
  nsIScriptObjectOwner* mOwner;
  nsCOMPtr<nsIAtom> mEventName;
};


#endif //nsJSEventListener_h__

