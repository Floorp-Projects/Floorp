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
#include "nsIDOMAttribute.h"
#include "nsString.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMAttributeIID, NS_IDOMATTRIBUTE_IID);

/***********************************************************************/

//
// AttributeList properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetAttributeListProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  // NS_ASSERTION(nsnull != attributeList, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == attributeList->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->GetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// AttributeList properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetAttributeListProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == attributeList->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->SetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// Document finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeAttributeList(JSContext *cx, JSObject *obj)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);

  if (nsnull != attributeList) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == attributeList->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    attributeList->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
GetAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      nsIDOMAttribute *attribute;
      if (NS_OK == attributeList->GetAttribute(string1, &attribute)) {

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
SetAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");

      nsIDOMAttribute *attribute1 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMAttributeIID, (void**)&attribute1)) {
        if (NS_OK == attributeList->SetAttribute(attribute1)) {
          // set the return value
          *rval = argv[0];
        }
        NS_RELEASE(attribute1);
      }
    }
  }
   
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Remove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);
      
      // call the function
      nsIDOMAttribute *attribute1 = nsnull;
      if (NS_OK == attributeList->Remove(string1, &attribute1)) {

        // get the js object
        nsIScriptObjectOwner *owner = nsnull;
        if (NS_OK == attribute1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
          JSObject *object = nsnull;
          if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
            // set the return value
            *rval = OBJECT_TO_JSVAL(object);
          }
          NS_RELEASE(owner);
        }
        NS_RELEASE(attribute1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Item(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_INT(argv[0])) {
      PRInt32 pos = JSVAL_TO_INT(argv[0]);
      
      // call the function
      nsIDOMAttribute *attribute1 = nsnull;
      if (NS_OK == attributeList->Item(pos, &attribute1)) {

        // get the js object
        nsIScriptObjectOwner *owner = nsnull;
        if (NS_OK == attribute1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
          JSObject *object = nsnull;
          if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
            // set the return value
            *rval = OBJECT_TO_JSVAL(object);
          }
          NS_RELEASE(owner);
        }
        NS_RELEASE(attribute1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetLength(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttributeList *attributeList = (nsIDOMAttributeList*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attributeList, "null pointer");
  
  PRUint32 length;

  // call the function
  if (NS_OK == attributeList->GetLength(&length)) {

    // set the return value
    *rval = INT_TO_JSVAL((PRInt32)length);
  }
  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM AttributeList
//
JSClass attributeList = {
  "AttributeList", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetAttributeListProperty,     
  SetAttributeListProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeAttributeList
};

//
// AttributeList property ids
//

//
// AttributeList class properties
//

//
// AttributeList class methods
//
static JSFunctionSpec attributeListMethods[] = {
  {"getAttribute",      GetAttribute,     1},
  {"setAttribute",      SetAttribute,     1},
  {"remove",            Remove,           1},
  {"item",              Item,             1},
  {"getLength",         GetLength,        0},
  {0}
};

//
// AttributeList constructor
//
PR_STATIC_CALLBACK(JSBool)
AttributeList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// AttributeList static property ids
//

//
// AttributeList static properties
//

//
// AttributeList static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitAttributeListClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    proto = JS_InitClass(aContext, // context
                          JS_GetGlobalObject(aContext), // global object
                          NULL, // parent proto 
                          &attributeList, // JSClass
                          AttributeList, // JSNative ctor
                          0, // ctor args
                          NULL, // proto props
                          attributeListMethods, // proto funcs
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
// AttributeList instance property ids
//

//
// AttributeList instance properties
//

//
// New a AttributeList object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptAttributeList(JSContext *aContext, nsIDOMAttributeList *aAttributeList, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aAttributeList && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitAttributeListClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &attributeList, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aAttributeList);
    aAttributeList->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

