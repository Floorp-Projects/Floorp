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
#include "nsJSDOMEventListener.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsJSUtils.h"
#include "jsapi.h"

static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * nsJSDOMEventListener implementation
 */
nsJSDOMEventListener::nsJSDOMEventListener(JSContext *aContext, JSObject *aTarget, JSObject *aHandler) 
{
  NS_INIT_REFCNT();
  mContext = aContext;
  mTarget = aTarget;
  mHandler = aHandler;
}

nsJSDOMEventListener::~nsJSDOMEventListener() 
{
}

nsresult nsJSDOMEventListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptEventListenerIID)) {
    *aInstancePtr = (void*)(nsIScriptEventListener*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMEventListener*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsJSDOMEventListener)

NS_IMPL_RELEASE(nsJSDOMEventListener)

nsresult nsJSDOMEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  jsval argv[1];
  JSObject *eventObj;

  nsCOMPtr<nsIScriptContext> scriptCX;
  nsJSUtils::nsGetStaticScriptContext(mContext, mTarget, 
                                      getter_AddRefs(scriptCX));
  if (!scriptCX) {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_NewScriptKeyEvent(scriptCX, aEvent, nsnull, (void**)&eventObj)) {
    return NS_ERROR_FAILURE;
  }

  argv[0] = OBJECT_TO_JSVAL(eventObj);
  PRBool jsBoolResult;
  if (NS_FAILED(scriptCX->CallEventHandler(mTarget, mHandler, 1, argv, &jsBoolResult))) {
    return NS_ERROR_FAILURE;
  }
  return jsBoolResult ? NS_OK : NS_ERROR_FAILURE;
}

nsresult nsJSDOMEventListener::CheckIfEqual(nsIScriptEventListener *aListener)
{
  /*
   * This should be rewritten to use a PRBool out parameter in stead of
   * using the return value to show if this equals aListener...
   */
  return NS_ERROR_FAILURE;
}

/*
 * Factory functions
 */

extern "C" NS_DOM nsresult
NS_NewScriptEventListener(nsIDOMEventListener ** aInstancePtrResult,
                          nsIScriptContext *aContext,
                          void* aTarget,
                          void *aHandler)
{
  JSContext *mCX = (JSContext*)aContext->GetNativeContext();
  
  nsJSDOMEventListener* it = new nsJSDOMEventListener(mCX, (JSObject*)aTarget, (JSObject*)aHandler);
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

