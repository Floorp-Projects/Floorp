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
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMNode.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMCSSStyleRuleCollection.h"
#include "nsIDOMCSSStyleSheet.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kICSSStyleRuleCollectionIID, NS_IDOMCSSSTYLERULECOLLECTION_IID);
static NS_DEFINE_IID(kICSSStyleSheetIID, NS_IDOMCSSSTYLESHEET_IID);

NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMStyleSheet);
NS_DEF_PTR(nsIDOMCSSStyleRuleCollection);
NS_DEF_PTR(nsIDOMCSSStyleSheet);

//
// CSSStyleSheet property ids
//
enum CSSStyleSheet_slots {
  CSSSTYLESHEET_OWNINGNODE = -1,
  CSSSTYLESHEET_PARENTSTYLESHEET = -2,
  CSSSTYLESHEET_HREF = -3,
  CSSSTYLESHEET_TITLE = -4,
  CSSSTYLESHEET_MEDIA = -5,
  CSSSTYLESHEET_CSSRULES = -6
};

/***********************************************************************/
//
// CSSStyleSheet Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSSStyleSheetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleSheet *a = (nsIDOMCSSStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case CSSSTYLESHEET_OWNINGNODE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.owningnode", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMNode* prop;
        if (NS_SUCCEEDED(a->GetOwningNode(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLESHEET_PARENTSTYLESHEET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.parentstylesheet", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMStyleSheet* prop;
        if (NS_SUCCEEDED(a->GetParentStyleSheet(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLESHEET_HREF:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.href", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHref(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLESHEET_TITLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.title", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTitle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLESHEET_MEDIA:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.media", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMedia(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLESHEET_CSSRULES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.cssrules", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMCSSStyleRuleCollection* prop;
        if (NS_SUCCEEDED(a->GetCssRules(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// CSSStyleSheet Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSSStyleSheetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleSheet *a = (nsIDOMCSSStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// CSSStyleSheet finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSSStyleSheet(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// CSSStyleSheet enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSSStyleSheet(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// CSSStyleSheet resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSSStyleSheet(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method InsertRule
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleSheetInsertRule(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleSheet *nativeThis = (nsIDOMCSSStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRUint32 nativeRet;
  nsAutoString b0;
  PRUint32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.insertrule",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function insertRule requires 2 parameters");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->InsertRule(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method DeleteRule
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleSheetDeleteRule(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleSheet *nativeThis = (nsIDOMCSSStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRUint32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "cssstylesheet.deleterule",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function deleteRule requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->DeleteRule(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for CSSStyleSheet
//
JSClass CSSStyleSheetClass = {
  "CSSStyleSheet", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSSStyleSheetProperty,
  SetCSSStyleSheetProperty,
  EnumerateCSSStyleSheet,
  ResolveCSSStyleSheet,
  JS_ConvertStub,
  FinalizeCSSStyleSheet
};


//
// CSSStyleSheet class properties
//
static JSPropertySpec CSSStyleSheetProperties[] =
{
  {"owningNode",    CSSSTYLESHEET_OWNINGNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"parentStyleSheet",    CSSSTYLESHEET_PARENTSTYLESHEET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"href",    CSSSTYLESHEET_HREF,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"title",    CSSSTYLESHEET_TITLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"media",    CSSSTYLESHEET_MEDIA,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cssRules",    CSSSTYLESHEET_CSSRULES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// CSSStyleSheet class methods
//
static JSFunctionSpec CSSStyleSheetMethods[] = 
{
  {"insertRule",          CSSStyleSheetInsertRule,     2},
  {"deleteRule",          CSSStyleSheetDeleteRule,     1},
  {0}
};


//
// CSSStyleSheet constructor
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleSheet(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSSStyleSheet class initialization
//
extern "C" NS_DOM nsresult NS_InitCSSStyleSheetClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSSStyleSheet", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitStyleSheetClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSSStyleSheetClass,      // JSClass
                         CSSStyleSheet,            // JSNative ctor
                         0,             // ctor args
                         CSSStyleSheetProperties,  // proto props
                         CSSStyleSheetMethods,     // proto funcs
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
// Method for creating a new CSSStyleSheet JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSSStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSSStyleSheet");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSSStyleSheet *aCSSStyleSheet;

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

  if (NS_OK != NS_InitCSSStyleSheetClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSSStyleSheetIID, (void **)&aCSSStyleSheet);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSSStyleSheetClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSSStyleSheet);
  }
  else {
    NS_RELEASE(aCSSStyleSheet);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
