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
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSRuleList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSRuleIID, NS_IDOMCSSRULE_IID);
static NS_DEFINE_IID(kICSSRuleListIID, NS_IDOMCSSRULELIST_IID);

//
// CSSRuleList property ids
//
enum CSSRuleList_slots {
  CSSRULELIST_LENGTH = -1
};

/***********************************************************************/
//
// CSSRuleList Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSSRuleListProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSRuleList *a = (nsIDOMCSSRuleList*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case CSSRULELIST_LENGTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSRULELIST_LENGTH, PR_FALSE);
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
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSRULELIST_ITEM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSRule* prop;
          rv = a->Item(JSVAL_TO_INT(id), &prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
      }
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
// CSSRuleList Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSSRuleListProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSRuleList *a = (nsIDOMCSSRuleList*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// CSSRuleList finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSSRuleList(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// CSSRuleList enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSSRuleList(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// CSSRuleList resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSSRuleList(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
CSSRuleListItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSRuleList *nativeThis = (nsIDOMCSSRuleList*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMCSSRule* nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSSRULELIST_ITEM, PR_FALSE);
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


/***********************************************************************/
//
// class for CSSRuleList
//
JSClass CSSRuleListClass = {
  "CSSRuleList", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSSRuleListProperty,
  SetCSSRuleListProperty,
  EnumerateCSSRuleList,
  ResolveCSSRuleList,
  JS_ConvertStub,
  FinalizeCSSRuleList,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// CSSRuleList class properties
//
static JSPropertySpec CSSRuleListProperties[] =
{
  {"length",    CSSRULELIST_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// CSSRuleList class methods
//
static JSFunctionSpec CSSRuleListMethods[] = 
{
  {"item",          CSSRuleListItem,     1},
  {0}
};


//
// CSSRuleList constructor
//
PR_STATIC_CALLBACK(JSBool)
CSSRuleList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSSRuleList class initialization
//
extern "C" NS_DOM nsresult NS_InitCSSRuleListClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSSRuleList", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSSRuleListClass,      // JSClass
                         CSSRuleList,            // JSNative ctor
                         0,             // ctor args
                         CSSRuleListProperties,  // proto props
                         CSSRuleListMethods,     // proto funcs
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
// Method for creating a new CSSRuleList JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSSRuleList(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSSRuleList");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSSRuleList *aCSSRuleList;

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

  if (NS_OK != NS_InitCSSRuleListClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSSRuleListIID, (void **)&aCSSRuleList);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSSRuleListClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSSRuleList);
  }
  else {
    NS_RELEASE(aCSSRuleList);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
