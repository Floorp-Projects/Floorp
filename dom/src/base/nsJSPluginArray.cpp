/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIDOMPlugin.h"
#include "nsIDOMPluginArray.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IDOMPLUGIN_IID);
static NS_DEFINE_IID(kIPluginArrayIID, NS_IDOMPLUGINARRAY_IID);

//
// PluginArray property ids
//
enum PluginArray_slots {
  PLUGINARRAY_LENGTH = -1
};

/***********************************************************************/
//
// PluginArray Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetPluginArrayProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMPluginArray *a = (nsIDOMPluginArray*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case PLUGINARRAY_LENGTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PLUGINARRAY_LENGTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetLength(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      default:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PLUGINARRAY_ITEM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMPlugin* prop;
          rv = a->Item(JSVAL_TO_INT(id), &prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
      }
    }
  }

  if (checkNamedItem) {
    nsIDOMPlugin* prop;
    nsAutoString name;
    nsresult result = NS_OK;

    JSString *jsstring = JS_ValueToString(cx, id);
    if (nsnull != jsstring) {
      name.Assign(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsstring)));
    }
    else {
      name.SetLength(0);
    }

    result = a->NamedItem(name, &prop);
    if (NS_SUCCEEDED(result)) {
      if (NULL != prop) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
      }
      else {
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
      }
    }
    else {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}

/***********************************************************************/
//
// PluginArray Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetPluginArrayProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMPluginArray *a = (nsIDOMPluginArray*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}


//
// PluginArray finalizer
//
PR_STATIC_CALLBACK(void)
FinalizePluginArray(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// PluginArray enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumeratePluginArray(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// PluginArray resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolvePluginArray(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
PluginArrayItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMPluginArray *nativeThis = (nsIDOMPluginArray*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMPlugin* nativeRet;
  PRUint32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PLUGINARRAY_ITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->Item(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method NamedItem
//
PR_STATIC_CALLBACK(JSBool)
PluginArrayNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMPluginArray *nativeThis = (nsIDOMPluginArray*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMPlugin* nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PLUGINARRAY_NAMEDITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->NamedItem(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method Refresh
//
PR_STATIC_CALLBACK(JSBool)
PluginArrayRefresh(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMPluginArray *nativeThis = (nsIDOMPluginArray*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRBool b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PLUGINARRAY_REFRESH, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b0, cx, argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->Refresh(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for PluginArray
//
JSClass PluginArrayClass = {
  "PluginArray", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetPluginArrayProperty,
  SetPluginArrayProperty,
  EnumeratePluginArray,
  ResolvePluginArray,
  JS_ConvertStub,
  FinalizePluginArray,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// PluginArray class properties
//
static JSPropertySpec PluginArrayProperties[] =
{
  {"length",    PLUGINARRAY_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// PluginArray class methods
//
static JSFunctionSpec PluginArrayMethods[] = 
{
  {"item",          PluginArrayItem,     1},
  {"namedItem",          PluginArrayNamedItem,     1},
  {"refresh",          PluginArrayRefresh,     1},
  {0}
};


//
// PluginArray constructor
//
PR_STATIC_CALLBACK(JSBool)
PluginArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// PluginArray class initialization
//
extern "C" NS_DOM nsresult NS_InitPluginArrayClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "PluginArray", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &PluginArrayClass,      // JSClass
                         PluginArray,            // JSNative ctor
                         0,             // ctor args
                         PluginArrayProperties,  // proto props
                         PluginArrayMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new PluginArray JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptPluginArray(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptPluginArray");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMPluginArray *aPluginArray;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitPluginArrayClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIPluginArrayIID, (void **)&aPluginArray);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &PluginArrayClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aPluginArray);
  }
  else {
    NS_RELEASE(aPluginArray);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
