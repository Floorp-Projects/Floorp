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
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMAttributeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeIterator.h"
#include "nsIDOMAttribute.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIAttributeListIID, NS_IDOMATTRIBUTELIST_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kINodeIteratorIID, NS_IDOMNODEITERATOR_IID);
static NS_DEFINE_IID(kIAttributeIID, NS_IDOMATTRIBUTE_IID);

NS_DEF_PTR(nsIDOMAttributeList);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMNodeIterator);
NS_DEF_PTR(nsIDOMAttribute);


/***********************************************************************/
//
// Element Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMElement *a = (nsIDOMElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
      {
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->GetProperty(cx, id, vp);
          NS_RELEASE(object);
          return rval;
        }
      }
    }
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Element Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMElement *a = (nsIDOMElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
      {
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->SetProperty(cx, id, vp);
          NS_RELEASE(object);
          return rval;
        }
      }
    }
  }

  return PR_TRUE;
}


//
// Element finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeElement(JSContext *cx, JSObject *obj)
{
  nsIDOMElement *a = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == a->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    NS_RELEASE(a);
  }
}


//
// Element enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateElement(JSContext *cx, JSObject *obj)
{
  nsIDOMElement *a = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->EnumerateProperty(cx);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


//
// Element resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveElement(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMElement *a = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->Resolve(cx, id);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


//
// Native method GetTagName
//
PR_STATIC_CALLBACK(JSBool)
ElementGetTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetTagName(nativeRet)) {
      return JS_FALSE;
    }

    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetAttributes
//
PR_STATIC_CALLBACK(JSBool)
ElementGetAttributes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMAttributeList* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->GetAttributes(&nativeRet)) {
      return JS_FALSE;
    }

    if (nativeRet != nsnull) {
      nsIScriptObjectOwner *owner = nsnull;
      if (NS_OK == nativeRet->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
        JSObject *object = nsnull;
        nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
        if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
          // set the return value
          *rval = OBJECT_TO_JSVAL(object);
        }
        NS_RELEASE(owner);
      }
      NS_RELEASE(nativeRet);
    }
    else {
      *rval = JSVAL_NULL;
    }
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetDOMAttribute
//
PR_STATIC_CALLBACK(JSBool)
ElementGetDOMAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->GetDOMAttribute(b0, nativeRet)) {
      return JS_FALSE;
    }

    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetDOMAttribute
//
PR_STATIC_CALLBACK(JSBool)
ElementSetDOMAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    JSString *jsstring1 = JS_ValueToString(cx, argv[1]);
    if (nsnull != jsstring1) {
      b1.SetString(JS_GetStringChars(jsstring1));
    }
    else {
      b1.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->SetDOMAttribute(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveAttribute
//
PR_STATIC_CALLBACK(JSBool)
ElementRemoveAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->RemoveAttribute(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetAttributeNode
//
PR_STATIC_CALLBACK(JSBool)
ElementGetAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMAttribute* nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->GetAttributeNode(b0, &nativeRet)) {
      return JS_FALSE;
    }

    if (nativeRet != nsnull) {
      nsIScriptObjectOwner *owner = nsnull;
      if (NS_OK == nativeRet->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
        JSObject *object = nsnull;
        nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
        if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
          // set the return value
          *rval = OBJECT_TO_JSVAL(object);
        }
        NS_RELEASE(owner);
      }
      NS_RELEASE(nativeRet);
    }
    else {
      *rval = JSVAL_NULL;
    }
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetAttributeNode
//
PR_STATIC_CALLBACK(JSBool)
ElementSetAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMAttributePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JSVAL_IS_OBJECT(argv[0])) {
      nsISupports *supports0 = (nsISupports *)JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
      NS_ASSERTION(nsnull != supports0, "null pointer");

      if ((nsnull == supports0) ||
          (NS_OK != supports0->QueryInterface(kIAttributeIID, (void **)(b0.Query())))) {
        return JS_FALSE;
      }
    }
    else if (JSVAL_IS_NULL(argv[0])){
      b0 = nsnull;
    }
    else {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetAttributeNode(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveAttributeNode
//
PR_STATIC_CALLBACK(JSBool)
ElementRemoveAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMAttributePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JSVAL_IS_OBJECT(argv[0])) {
      nsISupports *supports0 = (nsISupports *)JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[0]));
      NS_ASSERTION(nsnull != supports0, "null pointer");

      if ((nsnull == supports0) ||
          (NS_OK != supports0->QueryInterface(kIAttributeIID, (void **)(b0.Query())))) {
        return JS_FALSE;
      }
    }
    else if (JSVAL_IS_NULL(argv[0])){
      b0 = nsnull;
    }
    else {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveAttributeNode(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetElementsByTagName
//
PR_STATIC_CALLBACK(JSBool)
ElementGetElementsByTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodeIterator* nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->GetElementsByTagName(b0, &nativeRet)) {
      return JS_FALSE;
    }

    if (nativeRet != nsnull) {
      nsIScriptObjectOwner *owner = nsnull;
      if (NS_OK == nativeRet->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
        JSObject *object = nsnull;
        nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
        if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
          // set the return value
          *rval = OBJECT_TO_JSVAL(object);
        }
        NS_RELEASE(owner);
      }
      NS_RELEASE(nativeRet);
    }
    else {
      *rval = JSVAL_NULL;
    }
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Normalize
//
PR_STATIC_CALLBACK(JSBool)
ElementNormalize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *nativeThis = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Normalize()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Element
//
JSClass ElementClass = {
  "Element", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetElementProperty,
  SetElementProperty,
  EnumerateElement,
  ResolveElement,
  JS_ConvertStub,
  FinalizeElement
};


//
// Element class properties
//
static JSPropertySpec ElementProperties[] =
{
  {0}
};


//
// Element class methods
//
static JSFunctionSpec ElementMethods[] = 
{
  {"getTagName",          ElementGetTagName,     0},
  {"getAttributes",          ElementGetAttributes,     0},
  {"getDOMAttribute",          ElementGetDOMAttribute,     1},
  {"setDOMAttribute",          ElementSetDOMAttribute,     2},
  {"removeAttribute",          ElementRemoveAttribute,     1},
  {"getAttributeNode",          ElementGetAttributeNode,     1},
  {"setAttributeNode",          ElementSetAttributeNode,     1},
  {"removeAttributeNode",          ElementRemoveAttributeNode,     1},
  {"getElementsByTagName",          ElementGetElementsByTagName,     1},
  {"normalize",          ElementNormalize,     0},
  {0}
};


//
// Element constructor
//
PR_STATIC_CALLBACK(JSBool)
Element(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}


//
// Element class initialization
//
nsresult NS_InitElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Element", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitNodeClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &ElementClass,      // JSClass
                         Element,            // JSNative ctor
                         0,             // ctor args
                         ElementProperties,  // proto props
                         ElementMethods,     // proto funcs
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
// Method for creating a new Element JavaScript object
//
extern "C" NS_DOM NS_NewScriptElement(nsIScriptContext *aContext, nsIDOMElement *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();

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

  if (NS_OK != NS_InitElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSupports);
    NS_ADDREF(aSupports);
  }
  else {
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
