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

#include "nsCOMPtr.h"
#include "nsIDOMKeyEvent.h"
#include "nsIScriptContext.h"
#include "nsIScriptEventListener.h"
#include "nsIDOMMouseListener.h"
#include "jsapi.h"

//nsIDOMMouseListener interface
class nsJSDOMEventListener : public nsIDOMEventListener, public nsIScriptEventListener {
public:
  nsJSDOMEventListener(nsIScriptContext* aContext, JSObject *aTarget, JSObject *aHandler);
  virtual ~nsJSDOMEventListener();

  NS_DECL_ISUPPORTS

  //nsIDOMEventListener interface
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  //nsIScriptEventListener interface
  NS_IMETHOD CheckIfEqual(nsIScriptEventListener *aListener, PRBool* aResult);
  NS_IMETHOD GetInternals(void** aTarget, void** aHandler);

protected:
  nsCOMPtr<nsIScriptContext> mContext;
  JSObject  *mTarget;
  JSObject  *mHandler;
};

#endif //nsJSEventListener_h__
