/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* This file is a small subset of nsJSUtils utility functions,
   from the dom directory.
*/

#include "nsCOMPtr.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsWWJSUtils.h"
#include "nsIXPConnect.h"

NS_EXPORT nsresult 
nsWWJSUtils::nsGetStaticScriptGlobal(JSContext* aContext,
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
 
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(supports));
  NS_ENSURE_TRUE(wrapper, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

  return CallQueryInterface(native, aNativeGlobal);
}

NS_EXPORT nsresult 
nsWWJSUtils::nsGetDynamicScriptContext(JSContext *aContext,
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

NS_EXPORT nsresult 
nsWWJSUtils::nsGetStaticScriptContext(JSContext* aContext,
                                      JSObject* aObj,
                                      nsIScriptContext** aScriptContext)
{
  nsCOMPtr<nsIScriptGlobalObject> nativeGlobal;
  nsGetStaticScriptGlobal(aContext, aObj, getter_AddRefs(nativeGlobal));
  if (!nativeGlobal)    
    return NS_ERROR_FAILURE;
  nsIScriptContext* scriptContext = nsnull;
  nativeGlobal->GetContext(&scriptContext);
  *aScriptContext = scriptContext;
  return scriptContext ? NS_OK : NS_ERROR_FAILURE;
}  

