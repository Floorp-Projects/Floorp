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
#include "nsIDOMHTMLTableColElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLTableColElementIID, NS_IDOMHTMLTABLECOLELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLTableColElement);

//
// HTMLTableColElement property ids
//
enum HTMLTableColElement_slots {
  HTMLTABLECOLELEMENT_ALIGN = -1,
  HTMLTABLECOLELEMENT_CH = -2,
  HTMLTABLECOLELEMENT_CHOFF = -3,
  HTMLTABLECOLELEMENT_SPAN = -4,
  HTMLTABLECOLELEMENT_VALIGN = -5,
  HTMLTABLECOLELEMENT_WIDTH = -6
};

/***********************************************************************/
//
// HTMLTableColElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTableColElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableColElement *a = (nsIDOMHTMLTableColElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLECOLELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.align", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLECOLELEMENT_CH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.ch", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCh(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLECOLELEMENT_CHOFF:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.choff", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetChOff(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLECOLELEMENT_SPAN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.span", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetSpan(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLECOLELEMENT_VALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.valign", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLECOLELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.width", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
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
// HTMLTableColElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTableColElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableColElement *a = (nsIDOMHTMLTableColElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLECOLELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.align", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLTABLECOLELEMENT_CH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.ch", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCh(prop);
        
        break;
      }
      case HTMLTABLECOLELEMENT_CHOFF:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.choff", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetChOff(prop);
        
        break;
      }
      case HTMLTABLECOLELEMENT_SPAN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.span", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetSpan(prop);
        
        break;
      }
      case HTMLTABLECOLELEMENT_VALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.valign", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVAlign(prop);
        
        break;
      }
      case HTMLTABLECOLELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablecolelement.width", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWidth(prop);
        
        break;
      }
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
// HTMLTableColElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTableColElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTableColElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTableColElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLTableColElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTableColElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLTableColElement
//
JSClass HTMLTableColElementClass = {
  "HTMLTableColElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTableColElementProperty,
  SetHTMLTableColElementProperty,
  EnumerateHTMLTableColElement,
  ResolveHTMLTableColElement,
  JS_ConvertStub,
  FinalizeHTMLTableColElement
};


//
// HTMLTableColElement class properties
//
static JSPropertySpec HTMLTableColElementProperties[] =
{
  {"align",    HTMLTABLECOLELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"ch",    HTMLTABLECOLELEMENT_CH,    JSPROP_ENUMERATE},
  {"chOff",    HTMLTABLECOLELEMENT_CHOFF,    JSPROP_ENUMERATE},
  {"span",    HTMLTABLECOLELEMENT_SPAN,    JSPROP_ENUMERATE},
  {"vAlign",    HTMLTABLECOLELEMENT_VALIGN,    JSPROP_ENUMERATE},
  {"width",    HTMLTABLECOLELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLTableColElement class methods
//
static JSFunctionSpec HTMLTableColElementMethods[] = 
{
  {0}
};


//
// HTMLTableColElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableColElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTableColElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTableColElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTableColElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitHTMLElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLTableColElementClass,      // JSClass
                         HTMLTableColElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTableColElementProperties,  // proto props
                         HTMLTableColElementMethods,     // proto funcs
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
// Method for creating a new HTMLTableColElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTableColElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTableColElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTableColElement *aHTMLTableColElement;

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

  if (NS_OK != NS_InitHTMLTableColElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTableColElementIID, (void **)&aHTMLTableColElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTableColElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTableColElement);
  }
  else {
    NS_RELEASE(aHTMLTableColElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
