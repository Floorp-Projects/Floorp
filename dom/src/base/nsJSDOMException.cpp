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
#include "nsIDOMDOMException.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDOMExceptionIID, NS_IDOMDOMEXCEPTION_IID);

//
// DOMException property ids
//
enum DOMException_slots {
  DOMEXCEPTION_CODE = -1,
  DOMEXCEPTION_RESULT = -2,
  DOMEXCEPTION_MESSAGE = -3,
  DOMEXCEPTION_NAME = -4
};

/***********************************************************************/
//
// DOMException Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetDOMExceptionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDOMException *a = (nsIDOMDOMException*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case DOMEXCEPTION_CODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOMEXCEPTION_CODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetCode(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case DOMEXCEPTION_RESULT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOMEXCEPTION_RESULT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetResult(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case DOMEXCEPTION_MESSAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOMEXCEPTION_MESSAGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMessage(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case DOMEXCEPTION_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOMEXCEPTION_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
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
// DOMException Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetDOMExceptionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDOMException *a = (nsIDOMDOMException*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
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
// DOMException class properties
//
static JSPropertySpec DOMExceptionProperties[] =
{
  {"code",    DOMEXCEPTION_CODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"result",    DOMEXCEPTION_RESULT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"message",    DOMEXCEPTION_MESSAGE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    DOMEXCEPTION_NAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// DOMException finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeDOMException(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// DOMException enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateDOMException(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// DOMException resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveDOMException(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
DOMExceptionToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMDOMException *nativeThis = (nsIDOMDOMException*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOMEXCEPTION_TOSTRING, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->ToString(nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for DOMException
//
JSClass DOMExceptionClass = {
  "DOMException", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetDOMExceptionProperty,
  SetDOMExceptionProperty,
  EnumerateDOMException,
  ResolveDOMException,
  JS_ConvertStub,
  FinalizeDOMException,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// DOMException class methods
//
static JSFunctionSpec DOMExceptionMethods[] = 
{
  {"toString",          DOMExceptionToString,     0},
  {0}
};


//
// DOMException constructor
//
PR_STATIC_CALLBACK(JSBool)
DOMException(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// DOMException class initialization
//
extern "C" NS_DOM nsresult NS_InitDOMExceptionClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "DOMException", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &DOMExceptionClass,      // JSClass
                         DOMException,            // JSNative ctor
                         0,             // ctor args
                         DOMExceptionProperties,  // proto props
                         DOMExceptionMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "DOMException", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMDOMException::INDEX_SIZE_ERR);
      JS_SetProperty(jscontext, constructor, "INDEX_SIZE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::DOMSTRING_SIZE_ERR);
      JS_SetProperty(jscontext, constructor, "DOMSTRING_SIZE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::HIERARCHY_REQUEST_ERR);
      JS_SetProperty(jscontext, constructor, "HIERARCHY_REQUEST_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::WRONG_DOCUMENT_ERR);
      JS_SetProperty(jscontext, constructor, "WRONG_DOCUMENT_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::INVALID_CHARACTER_ERR);
      JS_SetProperty(jscontext, constructor, "INVALID_CHARACTER_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::NO_DATA_ALLOWED_ERR);
      JS_SetProperty(jscontext, constructor, "NO_DATA_ALLOWED_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::NO_MODIFICATION_ALLOWED_ERR);
      JS_SetProperty(jscontext, constructor, "NO_MODIFICATION_ALLOWED_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::NOT_FOUND_ERR);
      JS_SetProperty(jscontext, constructor, "NOT_FOUND_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::NOT_SUPPORTED_ERR);
      JS_SetProperty(jscontext, constructor, "NOT_SUPPORTED_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::INUSE_ATTRIBUTE_ERR);
      JS_SetProperty(jscontext, constructor, "INUSE_ATTRIBUTE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::INVALID_STATE_ERR);
      JS_SetProperty(jscontext, constructor, "INVALID_STATE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::SYNTAX_ERR);
      JS_SetProperty(jscontext, constructor, "SYNTAX_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::INVALID_MODIFICATION_ERR);
      JS_SetProperty(jscontext, constructor, "INVALID_MODIFICATION_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::NAMESPACE_ERR);
      JS_SetProperty(jscontext, constructor, "NAMESPACE_ERR", &vp);

      vp = INT_TO_JSVAL(nsIDOMDOMException::INVALID_ACCESS_ERR);
      JS_SetProperty(jscontext, constructor, "INVALID_ACCESS_ERR", &vp);

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
// Method for creating a new DOMException JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptDOMException(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptDOMException");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMDOMException *aDOMException;

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

  if (NS_OK != NS_InitDOMExceptionClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIDOMExceptionIID, (void **)&aDOMException);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &DOMExceptionClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aDOMException);
  }
  else {
    NS_RELEASE(aDOMException);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
