/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* A namespace class for static layout utilities. */

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsLayoutUtils.h"

// static 
nsresult 
nsLayoutUtils::GetStaticScriptGlobal(JSContext* aContext,
                                     JSObject* aObj,
                                     nsIScriptGlobalObject** aNativeGlobal)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return NS_ERROR_FAILURE;

  while (nsnull != (parent = JS_GetParent(aContext, glob)))
    glob = parent;

#ifdef JS_THREADSAFE
  clazz = JS_GetClass(aContext, glob);
#else
  clazz = JS_GetClass(glob);
#endif

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*) JS_GetPrivate(aContext, glob))) {
    return NS_ERROR_FAILURE;
  }

  return supports->QueryInterface(NS_GET_IID(nsIScriptGlobalObject),
                                  (void**) aNativeGlobal);
}

//static
nsresult 
nsLayoutUtils::GetStaticScriptContext(JSContext* aContext,
                                      JSObject* aObj,
                                      nsIScriptContext** aScriptContext)
{
  nsCOMPtr<nsIScriptGlobalObject> nativeGlobal;
  GetStaticScriptGlobal(aContext, aObj, getter_AddRefs(nativeGlobal));
  if (!nativeGlobal)    
    return NS_ERROR_FAILURE;
  nsIScriptContext* scriptContext = nsnull;
  nativeGlobal->GetContext(&scriptContext);
  *aScriptContext = scriptContext;
  return scriptContext ? NS_OK : NS_ERROR_FAILURE;
}  

//static
nsresult 
nsLayoutUtils::GetDynamicScriptGlobal(JSContext* aContext,
                                      nsIScriptGlobalObject** aNativeGlobal)
{
  nsIScriptGlobalObject* nativeGlobal = nsnull;
  nsCOMPtr<nsIScriptContext> scriptCX;
  GetDynamicScriptContext(aContext, getter_AddRefs(scriptCX));
  if (scriptCX) {
    *aNativeGlobal = nativeGlobal = scriptCX->GetGlobalObject();
  }
  return nativeGlobal ? NS_OK : NS_ERROR_FAILURE;
}  

//static
nsresult 
nsLayoutUtils::GetDynamicScriptContext(JSContext *aContext,
                                       nsIScriptContext** aScriptContext)
{
  // XXX We rely on the rule that if any JSContext in our JSRuntime has a 
  // private set then that private *must* be a pointer to an nsISupports.
  nsISupports *supports = (nsIScriptContext*) JS_GetContextPrivate(aContext);
  if (!supports)
      return nsnull;
  return supports->QueryInterface(NS_GET_IID(nsIScriptContext),
                                  (void**)aScriptContext);
}


