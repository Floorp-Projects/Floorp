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
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIScriptSecurityManager.h"

const uint32 gGCSize = 4L * 1024L * 1024L;
const size_t gStackSize = 8192;

static NS_DEFINE_IID(kIScriptContextIID, NS_ISCRIPTCONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);

void PR_CALLBACK
NS_ScriptErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
  if (nsnull != report) {
    printf("JavaScript error: %s\nURL :%s, LineNo :%u\nLine text: '%s', Error text: '%s'\n", message, 
           report->filename, report->lineno, report->linebuf, report->tokenptr);
  }
  else {
    printf("JavaScript error: %s\n", message);
  }
}

nsJSContext::nsJSContext(JSRuntime *aRuntime)
{
  mRefCnt = 0;
  mContext = JS_NewContext(aRuntime, gStackSize);
  JS_SetContextPrivate(mContext, (void *)this);
}

nsJSContext::~nsJSContext()
{
  JS_DestroyContext(mContext);
}

NS_IMPL_ISUPPORTS(nsJSContext, kIScriptContextIID);

PRBool nsJSContext::EvaluateString(const nsString& aScript, 
                                   const char *aURL,
                                   PRUint32 aLineNo,
                                   nsString& aRetValue,
                                   PRBool* aIsUndefined)
{
  jsval val;

  PRBool ret = ::JS_EvaluateUCScriptForPrincipals(mContext, 
                                                  JS_GetGlobalObject(mContext),
                                                  nsnull,
                                                  (jschar*)aScript.GetUnicode(), 
                                                  aScript.Length(),
                                                  aURL, 
                                                  aLineNo,
                                                  &val);
  
  if (ret) {
    *aIsUndefined = JSVAL_IS_VOID(val);
    JSString* jsstring = JS_ValueToString(mContext, val);   
    aRetValue.SetString(JS_GetStringChars(jsstring));
  }
  else {
    aRetValue.Truncate();
  }

  return ret;
}

nsIScriptGlobalObject* nsJSContext::GetGlobalObject()
{
  JSObject *global = JS_GetGlobalObject(mContext);
  nsIScriptGlobalObject *script_global = nsnull;
  
  if (nsnull != global) {
    nsISupports* sup = (nsISupports *)JS_GetPrivate(mContext, global);
    if (nsnull != sup) {
      sup->QueryInterface(kIScriptGlobalObjectIID, (void**) &script_global);
    }
    return script_global;
  }
  else {
    return nsnull;
  }
}

void* nsJSContext::GetNativeContext()
{
  return (void *)mContext;
}


nsresult nsJSContext::InitContext(nsIScriptGlobalObject *aGlobalObject)
{
  nsresult result = NS_ERROR_FAILURE;
  nsIScriptObjectOwner *owner;
  JSObject *global;
  nsresult res = aGlobalObject->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);

  if (NS_OK == res) {
    res = owner->GetScriptObject(this, (void **)&global);

    // init standard classes
    if ((NS_OK == res) && ::JS_InitStandardClasses(mContext, global)) {
      JS_SetGlobalObject(mContext, global);
      res = InitClasses(); // this will complete the global object initialization
    }

    NS_RELEASE(owner);
  
    if (NS_OK == res) {
      ::JS_SetErrorReporter(mContext, NS_ScriptErrorReporter); 
    }
  }

  return res;
}

nsresult nsJSContext::InitClasses()
{
  nsresult res = NS_ERROR_FAILURE;
  nsIScriptGlobalObject *global = GetGlobalObject();

  if (NS_OK == NS_InitWindowClass(this, global) &&
      NS_OK == NS_InitNodeClass(this, nsnull) &&
      NS_OK == NS_InitElementClass(this, nsnull) &&
      NS_OK == NS_InitDocumentClass(this, nsnull) &&
      NS_OK == NS_InitTextClass(this, nsnull) &&
      NS_OK == NS_InitAttrClass(this, nsnull) &&
      NS_OK == NS_InitNamedNodeMapClass(this, nsnull) &&
      NS_OK == NS_InitNodeListClass(this, nsnull) &&
      // XXX Temporarily here. This shouldn't be hardcoded.
      NS_OK == NS_InitHTMLImageElementClass(this, nsnull)) {
    res = NS_OK;
  }

  NS_RELEASE(global);
  return res;
}

nsresult 
nsJSContext::AddNamedReference(void *aSlot, 
                               void *aScriptObject,
                               const char *aName)
{
  if (::JS_AddNamedRoot(mContext, aSlot, aName)) {
    return NS_OK;
  }
  else {
    return NS_ERROR_FAILURE;
  }
}

nsresult 
nsJSContext::RemoveReference(void *aSlot, void *aScriptObject)
{
  JSObject *obj = (JSObject *)aScriptObject;

  if (::JS_RemoveRoot(mContext, aSlot)) {
    return NS_OK;
  }
  else {
    return NS_ERROR_FAILURE;
  }
}

nsresult
nsJSContext::GC()
{
  JS_GC(mContext);
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::GetSecurityManager(nsIScriptSecurityManager** aInstancePtr)
{
  return NS_NewScriptSecurityManager(aInstancePtr);
}

nsJSEnvironment *nsJSEnvironment::sTheEnvironment = nsnull;

// Class to manage destruction of the singleton
// JSEnvironment
struct JSEnvironmentInit {
  JSEnvironmentInit() {
  }

  ~JSEnvironmentInit() {
    if (nsJSEnvironment::sTheEnvironment) {
      delete nsJSEnvironment::sTheEnvironment;
      nsJSEnvironment::sTheEnvironment = nsnull;
    }
  }
};

#ifndef XP_MAC
static JSEnvironmentInit initJSEnvironment;
#endif

nsJSEnvironment *
nsJSEnvironment::GetScriptingEnvironment()
{
  if (nsnull == sTheEnvironment) {
    sTheEnvironment = new nsJSEnvironment();
  }
  return sTheEnvironment;
}

nsJSEnvironment::nsJSEnvironment()
{
  mRuntime = JS_Init(gGCSize);
}

nsJSEnvironment::~nsJSEnvironment()
{
  JS_Finish(mRuntime);
}

nsIScriptContext* nsJSEnvironment::GetNewContext()
{
  nsIScriptContext *context;
  context = new nsJSContext(mRuntime);
  NS_ADDREF(context);
  return context;
}

extern "C" NS_DOM nsresult NS_CreateContext(nsIScriptGlobalObject *aGlobal, nsIScriptContext **aContext)
{
  nsJSEnvironment *environment = nsJSEnvironment::GetScriptingEnvironment();
  *aContext = environment->GetNewContext();
  (*aContext)->InitContext(aGlobal);
  aGlobal->SetContext(*aContext);
  return NS_OK;
}

