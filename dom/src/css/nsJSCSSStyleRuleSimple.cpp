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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMCSSStyleRuleSimple.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleSimpleIID, NS_IDOMCSSSTYLERULESIMPLE_IID);

NS_DEF_PTR(nsIDOMCSSStyleDeclaration);
NS_DEF_PTR(nsIDOMCSSStyleRuleSimple);

//
// CSSStyleRuleSimple property ids
//
enum CSSStyleRuleSimple_slots {
  CSSSTYLERULESIMPLE_SELECTORTEXT = -1,
  CSSSTYLERULESIMPLE_STYLE = -2
};

/***********************************************************************/
//
// CSSStyleRuleSimple Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSSStyleRuleSimpleProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleRuleSimple *a = (nsIDOMCSSStyleRuleSimple*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSSSTYLERULESIMPLE_SELECTORTEXT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSelectorText(prop)) {
          nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLERULESIMPLE_STYLE:
      {
        nsIDOMCSSStyleDeclaration* prop;
        if (NS_OK == a->GetStyle(&prop)) {
          // get the js object
          nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// CSSStyleRuleSimple Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSSStyleRuleSimpleProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleRuleSimple *a = (nsIDOMCSSStyleRuleSimple*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSSSTYLERULESIMPLE_SELECTORTEXT:
      {
        nsAutoString prop;
        nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSelectorText(prop);
        
        break;
      }
      default:
        return nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// CSSStyleRuleSimple finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSSStyleRuleSimple(JSContext *cx, JSObject *obj)
{
  nsGenericFinalize(cx, obj);
}


//
// CSSStyleRuleSimple enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSSStyleRuleSimple(JSContext *cx, JSObject *obj)
{
  return nsGenericEnumerate(cx, obj);
}


//
// CSSStyleRuleSimple resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSSStyleRuleSimple(JSContext *cx, JSObject *obj, jsval id)
{
  return nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for CSSStyleRuleSimple
//
JSClass CSSStyleRuleSimpleClass = {
  "CSSStyleRuleSimple", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSSStyleRuleSimpleProperty,
  SetCSSStyleRuleSimpleProperty,
  EnumerateCSSStyleRuleSimple,
  ResolveCSSStyleRuleSimple,
  JS_ConvertStub,
  FinalizeCSSStyleRuleSimple
};


//
// CSSStyleRuleSimple class properties
//
static JSPropertySpec CSSStyleRuleSimpleProperties[] =
{
  {"selectorText",    CSSSTYLERULESIMPLE_SELECTORTEXT,    JSPROP_ENUMERATE},
  {"style",    CSSSTYLERULESIMPLE_STYLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// CSSStyleRuleSimple class methods
//
static JSFunctionSpec CSSStyleRuleSimpleMethods[] = 
{
  {0}
};


//
// CSSStyleRuleSimple constructor
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleRuleSimple(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSSStyleRuleSimple class initialization
//
nsresult NS_InitCSSStyleRuleSimpleClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSSStyleRuleSimple", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitCSSStyleRuleClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSSStyleRuleSimpleClass,      // JSClass
                         CSSStyleRuleSimple,            // JSNative ctor
                         0,             // ctor args
                         CSSStyleRuleSimpleProperties,  // proto props
                         CSSStyleRuleSimpleMethods,     // proto funcs
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
// Method for creating a new CSSStyleRuleSimple JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSSStyleRuleSimple(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSSStyleRuleSimple");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSSStyleRuleSimple *aCSSStyleRuleSimple;

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

  if (NS_OK != NS_InitCSSStyleRuleSimpleClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSSStyleRuleSimpleIID, (void **)&aCSSStyleRuleSimple);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSSStyleRuleSimpleClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSSStyleRuleSimple);
  }
  else {
    NS_RELEASE(aCSSStyleRuleSimple);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
