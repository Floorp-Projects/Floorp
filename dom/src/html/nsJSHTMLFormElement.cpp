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
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINSHTMLFormElementIID, NS_IDOMNSHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

NS_DEF_PTR(nsIDOMNSHTMLFormElement);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMHTMLCollection);

//
// HTMLFormElement property ids
//
enum HTMLFormElement_slots {
  HTMLFORMELEMENT_ELEMENTS = -1,
  HTMLFORMELEMENT_LENGTH = -2,
  HTMLFORMELEMENT_NAME = -3,
  HTMLFORMELEMENT_ACCEPTCHARSET = -4,
  HTMLFORMELEMENT_ACTION = -5,
  HTMLFORMELEMENT_ENCTYPE = -6,
  HTMLFORMELEMENT_METHOD = -7,
  HTMLFORMELEMENT_TARGET = -8,
  NSHTMLFORMELEMENT_ENCODING = -9
};

/***********************************************************************/
//
// HTMLFormElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLFormElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLFormElement *a = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case HTMLFORMELEMENT_ELEMENTS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.elements", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (NS_SUCCEEDED(a->GetElements(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_LENGTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.length", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint32 prop;
        if (NS_SUCCEEDED(a->GetLength(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.name", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetName(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_ACCEPTCHARSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.acceptcharset", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAcceptCharset(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_ACTION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.action", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAction(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_ENCTYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.enctype", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetEnctype(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_METHOD:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.method", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMethod(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLFORMELEMENT_TARGET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.target", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTarget(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NSHTMLFORMELEMENT_ENCODING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nshtmlformelement.encoding", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsIDOMNSHTMLFormElement* b;
        if (NS_OK == a->QueryInterface(kINSHTMLFormElementIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetEncoding(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSHTMLFormElement");
          return JS_FALSE;
        }
        break;
      }
      default:
      {
        nsIDOMElement* prop;
        nsIDOMNSHTMLFormElement* b;
        if (NS_OK == a->QueryInterface(kINSHTMLFormElementIID, (void **)&b)) {
          if (NS_OK == b->Item(JSVAL_TO_INT(id), &prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSHTMLFormElement");
          return JS_FALSE;
        }
      }
    }
  }

  if (checkNamedItem) {
    nsIDOMElement* prop;
    nsIDOMNSHTMLFormElement* b;
    nsAutoString name;

    JSString *jsstring = JS_ValueToString(cx, id);
    if (nsnull != jsstring) {
      name.SetString(JS_GetStringChars(jsstring));
    }
    else {
      name.SetString("");
    }

    if (NS_OK == a->QueryInterface(kINSHTMLFormElementIID, (void **)&b)) {
      if (NS_OK == b->NamedItem(name, &prop)) {
        NS_RELEASE(b);
        if (NULL != prop) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
        }
      }
      else {
        NS_RELEASE(b);
        return JS_FALSE;
      }
    }
    else {
      JS_ReportError(cx, "Object must be of type NSHTMLFormElement");
      return JS_FALSE;
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLFormElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLFormElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLFormElement *a = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case HTMLFORMELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLFORMELEMENT_ACCEPTCHARSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.acceptcharset", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAcceptCharset(prop);
        
        break;
      }
      case HTMLFORMELEMENT_ACTION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.action", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAction(prop);
        
        break;
      }
      case HTMLFORMELEMENT_ENCTYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.enctype", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetEnctype(prop);
        
        break;
      }
      case HTMLFORMELEMENT_METHOD:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.method", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMethod(prop);
        
        break;
      }
      case HTMLFORMELEMENT_TARGET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.target", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTarget(prop);
        
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
// HTMLFormElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLFormElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLFormElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLFormElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLFormElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLFormElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Submit
//
PR_STATIC_CALLBACK(JSBool)
HTMLFormElementSubmit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLFormElement *nativeThis = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.submit",PR_FALSE , &ok);
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

    if (NS_OK != nativeThis->Submit()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Reset
//
PR_STATIC_CALLBACK(JSBool)
HTMLFormElementReset(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLFormElement *nativeThis = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlformelement.reset",PR_FALSE , &ok);
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

    if (NS_OK != nativeThis->Reset()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method NamedItem
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLFormElementNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLFormElement *privateThis = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNSHTMLFormElement *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLFormElementIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type NSHTMLFormElement");
    return JS_FALSE;
  }

  nsIDOMElement* nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "nshtmlformelement.nameditem",PR_FALSE , &ok);
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
      JS_ReportError(cx, "Function namedItem requires 1 parameter");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->NamedItem(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLFormElementItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLFormElement *privateThis = (nsIDOMHTMLFormElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNSHTMLFormElement *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLFormElementIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type NSHTMLFormElement");
    return JS_FALSE;
  }

  nsIDOMElement* nativeRet;
  PRUint32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "nshtmlformelement.item",PR_FALSE , &ok);
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
      JS_ReportError(cx, "Function item requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Item(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLFormElement
//
JSClass HTMLFormElementClass = {
  "HTMLFormElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLFormElementProperty,
  SetHTMLFormElementProperty,
  EnumerateHTMLFormElement,
  ResolveHTMLFormElement,
  JS_ConvertStub,
  FinalizeHTMLFormElement
};


//
// HTMLFormElement class properties
//
static JSPropertySpec HTMLFormElementProperties[] =
{
  {"elements",    HTMLFORMELEMENT_ELEMENTS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"length",    HTMLFORMELEMENT_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    HTMLFORMELEMENT_NAME,    JSPROP_ENUMERATE},
  {"acceptCharset",    HTMLFORMELEMENT_ACCEPTCHARSET,    JSPROP_ENUMERATE},
  {"action",    HTMLFORMELEMENT_ACTION,    JSPROP_ENUMERATE},
  {"enctype",    HTMLFORMELEMENT_ENCTYPE,    JSPROP_ENUMERATE},
  {"method",    HTMLFORMELEMENT_METHOD,    JSPROP_ENUMERATE},
  {"target",    HTMLFORMELEMENT_TARGET,    JSPROP_ENUMERATE},
  {"encoding",    NSHTMLFORMELEMENT_ENCODING,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLFormElement class methods
//
static JSFunctionSpec HTMLFormElementMethods[] = 
{
  {"submit",          HTMLFormElementSubmit,     0},
  {"reset",          HTMLFormElementReset,     0},
  {"namedItem",          NSHTMLFormElementNamedItem,     1},
  {"item",          NSHTMLFormElementItem,     1},
  {0}
};


//
// HTMLFormElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLFormElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLFormElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLFormElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLFormElement", &vp)) ||
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
                         &HTMLFormElementClass,      // JSClass
                         HTMLFormElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLFormElementProperties,  // proto props
                         HTMLFormElementMethods,     // proto funcs
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
// Method for creating a new HTMLFormElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLFormElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLFormElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLFormElement *aHTMLFormElement;

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

  if (NS_OK != NS_InitHTMLFormElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLFormElementIID, (void **)&aHTMLFormElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLFormElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLFormElement);
  }
  else {
    NS_RELEASE(aHTMLFormElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
