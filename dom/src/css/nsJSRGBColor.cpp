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
#include "nsIDOMRGBColor.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSPrimitiveValueIID, NS_IDOMCSSPRIMITIVEVALUE_IID);
static NS_DEFINE_IID(kIRGBColorIID, NS_IDOMRGBCOLOR_IID);

//
// RGBColor property ids
//
enum RGBColor_slots {
  RGBCOLOR_RED = -1,
  RGBCOLOR_GREEN = -2,
  RGBCOLOR_BLUE = -3
};

/***********************************************************************/
//
// RGBColor Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetRGBColorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRGBColor *a = (nsIDOMRGBColor*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case RGBCOLOR_RED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RGBCOLOR_RED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetRed(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RGBCOLOR_GREEN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RGBCOLOR_GREEN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetGreen(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RGBCOLOR_BLUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RGBCOLOR_BLUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSPrimitiveValue* prop;
          rv = a->GetBlue(&prop);
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
// RGBColor Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetRGBColorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRGBColor *a = (nsIDOMRGBColor*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// RGBColor class properties
//
static JSPropertySpec RGBColorProperties[] =
{
  {"red",    RGBCOLOR_RED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"green",    RGBCOLOR_GREEN,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"blue",    RGBCOLOR_BLUE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// RGBColor finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeRGBColor(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// RGBColor enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateRGBColor(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// RGBColor resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveRGBColor(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for RGBColor
//
JSClass RGBColorClass = {
  "RGBColor", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetRGBColorProperty,
  SetRGBColorProperty,
  EnumerateRGBColor,
  ResolveRGBColor,
  JS_ConvertStub,
  FinalizeRGBColor,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// RGBColor class methods
//
static JSFunctionSpec RGBColorMethods[] = 
{
  {0}
};


//
// RGBColor constructor
//
PR_STATIC_CALLBACK(JSBool)
RGBColor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// RGBColor class initialization
//
extern "C" NS_DOM nsresult NS_InitRGBColorClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "RGBColor", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &RGBColorClass,      // JSClass
                         RGBColor,            // JSNative ctor
                         0,             // ctor args
                         RGBColorProperties,  // proto props
                         RGBColorMethods,     // proto funcs
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
// Method for creating a new RGBColor JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptRGBColor(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptRGBColor");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMRGBColor *aRGBColor;

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

  if (NS_OK != NS_InitRGBColorClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIRGBColorIID, (void **)&aRGBColor);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &RGBColorClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aRGBColor);
  }
  else {
    NS_RELEASE(aRGBColor);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
