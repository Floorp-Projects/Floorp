/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsJSEventListener.h"
#include "nsString.h"
#include "nsIScriptEventListener.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"

static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * nsJSEventListener implementation
 */
nsJSEventListener::nsJSEventListener(JSContext *aContext, JSObject *aObj) 
{
  NS_INIT_REFCNT();
  mContext = aContext;
  mJSObj = aObj;
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

nsresult nsJSEventListener::HandleEvent(nsIDOMEvent* aEvent)
{
  jsval funval;
  jsval argv[1];
  JSObject *eventObj;
  char* eventChars;
  nsAutoString eventString;

  if (NS_OK != aEvent->GetType(eventString)) {
    //JS can't handle this event yet or can't handle it at all
    return NS_OK;
  }

  eventString.Insert("on", 0, 2);

  eventChars = eventString.ToNewCString();

  if (!JS_LookupProperty(mContext, mJSObj, eventChars, &funval)) {
    nsCRT::free(eventChars);
    return NS_ERROR_FAILURE;
  }

  nsCRT::free(eventChars);

  if (JS_TypeOfValue(mContext, funval) != JSTYPE_FUNCTION) {
    return NS_OK;
  }

  nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(mContext);
  if (NS_OK != NS_NewScriptUIEvent(mScriptCX, aEvent, nsnull, (void**)&eventObj)) {
    return NS_ERROR_FAILURE;
  }

  argv[0] = OBJECT_TO_JSVAL(eventObj);
  JSFunction *jsFun = JS_ValueToFunction(mContext, funval);
  PRBool jsBoolResult;
  if (!jsFun || NS_FAILED(mScriptCX->CallFunction(mJSObj, jsFun, 1, argv, 
                                                  &jsBoolResult))) 
  {
    return NS_ERROR_FAILURE;
  }
  if (!jsBoolResult) 
    aEvent->PreventDefault();

  return NS_OK;
}

/*
 * Factory functions
 */

extern "C" NS_DOM nsresult NS_NewJSEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, void *aObj)
{
  JSContext *mCX = (JSContext*)aContext->GetNativeContext();
  
  nsJSEventListener* it = new nsJSEventListener(mCX, (JSObject*)aObj);
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

