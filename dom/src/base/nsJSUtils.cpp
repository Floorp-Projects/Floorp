/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This is not a generated file. It contains common utility functions 
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsJSUtils.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "prprf.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"


JSBool
nsJSUtils::GetCallingLocation(JSContext* aContext, const char* *aFilename,
                              PRUint32 *aLineno)
{
  // Get the current filename and line number
  JSStackFrame* frame = nsnull;
  JSScript* script = nsnull;
  do {
    frame = ::JS_FrameIterator(aContext, &frame);

    if (frame) {
      script = ::JS_GetFrameScript(aContext, frame);
    }
  } while ((nsnull != frame) && (nsnull == script));

  if (script) {
    const char* filename = ::JS_GetScriptFilename(aContext, script);

    if (filename) {
      PRUint32 lineno = 0;
      jsbytecode* bytecode = ::JS_GetFramePC(aContext, frame);

      if (bytecode) {
        lineno = ::JS_PCToLineNumber(aContext, script, bytecode);
      }

      *aFilename = filename;
      *aLineno = lineno;
      return JS_TRUE;
    }
  }

  return JS_FALSE;
}

void 
nsJSUtils::ConvertStringToJSVal(const nsString& aProp, JSContext* aContext,
                                jsval* aReturn)
{
  JSString *jsstring =
    ::JS_NewUCStringCopyN(aContext, NS_REINTERPRET_CAST(const jschar*,
                                                        aProp.get()),
                          aProp.Length());

  // set the return value
  *aReturn = STRING_TO_JSVAL(jsstring);
}

PRBool
nsJSUtils::ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                   JSContext* aContext, jsval aValue)
{
  *aSupports = nsnull;
  if (JSVAL_IS_NULL(aValue)) {
    return JS_TRUE;
  }
  else if (JSVAL_IS_OBJECT(aValue)) {
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if (NS_FAILED(rv))
      return JS_FALSE;

    // WrapJS does all the work to recycle an existing wrapper and/or do a QI
    rv = xpc->WrapJS(aContext, JSVAL_TO_OBJECT(aValue), aIID,
                     (void**)aSupports);

    if (NS_FAILED(rv))
      return JS_FALSE;

    return JS_TRUE;
  }

  return JS_FALSE;
}

void 
nsJSUtils::ConvertJSValToString(nsAWritableString& aString,
                                JSContext* aContext, jsval aValue)
{
  JSString *jsstring;
  if ((jsstring = ::JS_ValueToString(aContext, aValue)) != nsnull) {
    aString.Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                       ::JS_GetStringChars(jsstring)),
                   ::JS_GetStringLength(jsstring));
  }
  else {
    aString.Truncate();
  }
}

PRBool
nsJSUtils::ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                jsval aValue)
{
  uint32 temp;
  if (::JS_ValueToECMAUint32(aContext, aValue, &temp)) {
    *aProp = (PRUint32)temp;
  }
  else {
    ::JS_ReportError(aContext, "Parameter must be an integer");
    return JS_FALSE;
  }
  
  return JS_TRUE;
}

nsresult 
nsJSUtils::GetStaticScriptGlobal(JSContext* aContext, JSObject* aObj,
                                 nsIScriptGlobalObject** aNativeGlobal)
{
  nsISupports* supports;
  JSClass* clazz;
  JSObject* parent;
  JSObject* glob = aObj; // starting point for search

  if (!glob)
    return NS_ERROR_FAILURE;

  while ((parent = ::JS_GetParent(aContext, glob)))
    glob = parent;

#ifdef JS_THREADSAFE
  clazz = ::JS_GetClass(aContext, glob);
#else
  clazz = ::JS_GetClass(glob);
#endif

  if (!clazz ||
      !(clazz->flags & JSCLASS_HAS_PRIVATE) ||
      !(clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) ||
      !(supports = (nsISupports*)::JS_GetPrivate(aContext, glob))) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(supports));
  NS_ENSURE_TRUE(wrapper, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupports> native;
  wrapper->GetNative(getter_AddRefs(native));

  return CallQueryInterface(native, aNativeGlobal);
}

nsresult 
nsJSUtils::GetStaticScriptContext(JSContext* aContext, JSObject* aObj,
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

nsresult 
nsJSUtils::GetDynamicScriptGlobal(JSContext* aContext,
                                  nsIScriptGlobalObject** aNativeGlobal)
{
  nsCOMPtr<nsIScriptContext> scriptCX;
  GetDynamicScriptContext(aContext, getter_AddRefs(scriptCX));
  if (!scriptCX)
    return NS_ERROR_FAILURE;
  return scriptCX->GetGlobalObject(aNativeGlobal);
}  

nsresult 
nsJSUtils::GetDynamicScriptContext(JSContext *aContext,
                                   nsIScriptContext** aScriptContext)
{
  nsISupports *supports =
    (::JS_GetOptions(aContext) & JSOPTION_PRIVATE_IS_NSISUPPORTS)
    ? NS_STATIC_CAST(nsISupports*, ::JS_GetContextPrivate(aContext))
    : nsnull;
  if (!supports)
    return nsnull;
  return supports->QueryInterface(NS_GET_IID(nsIScriptContext),
                                  (void**)aScriptContext);
}

