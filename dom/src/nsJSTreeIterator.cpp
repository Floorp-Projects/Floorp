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
#include "nsIDOMTreeIterator.h"
#include "nsIDOMNode.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kITreeIteratorIID, NS_IDOMTREEITERATOR_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);

NS_DEF_PTR(nsIDOMTreeIterator);
NS_DEF_PTR(nsIDOMNode);


/***********************************************************************/
//
// TreeIterator Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetTreeIteratorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMTreeIterator *a = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);

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
// TreeIterator Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetTreeIteratorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMTreeIterator *a = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);

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
// TreeIterator finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeTreeIterator(JSContext *cx, JSObject *obj)
{
  nsIDOMTreeIterator *a = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  
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
// TreeIterator enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateTreeIterator(JSContext *cx, JSObject *obj)
{
  nsIDOMTreeIterator *a = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  
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
// TreeIterator resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveTreeIterator(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMTreeIterator *a = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  
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
// Native method NumChildren
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorNumChildren(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRUint32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->NumChildren(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method NumPreviousSiblings
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorNumPreviousSiblings(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRUint32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->NumPreviousSiblings(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method NumNextSiblings
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorNumNextSiblings(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRUint32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->NumNextSiblings(&nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ToParent
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToParent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToParent(&nativeRet)) {
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
// Native method ToPreviousSibling
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToPreviousSibling(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToPreviousSibling(&nativeRet)) {
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
// Native method ToNextSibling
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToNextSibling(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToNextSibling(&nativeRet)) {
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
// Native method ToFirstChild
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToFirstChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToFirstChild(&nativeRet)) {
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
// Native method ToLastChild
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToLastChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToLastChild(&nativeRet)) {
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
// Native method ToNthChild
//
PR_STATIC_CALLBACK(JSBool)
TreeIteratorToNthChild(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMTreeIterator *nativeThis = (nsIDOMTreeIterator*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNode* nativeRet;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ToNthChild(b0, &nativeRet)) {
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


/***********************************************************************/
//
// class for TreeIterator
//
JSClass TreeIteratorClass = {
  "TreeIterator", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetTreeIteratorProperty,
  SetTreeIteratorProperty,
  EnumerateTreeIterator,
  ResolveTreeIterator,
  JS_ConvertStub,
  FinalizeTreeIterator
};


//
// TreeIterator class properties
//
static JSPropertySpec TreeIteratorProperties[] =
{
  {0}
};


//
// TreeIterator class methods
//
static JSFunctionSpec TreeIteratorMethods[] = 
{
  {"numChildren",          TreeIteratorNumChildren,     0},
  {"numPreviousSiblings",          TreeIteratorNumPreviousSiblings,     0},
  {"numNextSiblings",          TreeIteratorNumNextSiblings,     0},
  {"toParent",          TreeIteratorToParent,     0},
  {"toPreviousSibling",          TreeIteratorToPreviousSibling,     0},
  {"toNextSibling",          TreeIteratorToNextSibling,     0},
  {"toFirstChild",          TreeIteratorToFirstChild,     0},
  {"toLastChild",          TreeIteratorToLastChild,     0},
  {"toNthChild",          TreeIteratorToNthChild,     1},
  {0}
};


//
// TreeIterator constructor
//
PR_STATIC_CALLBACK(JSBool)
TreeIterator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}


//
// TreeIterator class initialization
//
nsresult NS_InitTreeIteratorClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "TreeIterator", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitNodeIteratorClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &TreeIteratorClass,      // JSClass
                         TreeIterator,            // JSNative ctor
                         0,             // ctor args
                         TreeIteratorProperties,  // proto props
                         TreeIteratorMethods,     // proto funcs
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
// Method for creating a new TreeIterator JavaScript object
//
extern "C" NS_DOM NS_NewScriptTreeIterator(nsIScriptContext *aContext, nsIDOMTreeIterator *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptTreeIterator");
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

  if (NS_OK != NS_InitTreeIteratorClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &TreeIteratorClass, proto, parent);
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
