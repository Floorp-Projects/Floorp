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

#include "nscore.h"
#include "nsIScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMNode.h"
#include "nsIDOMIterators.h"
#include "jsapi.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

/***********************************************************************/

//
// Node properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetNodeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == node->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->GetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// Node properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetNodeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
    default:
      nsIScriptObject *object;
      if (NS_OK == node->QueryInterface(kIScriptObjectIID, (void**)&object)) {
        return object->SetProperty(cx, id, vp);
      }
    }
  }

  return JS_TRUE;
}

//
// Node finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeNode(JSContext *cx, JSObject *obj)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");

  // get the js object
  nsIScriptObjectOwner *owner = nsnull;
  if (NS_OK == node->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    owner->ResetScriptObject();
    NS_RELEASE(owner);
  }

  node->Release();
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
GetNodeType(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");

  PRInt32 type = 0;
  // call the function
  if (NS_OK == node->GetNodeType(&type)) {
    *rval = INT_TO_JSVAL(type);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetParentNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *parent = nsnull;
  // call the function
  if (NS_OK == node->GetParentNode(&parent)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == parent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(parent);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetChildNodes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNodeIterator *nodeIterator = nsnull;
  // call the function
  if (NS_OK == node->GetChildNodes(&nodeIterator)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == nodeIterator->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(nodeIterator);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
HasChildNodes(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");

  // call the function
  if (NS_OK == node->HasChildNodes()) {
    *rval = JSVAL_TRUE;
  }
  else {
    *rval = JSVAL_FALSE;
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetFirstChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *child = nsnull;
  // call the function
  if (NS_OK == node->GetFirstChild(&child)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == child->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(child);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetPreviousSibling(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *previous = nsnull;
  // call the function
  if (NS_OK == node->GetPreviousSibling(&previous)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == previous->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(previous);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetNextSibling(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  *rval = JSVAL_NULL;

  nsIDOMNode *next = nsnull;
  // call the function
  if (NS_OK == node->GetNextSibling(&next)) {

    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == next->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      JSObject *object = nsnull;
      if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
        *rval = OBJECT_TO_JSVAL(object);
      }
      NS_RELEASE(owner);
    }
    NS_RELEASE(next);
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
InsertBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  NS_ASSERTION(2 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (2 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0]) && JSVAL_IS_OBJECT(argv[1])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      JSObject *obj2 = JSVAL_TO_OBJECT(argv[1]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");
      nsISupports *support2 = (nsISupports*)JS_GetPrivate(cx, obj2);
      NS_ASSERTION(nsnull != support2, "null pointer");

      nsIDOMNode *node1 = nsnull;
      nsIDOMNode *node2 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMNodeIID, (void**)&node1)) {
        if (NS_OK == support2->QueryInterface(kIDOMNodeIID, (void**)&node2)) {

          // call the function
          if (NS_OK == node->InsertBefore(node1, node2)) {
            
            // get the js object
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == node1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
                *rval = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
          }
          NS_RELEASE(node2);
        }
        NS_RELEASE(node1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
ReplaceChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  NS_ASSERTION(2 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (2 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0]) && JSVAL_IS_OBJECT(argv[1])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      JSObject *obj2 = JSVAL_TO_OBJECT(argv[1]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");
      nsISupports *support2 = (nsISupports*)JS_GetPrivate(cx, obj2);
      NS_ASSERTION(nsnull != support2, "null pointer");

      nsIDOMNode *node1 = nsnull;
      nsIDOMNode *node2 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMNodeIID, (void**)&node1)) {
        if (NS_OK == support2->QueryInterface(kIDOMNodeIID, (void**)&node2)) {

          // call the function
          if (NS_OK == node->ReplaceChild(node1, node2)) {
            
            // get the js object
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == node2->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
                *rval = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
          }
          NS_RELEASE(node2);
        }
        NS_RELEASE(node1);
      }
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
RemoveChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNode *node = (nsIDOMNode*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != node, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  *rval = JSVAL_NULL;
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_OBJECT(argv[0])) {
      JSObject *obj1 = JSVAL_TO_OBJECT(argv[0]);
      //XXX should check if that's a good class (do not just GetPrivate)
      nsISupports *support1 = (nsISupports*)JS_GetPrivate(cx, obj1);
      NS_ASSERTION(nsnull != support1, "null pointer");

      nsIDOMNode *node1 = nsnull;
      if (NS_OK == support1->QueryInterface(kIDOMNodeIID, (void**)&node1)) {

        // call the function
        if (NS_OK == node->RemoveChild(node1)) {
          
          // get the js object
          nsIScriptObjectOwner *owner = nsnull;
          if (NS_OK == node1->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
            JSObject *object = nsnull;
            if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
              *rval = OBJECT_TO_JSVAL(object);
            }
            NS_RELEASE(owner);
          }
        }
        NS_RELEASE(node1);
      }
    }
  }

  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Node
//
JSClass node = {
  "Node", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetNodeProperty,     
  SetNodeProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeNode
};

//
// Node property ids
//

//
// Node class properties
//

//
// Node class methods
//
static JSFunctionSpec nodeMethods[] = {
  {"GetNodeType",         GetNodeType,          0},
  {"GetParentNode",       GetParentNode,        0},
  {"GetChildNodes",       GetChildNodes,        0},
  {"HasChildNodes",       HasChildNodes,        0},
  {"GetFirstChild",       GetFirstChild,        0},
  {"GetPreviousSibling",  GetPreviousSibling,   0},
  {"GetNextSibling",      GetNextSibling,       0},
  {"InsertBefore",        InsertBefore,         2},
  {"ReplaceChild",        ReplaceChild,         2},
  {"RemoveChild",         RemoveChild,          1},
  {0}
};

//
// Node constructor
//
PR_STATIC_CALLBACK(JSBool)
Node(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// Node static property ids
//

//
// Node static properties
//

//
// Node static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitNodeClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  static JSObject *proto = nsnull;
  JSObject *global = JS_GetGlobalObject(aContext);

  if (nsnull == proto) {
    proto = JS_InitClass(aContext, // context
                          global, // global object
                          NULL, // parent proto 
                          &node, // JSClass
                          Node, // JSNative ctor
                          0, // ctor args
                          NULL, // proto props
                          nodeMethods, // proto funcs
                          NULL, // ctor props (static)
                          NULL); // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    jsval vp;
    if (PR_TRUE == JS_GetProperty(aContext, global, node.name, &vp)) {
      if (JSVAL_IS_OBJECT(vp)) {
        jsval value = INT_TO_JSVAL(1);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "DOCUMENT", &value);
        value = INT_TO_JSVAL(2);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "ELEMENT", &value);
        value = INT_TO_JSVAL(3);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "ATTRIBUTE", &value);
        value = INT_TO_JSVAL(4);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "PI", &value);
        value = INT_TO_JSVAL(5);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "COMMENT", &value);
        value = INT_TO_JSVAL(6);
        JS_SetProperty(aContext, JSVAL_TO_OBJECT(vp), "TEXT", &value);
      }
    }
  }

  if (nsnull != aPrototype) {
    *aPrototype = proto;
  }

  return NS_OK;
}

//
// Node instance property ids
//

//
// Node instance properties
//

//
// New a node object in js, connect the native and js worlds
//
nsresult NS_NewScriptNode(JSContext *aContext, nsIDOMNode *aNode, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aNode && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitNodeClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &node, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aNode);
    aNode->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

