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
#include "nsIDOMAttribute.h"
#include "nsIDOMAttributeList.h"
#include "nsIDOMNodeIterator.h"

const uint32 gGCSize = 4L * 1024L * 1024L;
const size_t gStackSize = 8192;

static NS_DEFINE_IID(kIScriptContextIID, NS_ISCRIPTCONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

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

nsIScriptGlobalObject* nsJSContext::GetGlobalObject()
{
  JSObject *global = JS_GetGlobalObject(mContext);
  nsIScriptGlobalObject *script_global;
  
  if (nsnull != global) {
    script_global = (nsIScriptGlobalObject *)JS_GetPrivate(mContext, global);
    NS_ADDREF(script_global);
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
      NS_OK == NS_InitAttributeClass(this, nsnull) &&
      NS_OK == NS_InitAttributeListClass(this, nsnull) &&
      NS_OK == NS_InitNodeIteratorClass(this, nsnull)) {
    res = NS_OK;
  }

  NS_RELEASE(global);
  return res;
}

nsJSEnvironment *nsJSEnvironment::sTheEnvironment = nsnull;

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


extern "C" NS_DOM NS_CreateContext(nsIScriptGlobalObject *aGlobal, nsIScriptContext **aContext)
{
  nsJSEnvironment *environment = nsJSEnvironment::GetScriptingEnvironment();
  *aContext = environment->GetNewContext();
  (*aContext)->InitContext(aGlobal);
  aGlobal->SetContext(*aContext);
  return NS_OK;
}

