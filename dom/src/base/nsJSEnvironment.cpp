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
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"

#if defined(OJI) && !defined(XP_UNIX)
#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"
#endif

const uint32 gGCSize = 4L * 1024L * 1024L;
const size_t gStackSize = 8192;

static NS_DEFINE_IID(kIScriptContextIID, NS_ISCRIPTCONTEXT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIScriptNameSetRegistryIID, NS_ISCRIPTNAMESETREGISTRY_IID);
static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

void PR_CALLBACK
NS_ScriptErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	if (nsnull != report) {
   		printf("JavaScript error: ");

		if(message) {
			printf("%s\n", message);
		}

		if(report->filename) {
			printf("URL: %s ", report->filename);
		}

		if(report->lineno) {
			printf("LineNo: %u", report->lineno);
		}

		printf("\n");

		if(report->linebuf) {
			printf("Line text: '%s', ", report->linebuf);
		}

		if(report->tokenptr) {
			printf("Error text: '%s'", report->tokenptr);
		}

		printf("\n");

	} else if(message) {
		printf("JavaScript error: %s\n", message);
	} else {
		printf("JavaScript error: <unknown>\n");
	}
}

nsJSContext::nsJSContext(JSRuntime *aRuntime)
{
  mRefCnt = 0;
  mContext = JS_NewContext(aRuntime, gStackSize);
  JS_SetContextPrivate(mContext, (void *)this);
  mNameSpaceManager = nsnull;
  mIsInitialized = PR_FALSE;
  mNumEvaluations = 0;
}

nsJSContext::~nsJSContext()
{
  NS_IF_RELEASE(mNameSpaceManager);
  JS_DestroyContext(mContext);
}

NS_IMPL_ISUPPORTS(nsJSContext, kIScriptContextIID);

NS_IMETHODIMP_(PRBool)
nsJSContext::EvaluateString(const nsString& aScript, 
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

  ScriptEvaluated();

  return ret;
}

NS_IMETHODIMP_(nsIScriptGlobalObject*)
nsJSContext::GetGlobalObject()
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

NS_IMETHODIMP_(void*)
nsJSContext::GetNativeContext()
{
  return (void *)mContext;
}


NS_IMETHODIMP
nsJSContext::InitContext(nsIScriptGlobalObject *aGlobalObject)
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

nsresult
nsJSContext::InitializeExternalClasses()
{
  nsresult result = NS_OK;

  nsIScriptNameSetRegistry* registry;
  result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                        kIScriptNameSetRegistryIID,
                                        (nsISupports **)&registry);
  if (NS_OK == result) {
    result = registry->InitializeClasses(this);
    nsServiceManager::ReleaseService(kCScriptNameSetRegistryCID,
                                     registry);
  }

  return result;
}

nsresult
nsJSContext::InitializeLiveConnectClasses()
{
	nsresult result = NS_OK;

#if defined(OJI) && !defined(XP_UNIX)
	nsILiveConnectManager* manager = NULL;
	result = nsServiceManager::GetService(nsIJVMManager::GetCID(),
	                                  nsILiveConnectManager::GetIID(),
	                                  (nsISupports **)&manager);
	if (result == NS_OK) {
		result = manager->InitLiveConnectClasses(mContext, JS_GetGlobalObject(mContext));
		nsServiceManager::ReleaseService(nsIJVMManager::GetCID(), manager);
	}
#endif

	// return all is well until things are stable.
	return NS_OK;
}

NS_IMETHODIMP
nsJSContext::InitClasses()
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
      NS_OK == InitializeExternalClasses() && 
      NS_OK == InitializeLiveConnectClasses() &&
      // XXX Temporarily here. This shouldn't be hardcoded.
      NS_OK == NS_InitHTMLImageElementClass(this, nsnull)) {
    res = NS_OK;
  }

  mIsInitialized = PR_TRUE;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP     
nsJSContext::IsContextInitialized()
{
  if (mIsInitialized) {
    return NS_OK;
  }
  else {
    return NS_COMFALSE;
  }
}

NS_IMETHODIMP
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

NS_IMETHODIMP
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

NS_IMETHODIMP
nsJSContext::GC()
{
  JS_GC(mContext);
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::ScriptEvaluated(void)
{
  mNumEvaluations++;

  if (mNumEvaluations > 20) {
    mNumEvaluations = 0;
    GC();
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsJSContext::GetNameSpaceManager(nsIScriptNameSpaceManager** aInstancePtr)
{
  nsresult result = NS_OK;

  if (nsnull == mNameSpaceManager) {
    result = NS_NewScriptNameSpaceManager(&mNameSpaceManager);
    if (NS_OK == result) {
      nsIScriptNameSetRegistry* registry;
      result = nsServiceManager::GetService(kCScriptNameSetRegistryCID,
                                            kIScriptNameSetRegistryIID,
                                            (nsISupports **)&registry);
      if (NS_OK == result) {
        result = registry->PopulateNameSpace(this);
        nsServiceManager::ReleaseService(kCScriptNameSetRegistryCID,
                                         registry);
      }
    }
  }

  *aInstancePtr = mNameSpaceManager;
  NS_ADDREF(mNameSpaceManager);

  return result;
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

#if defined(OJI) && !defined(XP_UNIX)
  // Initialize LiveConnect.
  nsILiveConnectManager* manager = NULL;
  nsresult result = nsServiceManager::GetService(nsIJVMManager::GetCID(),
                                        nsILiveConnectManager::GetIID(),
                                        (nsISupports **)&manager);

  // Should the JVM manager perhaps define methods for starting up LiveConnect?
  if (result == NS_OK && manager != NULL) {
    PRBool started = PR_FALSE;
    result = manager->StartupLiveConnect(mRuntime, started);
    result = nsServiceManager::ReleaseService(nsIJVMManager::GetCID(), manager);
  }
#endif
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

