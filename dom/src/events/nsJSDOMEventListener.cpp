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
#include "nsJSDOMEventListener.h"
#include "nsString.h"

static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

/*
 * nsJSDOMEventListener implementation
 */
nsJSDOMEventListener::nsJSDOMEventListener(JSContext *aContext, JSObject *aObj, JSFunction *aFun) 
{
  NS_INIT_REFCNT();
  mContext = aContext;
  mJSObj = aObj;
  mJSFun = aFun;
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
  jsval result;
  jsval argv[1];
  JSObject *eventObj;

  nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(mContext);
  if (NS_OK != NS_NewScriptEvent(mScriptCX, aEvent, nsnull, (void**)&eventObj)) {
    return NS_ERROR_FAILURE;
  }

  argv[0] = OBJECT_TO_JSVAL(eventObj);
  if (PR_TRUE == JS_CallFunction(mContext, mJSObj, mJSFun, 1, argv, &result)) {
    mScriptCX->ScriptEvaluated();
	  if (JSVAL_IS_BOOLEAN(result) && JSVAL_TO_BOOLEAN(result) == JS_FALSE) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }
  mScriptCX->ScriptEvaluated();

  return NS_ERROR_FAILURE;
}

nsresult nsJSDOMEventListener::CheckIfEqual(nsIScriptEventListener *aListener)
{
  return NS_COMFALSE;
}

/*
 * Factory functions
 */

extern "C" NS_DOM nsresult NS_NewScriptEventListener(nsIDOMEventListener ** aInstancePtrResult, nsIScriptContext *aContext, void* aObj, void *aFun)
{
  JSContext *mCX = (JSContext*)aContext->GetNativeContext();
  
  nsJSDOMEventListener* it = new nsJSDOMEventListener(mCX, (JSObject*)aObj, (JSFunction*)aFun);
  if (NULL == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIDOMEventListenerIID, (void **) aInstancePtrResult);   
}

