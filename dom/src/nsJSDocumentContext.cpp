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
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentContext.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDocumentContextIID, NS_IDOMDOCUMENTCONTEXT_IID);

NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMDocumentContext);

//
// DocumentContext property ids
//
enum DocumentContext_slots {
  DOCUMENTCONTEXT_DOCUMENT = -11
};

/***********************************************************************/
//
// DocumentContext Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetDocumentContextProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocumentContext *a = (nsIDOMDocumentContext*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOCUMENTCONTEXT_DOCUMENT:
      {
        nsIDOMDocument* prop;
        if (NS_OK == a->GetDocument(&prop)) {
          // get the js object
          if (prop != nsnull) {
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == prop->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
              if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
                // set the return value
                *vp = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
            NS_RELEASE(prop);
          }
          else {
            *vp = JSVAL_NULL;
          }
        }
        else {
          return JS_FALSE;
        }
        break;
      }
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
// DocumentContext Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetDocumentContextProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocumentContext *a = (nsIDOMDocumentContext*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOCUMENTCONTEXT_DOCUMENT:
      {
        nsIDOMDocument* prop;
        if (JSVAL_IS_NULL(*vp)) {
          prop = nsnull;
        }
        else if (JSVAL_IS_OBJECT(*vp)) {
          JSObject *jsobj = JSVAL_TO_OBJECT(*vp); 
          nsISupports *supports = (nsISupports *)JS_GetPrivate(cx, jsobj);
          if (NS_OK != supports->QueryInterface(kIDocumentIID, (void **)&prop)) {
            return JS_FALSE;
          }
        }
        else {
          return JS_FALSE;
        }
      
        a->SetDocument(prop);
        if (prop) NS_RELEASE(prop);
        break;
      }
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
// DocumentContext finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeDocumentContext(JSContext *cx, JSObject *obj)
{
  nsIDOMDocumentContext *a = (nsIDOMDocumentContext*)JS_GetPrivate(cx, obj);
  
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
// DocumentContext enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateDocumentContext(JSContext *cx, JSObject *obj)
{
  nsIDOMDocumentContext *a = (nsIDOMDocumentContext*)JS_GetPrivate(cx, obj);
  
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
// DocumentContext resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveDocumentContext(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMDocumentContext *a = (nsIDOMDocumentContext*)JS_GetPrivate(cx, obj);
  
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


/***********************************************************************/
//
// class for DocumentContext
//
JSClass DocumentContextClass = {
  "DocumentContext", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetDocumentContextProperty,
  SetDocumentContextProperty,
  EnumerateDocumentContext,
  ResolveDocumentContext,
  JS_ConvertStub,
  FinalizeDocumentContext
};


//
// DocumentContext class properties
//
static JSPropertySpec DocumentContextProperties[] =
{
  {"document",    DOCUMENTCONTEXT_DOCUMENT,    JSPROP_ENUMERATE},
  {0}
};


//
// DocumentContext class methods
//
static JSFunctionSpec DocumentContextMethods[] = 
{
  {0}
};


//
// DocumentContext constructor
//
PR_STATIC_CALLBACK(JSBool)
DocumentContext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}


//
// DocumentContext class initialization
//
nsresult NS_InitDocumentContextClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "DocumentContext", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &DocumentContextClass,      // JSClass
                         DocumentContext,            // JSNative ctor
                         0,             // ctor args
                         DocumentContextProperties,  // proto props
                         DocumentContextMethods,     // proto funcs
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
// Method for creating a new DocumentContext JavaScript object
//
extern "C" NS_DOM NS_NewScriptDocumentContext(nsIScriptContext *aContext, nsIDOMDocumentContext *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptDocumentContext");
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

  if (NS_OK != NS_InitDocumentContextClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &DocumentContextClass, proto, parent);
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
