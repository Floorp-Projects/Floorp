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
#include "nsIDOMIterators.h"
#include "nsIDOMNode.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

/***********************************************************************/

//
// NodeIterator properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetNodeIteratorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  // NS_ASSERTION(nsnull != nodeIterator, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == nodeIterator->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->GetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// NodeIterator properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetNodeIteratorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == nodeIterator->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->SetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// NodeIterator finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeNodeIterator(JSContext *cx, JSObject *obj)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  
  if (nsnull != nodeIterator) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == nodeIterator->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    nodeIterator->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
SetFilter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetLength(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");

  PRUint32 length = 0;
  // call the function
  if (NS_OK == nodeIterator->GetLength(&length)) {
    // set the return value
    *rval = INT_TO_JSVAL(length);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetCurrentNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *aNode1 = nsnull;
  // call the function
  if (NS_OK == nodeIterator->GetCurrentNode(&aNode1)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aNode1);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetNextNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *aNode1 = nsnull;
  // call the function
  if (NS_OK == nodeIterator->GetNextNode(&aNode1)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aNode1);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetPreviousNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *aNode1 = nsnull;
  // call the function
  if (NS_OK == nodeIterator->GetPreviousNode(&aNode1)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aNode1);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
ToFirst(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *aNode1 = nsnull;
  // call the function
  if (NS_OK == nodeIterator->ToFirst(&aNode1)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aNode1);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
ToLast(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *aNode1 = nsnull;
  // call the function
  if (NS_OK == nodeIterator->ToLast(&aNode1)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        // set the return value
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(aNode1);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
MoveTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNodeIterator *nodeIterator = (nsIDOMNodeIterator*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nodeIterator, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_INT(argv[0])) {

      nsIDOMNode *aNode1 = nsnull;
      // call the function
      if (NS_OK == nodeIterator->MoveTo(JSVAL_TO_INT(argv[0]), &aNode1)) {

        // get the js object
        nsIScriptObjectOwner *owner = nsnull;
        if (NS_OK == aNode1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
          JSObject *object = nsnull;
          if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
            // set the return value
            *rval = OBJECT_TO_JSVAL(object);
          }
          NS_RELEASE(owner);
        }
        NS_RELEASE(aNode1);
      }
    }
  }

  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM NodeIterator
//
JSClass nodeIterator = {
  "NodeIterator", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetNodeIteratorProperty,     
  SetNodeIteratorProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeNodeIterator
};

//
// NodeIterator property ids
//

//
// NodeIterator class properties
//

//
// NodeIterator class methods
//
static JSFunctionSpec nodeIteratorMethods[] = {
  {"setFilter",         SetFilter,          2},
  {"getLength",         GetLength,          0},
  {"getCurrentNode",    GetCurrentNode,     0},
  {"getNextNode",       GetNextNode,        0},
  {"getPreviousNode",   GetPreviousNode,    0},
  {"toFirst",           ToFirst,            0},
  {"toLast",            ToLast,             0},
  {"moveTo",            MoveTo,             1},
  {0}
};

//
// NodeIterator constructor
//
PR_STATIC_CALLBACK(JSBool)
NodeIterator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// NodeIterator static property ids
//

//
// NodeIterator static properties
//

//
// NodeIterator static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitNodeIteratorClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  jsval vp;
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    proto = JS_InitClass(aContext, // context
                          JS_GetGlobalObject(aContext), // global object
                          NULL, // parent proto 
                          &nodeIterator, // JSClass
                          NodeIterator, // JSNative ctor
                          0, // ctor args
                          NULL, // proto props
                          nodeIteratorMethods, // proto funcs
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
// NodeIterator instance property ids
//

//
// NodeIterator instance properties
//

//
// New a nodeIterator object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptNodeIterator(JSContext *aContext, nsIDOMNodeIterator *aNodeIterator, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aNodeIterator && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitNodeIteratorClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &nodeIterator, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aNodeIterator);
    aNodeIterator->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

