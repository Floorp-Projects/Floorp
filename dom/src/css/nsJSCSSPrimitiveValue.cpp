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
#include "nsIDOMCounter.h"
#include "nsIDOMRect.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSPrimitiveValueIID, NS_IDOMCSSPRIMITIVEVALUE_IID);
static NS_DEFINE_IID(kIRGBColorIID, NS_IDOMRGBCOLOR_IID);
static NS_DEFINE_IID(kICounterIID, NS_IDOMCOUNTER_IID);
static NS_DEFINE_IID(kIRectIID, NS_IDOMRECT_IID);

//
// CSSPrimitiveValue property ids
//
enum CSSPrimitiveValue_slots {
  CSSPRIMITIVEVALUE_PRIMITIVETYPE = -1
};

/***********************************************************************/
//
// CSSPrimitiveValue Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSSPrimitiveValueProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSPrimitiveValue *a = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case CSSPRIMITIVEVALUE_PRIMITIVETYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_PRIMITIVETYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint16 prop;
          rv = a->GetPrimitiveType(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
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
// CSSPrimitiveValue Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSSPrimitiveValueProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSPrimitiveValue *a = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// CSSPrimitiveValue class properties
//
static JSPropertySpec CSSPrimitiveValueProperties[] =
{
  {"primitiveType",    CSSPRIMITIVEVALUE_PRIMITIVETYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// CSSPrimitiveValue finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSSPrimitiveValue(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// CSSPrimitiveValue enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSSPrimitiveValue(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// CSSPrimitiveValue resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSSPrimitiveValue(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method SetFloatValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueSetFloatValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRUint32 b0;
  jsdouble b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_SETFLOATVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToNumber(cx, argv[1], &b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->SetFloatValue(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetFloatValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueGetFloatValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  float nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_GETFLOATVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->GetFloatValue(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx, nativeRet));
  }

  return JS_TRUE;
}


//
// Native method SetStringValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueSetStringValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRUint32 b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_SETSTRINGVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->SetStringValue(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetStringValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueGetStringValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_GETSTRINGVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetStringValue(nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method GetCounterValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueGetCounterValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMCounter* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_GETCOUNTERVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetCounterValue(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method GetRectValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueGetRectValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMRect* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_GETRECTVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetRectValue(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method GetRGBColorValue
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValueGetRGBColorValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSPrimitiveValue *nativeThis = (nsIDOMCSSPrimitiveValue*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMRGBColor* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSPRIMITIVEVALUE_GETRGBCOLORVALUE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetRGBColorValue(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for CSSPrimitiveValue
//
JSClass CSSPrimitiveValueClass = {
  "CSSPrimitiveValue", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSSPrimitiveValueProperty,
  SetCSSPrimitiveValueProperty,
  EnumerateCSSPrimitiveValue,
  ResolveCSSPrimitiveValue,
  JS_ConvertStub,
  FinalizeCSSPrimitiveValue,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// CSSPrimitiveValue class methods
//
static JSFunctionSpec CSSPrimitiveValueMethods[] = 
{
  {"setFloatValue",          CSSPrimitiveValueSetFloatValue,     2},
  {"getFloatValue",          CSSPrimitiveValueGetFloatValue,     1},
  {"setStringValue",          CSSPrimitiveValueSetStringValue,     2},
  {"getStringValue",          CSSPrimitiveValueGetStringValue,     0},
  {"getCounterValue",          CSSPrimitiveValueGetCounterValue,     0},
  {"getRectValue",          CSSPrimitiveValueGetRectValue,     0},
  {"getRGBColorValue",          CSSPrimitiveValueGetRGBColorValue,     0},
  {0}
};


//
// CSSPrimitiveValue constructor
//
PR_STATIC_CALLBACK(JSBool)
CSSPrimitiveValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSSPrimitiveValue class initialization
//
extern "C" NS_DOM nsresult NS_InitCSSPrimitiveValueClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSSPrimitiveValue", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitCSSValueClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSSPrimitiveValueClass,      // JSClass
                         CSSPrimitiveValue,            // JSNative ctor
                         0,             // ctor args
                         CSSPrimitiveValueProperties,  // proto props
                         CSSPrimitiveValueMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "CSSPrimitiveValue", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_UNKNOWN);
      JS_SetProperty(jscontext, constructor, "CSS_UNKNOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_NUMBER);
      JS_SetProperty(jscontext, constructor, "CSS_NUMBER", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_PERCENTAGE);
      JS_SetProperty(jscontext, constructor, "CSS_PERCENTAGE", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_EMS);
      JS_SetProperty(jscontext, constructor, "CSS_EMS", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_EXS);
      JS_SetProperty(jscontext, constructor, "CSS_EXS", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_PX);
      JS_SetProperty(jscontext, constructor, "CSS_PX", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_CM);
      JS_SetProperty(jscontext, constructor, "CSS_CM", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_MM);
      JS_SetProperty(jscontext, constructor, "CSS_MM", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_IN);
      JS_SetProperty(jscontext, constructor, "CSS_IN", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_PT);
      JS_SetProperty(jscontext, constructor, "CSS_PT", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_PC);
      JS_SetProperty(jscontext, constructor, "CSS_PC", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_DEG);
      JS_SetProperty(jscontext, constructor, "CSS_DEG", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_RAD);
      JS_SetProperty(jscontext, constructor, "CSS_RAD", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_GRAD);
      JS_SetProperty(jscontext, constructor, "CSS_GRAD", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_MS);
      JS_SetProperty(jscontext, constructor, "CSS_MS", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_S);
      JS_SetProperty(jscontext, constructor, "CSS_S", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_HZ);
      JS_SetProperty(jscontext, constructor, "CSS_HZ", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_KHZ);
      JS_SetProperty(jscontext, constructor, "CSS_KHZ", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_DIMENSION);
      JS_SetProperty(jscontext, constructor, "CSS_DIMENSION", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_STRING);
      JS_SetProperty(jscontext, constructor, "CSS_STRING", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_URI);
      JS_SetProperty(jscontext, constructor, "CSS_URI", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_IDENT);
      JS_SetProperty(jscontext, constructor, "CSS_IDENT", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_ATTR);
      JS_SetProperty(jscontext, constructor, "CSS_ATTR", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_COUNTER);
      JS_SetProperty(jscontext, constructor, "CSS_COUNTER", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_RECT);
      JS_SetProperty(jscontext, constructor, "CSS_RECT", &vp);

      vp = INT_TO_JSVAL(nsIDOMCSSPrimitiveValue::CSS_RGBCOLOR);
      JS_SetProperty(jscontext, constructor, "CSS_RGBCOLOR", &vp);

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
// Method for creating a new CSSPrimitiveValue JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSSPrimitiveValue(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSSPrimitiveValue");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSSPrimitiveValue *aCSSPrimitiveValue;

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

  if (NS_OK != NS_InitCSSPrimitiveValueClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSSPrimitiveValueIID, (void **)&aCSSPrimitiveValue);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSSPrimitiveValueClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSSPrimitiveValue);
  }
  else {
    NS_RELEASE(aCSSPrimitiveValue);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
