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
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsIDOMRect.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSPrimitiveValueIID, NS_IDOMCSSPRIMITIVEVALUE_IID);
static NS_DEFINE_IID(kIRectIID, NS_IDOMRECT_IID);

//
// Rect property ids
//
enum Rect_slots {
  RECT_TOP = -1,
  RECT_RIGHT = -2,
  RECT_BOTTOM = -3,
  RECT_LEFT = -4
};

/***********************************************************************/
//
// Rect Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetRectProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRect *a = (nsIDOMRect*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case RECT_TOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RECT_TOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RECT_RIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RECT_RIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetRight(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RECT_BOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RECT_BOTTOM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetBottom(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RECT_LEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RECT_LEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetLeft(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
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
// Rect Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetRectProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRect *a = (nsIDOMRect*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// Rect class properties
//
static JSPropertySpec RectProperties[] =
{
  {"top",    RECT_TOP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"right",    RECT_RIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"bottom",    RECT_BOTTOM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"left",    RECT_LEFT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Rect finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeRect(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Rect enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateRect(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// Rect resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveRect(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for Rect
//
JSClass RectClass = {
  "Rect", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetRectProperty,
  SetRectProperty,
  EnumerateRect,
  ResolveRect,
  JS_ConvertStub,
  FinalizeRect,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// Rect class methods
//
static JSFunctionSpec RectMethods[] = 
{
  {0}
};


//
// Rect constructor
//
PR_STATIC_CALLBACK(JSBool)
Rect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Rect class initialization
//
extern "C" NS_DOM nsresult NS_InitRectClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Rect", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &RectClass,      // JSClass
                         Rect,            // JSNative ctor
                         0,             // ctor args
                         RectProperties,  // proto props
                         RectMethods,     // proto funcs
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
// Method for creating a new Rect JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptRect(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptRect");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMRect *aRect;

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

  if (NS_OK != NS_InitRectClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIRectIID, (void **)&aRect);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &RectClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aRect);
  }
  else {
    NS_RELEASE(aRect);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
