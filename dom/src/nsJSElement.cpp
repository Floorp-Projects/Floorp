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

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMElement.h"
#include "nsIDOMAttribute.h"
#include "nsIDOMIterators.h"
#include "nsString.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMAttributeIID, NS_IDOMATTRIBUTE_IID);

/***********************************************************************/

//
// Element properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  // NS_ASSERTION(nsnull != element, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == element->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->GetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// Element properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object = nsnull;
      if (NS_OK == element->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->SetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// Element finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeElement(JSContext *cx, JSObject *obj)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != element) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == element->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    element->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
GetTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  *rval = JSVAL_NULL;

  // call the function
  nsAutoString string;
  if (NS_OK == element->GetTagName(string)) {
    JSString *jsstring = JS_NewUCStringCopyN(cx, string, string.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetAttributes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  *rval = JSVAL_NULL;

  // call the function
  nsIDOMAttributeList *attributeList = nsnull;
  if (NS_OK == element->GetAttributes(&attributeList)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == attributeList->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(attributeList);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      nsAutoString string;
      if (NS_OK == element->GetDOMAttribute(string1, string)) {
        JSString *jsstring = JS_NewUCStringCopyN(cx, string, string.Length());
        // set the return value
        *rval = STRING_TO_JSVAL(jsstring);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
SetAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(2 == argc, "wrong number of arguments");
  if (2 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0]) &&
        JSVAL_IS_STRING(argv[1])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);
      JSString *jsstring2 = JSVAL_TO_STRING(argv[1]);
      nsAutoString string2 = JS_GetStringChars(jsstring2);

      // call the function
      element->SetDOMAttribute(string1, string2);
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
RemoveAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      element->RemoveAttribute(string1);
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      nsIDOMAttribute *attribute = nsnull;
      if (NS_OK == element->GetAttributeNode(string1, &attribute)) {

        // get the js object
        nsIScriptObjectOwner *owner = nsnull;
        if (NS_OK == attribute->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
          JSObject *object = nsnull;
          if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
            // set the return value
            *rval = OBJECT_TO_JSVAL(object);
          }
          NS_RELEASE(owner);
        }
        NS_RELEASE(attribute);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
SetAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");

      nsIDOMAttribute *attribute1 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMAttributeIID, (void**)&attribute1)) {
        element->SetAttributeNode(attribute1);
        NS_RELEASE(attribute1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
RemoveAttributeNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");

      nsIDOMAttribute *attribute1 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMAttributeIID, (void**)&attribute1)) {
        element->RemoveAttributeNode(attribute1);
        NS_RELEASE(attribute1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetElementsByTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      nsIDOMNodeIterator *nodeIterator = nsnull;
      if (NS_OK == element->GetElementsByTagName(string1, &nodeIterator)) {

        // get the js object
        nsIScriptObjectOwner *owner = nsnull;
        if (NS_OK == nodeIterator->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
          JSObject *object = nsnull;
          if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
            // set the return value
            *rval = OBJECT_TO_JSVAL(object);
          }
          NS_RELEASE(owner);
        }
        NS_RELEASE(nodeIterator);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Normalize(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMElement *element = (nsIDOMElement*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != element, "null pointer");

  // call the function
  element->Normalize();

  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Element
//
JSClass element = {
  "Element", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetElementProperty,     
  SetElementProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeElement
};

//
// Element property ids
//

//
// Element class properties
//

//
// Element class methods
//
static JSFunctionSpec elementMethods[] = {
  {"getTagName",              GetTagName,             0},
  {"getAttributes",           GetAttributes,          0},
  {"getAttribute",            GetAttribute,           1},
  {"setAttribute",            SetAttribute,           2},
  {"removeAttribute",         RemoveAttribute,        1},
  {"getAttributeNode",        GetAttributeNode,       1},
  {"setAttributeNode",        SetAttributeNode,       1},
  {"removeAttributeNode",     RemoveAttributeNode,    1},
  {"getElementsByTagName",    GetElementsByTagName,   1},
  {"normalize",               Normalize,              0},
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
// Element static property ids
//

//
// Element static properties
//

//
// Element static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitNodeClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitElementClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  jsval vp;
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    JSObject *parentProto;
    NS_InitNodeClass(aContext, &parentProto);
    proto = JS_InitClass(aContext, // context
                          JS_GetGlobalObject(aContext), // global object
                          parentProto, // parent proto 
                          &element, // JSClass
                          Element, // JSNative ctor
                          0, // ctor args
                          NULL, // proto props
                          elementMethods, // proto funcs
                          NULL, // ctor props (static)
                          NULL); // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }
  }

  if (nsnull != aPrototype) {
    *aPrototype = proto;
  }

  return NS_OK;
}

//
// Element instance property ids
//

//
// Element instance properties
//

//
// New a element object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptElement(JSContext *aContext, nsIDOMElement *aElement, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aElement && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitElementClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &element, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aElement);
    aElement->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

