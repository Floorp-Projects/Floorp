/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsJSEnvironment.h"
#include "nsIScriptObjectOwner.h"

const uint32 gGCSize = 4L * 1024L * 1024L;
const size_t gStackSize = 8192;

static NS_DEFINE_IID(kIScriptContextIID, NS_ISCRIPTCONTEXT_IID);

nsJSContext::nsJSContext(JSRuntime *aRuntime)
{
  mRefCnt = 1;
  mContext = JS_NewContext(aRuntime, gStackSize);
}

nsJSContext::~nsJSContext()
{
  JS_DestroyContext(mContext);
}

NS_IMPL_ISUPPORTS(nsJSContext, kIScriptContextIID);

PRBool nsJSContext::EvaluateString(const char *aScript, 
                                  PRUint32 aScriptSize, 
                                  jsval *aRetValue)
{
  return ::JS_EvaluateScript(mContext, 
                              JS_GetGlobalObject(mContext),
                              aScript, 
                              aScriptSize,
                              NULL, 
                              0,
                              aRetValue);
}

JSObject* nsJSContext::GetGlobalObject()
{
  return JS_GetGlobalObject(mContext);
}

JSContext* nsJSContext::GetContext()
{
  return mContext;
}

#define GLOBAL_OBJECT_NAME "window_object"
nsresult NS_NewGlobalWindow(JSContext *aContext, nsIWebWidget *aWindow, void **aJSObject);

nsresult nsJSContext::InitContext(nsIWebWidget *aGlobalObject)
{
  nsresult result = NS_ERROR_FAILURE;

  JSObject *global;
  nsresult res = NS_NewGlobalWindow(mContext, aGlobalObject, (void**)&global);
  if (NS_OK == res) {
    // init standard classes
    if (::JS_InitStandardClasses(mContext, global)) {
      res = InitAllClasses(); // this will complete the global object initialization
    }
  }

  return res;
}

nsresult NS_InitWindowClass(JSContext *aContext, JSObject *aGlobal);
nsresult NS_InitNodeClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitElementClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitDocumentClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitTextClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitAttributeClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitAttributeListClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitNodeIteratorClass(JSContext *aContext, JSObject **aPrototype);

nsresult nsJSContext::InitAllClasses()
{
  if (NS_OK == NS_InitWindowClass(mContext, GetGlobalObject()) &&
      NS_OK == NS_InitNodeClass(mContext, nsnull) &&
      NS_OK == NS_InitElementClass(mContext, nsnull) &&
      NS_OK == NS_InitDocumentClass(mContext, nsnull) &&
      NS_OK == NS_InitTextClass(mContext, nsnull) &&
      NS_OK == NS_InitAttributeClass(mContext, nsnull) &&
      NS_OK == NS_InitAttributeListClass(mContext, nsnull) &&
      NS_OK == NS_InitNodeIteratorClass(mContext, nsnull))
    return NS_OK;

  return NS_ERROR_FAILURE;
}

nsJSEnvironment::nsJSEnvironment()
{
  mRuntime = JS_Init(gGCSize);
  mScriptContext = new nsJSContext(mRuntime);
}

nsJSEnvironment::~nsJSEnvironment()
{
  NS_RELEASE(mScriptContext);
  JS_Finish(mRuntime);
}

nsIScriptContext* nsJSEnvironment::GetContext()
{
  return mScriptContext;
}

extern "C" NS_DOM NS_CreateContext(nsIWebWidget *aGlobal, nsIScriptContext **aContext)
{
  nsJSEnvironment *environment = new nsJSEnvironment();
  *aContext = environment->GetContext();
  (*aContext)->InitContext(aGlobal);
  return NS_OK;
}

