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
#include "nsJSEventListener.h"
#include "nsString.h"
#include "nsIScriptEventListener.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptObjectOwner.h"

static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIJSEventListenerIID, NS_IJSEVENTLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(nsIScriptContext *aContext, 
                                     nsIScriptObjectOwner *aOwner) 
{
  NS_INIT_REFCNT();

  // Both of these are weak references. We are guaranteed
  // because of the ownership model that this object will be
  // freed (and the references dropped) before either the context
  // or the owner goes away.
  mContext = aContext;
  mOwner = aOwner;
  mReturnResult = nsReturnResult_eNotSet;
}

nsJSEventListener::~nsJSEventListener() 
{
}

nsresult nsJSEventListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMEventListenerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIJSEventListenerIID)) {
    *aInstancePtr = (void*)(nsIJSEventListener*)this;
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

NS_IMPL_ADDREF(nsJSEventListener)

NS_IMPL_RELEASE(nsJSEventListener)

//static nsString onPrefix = "on";

nsresult nsJSEventListener::SetEventName(nsIAtom* aName)
{
  mEventName = aName;
  return NS_OK;
}

nsresult nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  jsval funval;
  jsval argv[1];
  JSObject *eventObj;
  char* eventChars;
  nsAutoString eventString;
  // XXX This doesn't seem like the correct context on which to execute
  // the event handler. Might need to get one from the JS thread context
  // stack.
  JSContext* cx = (JSContext*)mContext->GetNativeContext();
  JSObject* obj;
  nsresult result = NS_OK;

  if (!mEventName) {
    if (NS_OK != aEvent->GetType(eventString)) {
      //JS can't handle this event yet or can't handle it at all
      return NS_OK;
    }
    if (mReturnResult == nsReturnResult_eNotSet) {
      if (eventString.EqualsWithConversion("error") || eventString.EqualsWithConversion("mouseover")) {
        mReturnResult = nsReturnResult_eReverseReturnResult;
      }
      else {
        mReturnResult = nsReturnResult_eDoNotReverseReturnResult;
      }
    }
    eventString.InsertWithConversion("on", 0, 2);
  }
  else {
    mEventName->ToString(eventString);
  }

  eventChars = eventString.ToNewCString();
  
  result = mOwner->GetScriptObject(mContext, (void**)&obj);
  if (NS_FAILED(result)) {
    return result;
  }

  if (!JS_LookupProperty(cx, obj, eventChars, &funval)) {
    nsCRT::free(eventChars);
    return NS_ERROR_FAILURE;
  }

  nsCRT::free(eventChars);

  if (JS_TypeOfValue(cx, funval) != JSTYPE_FUNCTION) {
    return NS_OK;
  }

  result = NS_NewScriptKeyEvent(mContext, aEvent, nsnull, (void**)&eventObj);
  if (NS_FAILED(result)) {
    return NS_ERROR_FAILURE;
  }

  argv[0] = OBJECT_TO_JSVAL(eventObj);
  PRBool jsBoolResult;
  PRBool returnResult = mReturnResult == nsReturnResult_eReverseReturnResult ? PR_TRUE : PR_FALSE;
  result = mContext->CallEventHandler(obj, 
                                      (void*) JSVAL_TO_OBJECT(funval), 
                                      1, 
                                      argv, 
                                      &jsBoolResult, 
                                      returnResult);
  if (NS_FAILED(result)) {
    return result;
  }
  if (!jsBoolResult) 
    aEvent->PreventDefault();

  return result;
}

NS_IMETHODIMP 
nsJSEventListener::GetEventTarget(nsIScriptContext**aContext, 
                                  nsIScriptObjectOwner** aOwner)
{
  NS_ASSERTION(aContext && aOwner, "null argument");
  if (aContext) {
    *aContext = mContext;
    NS_ADDREF(mContext);
  }
  if (aOwner) { 
    *aOwner = mOwner;
    NS_ADDREF(mOwner);
  }

  return NS_OK;
}

/*
 * Factory functions
 */

extern "C" NS_DOM nsresult NS_NewJSEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, nsIScriptObjectOwner *aOwner)
{
  nsJSEventListener* it = new nsJSEventListener(aContext, aOwner);
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

