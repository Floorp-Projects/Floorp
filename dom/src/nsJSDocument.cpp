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
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

//
// Document property ids
//
enum document_slot {
  DOCUMENT_DOCUMENTTYPE        = -21,
  DOCUMENT_DOCUMENTELEMENT     = -22,
  DOCUMENT_CONTEXTINFO         = -23
};

/***********************************************************************/

//
// Document properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocument *document = (nsIDOMDocument*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != document, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOCUMENT_DOCUMENTTYPE:
      {
        //XXX TBI
        break;
      }
      case DOCUMENT_DOCUMENTELEMENT:
      {
        nsIDOMElement *element = nsnull;
        // call the function
        if (NS_OK == document->GetDocumentElement(&element)) {

          // get the js object
          nsIScriptObjectOwner *owner = nsnull;
          if (NS_OK == element->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
            JSObject *object = nsnull;
            if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
              // set the return value
              *vp = OBJECT_TO_JSVAL(object);
            }
            NS_RELEASE(owner);
          }
          NS_RELEASE(element);
        }
        break;
      }
      case DOCUMENT_CONTEXTINFO:
      {
        //XXX TBI
        break;
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == document->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->GetProperty(cx, id, vp);
        }
      }
    }
  }

  return PR_TRUE;
}

//
// Document properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocument *document = (nsIDOMDocument*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != document, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case DOCUMENT_DOCUMENTTYPE:
      {
        //XXX TBI
        break;
      }
      case DOCUMENT_DOCUMENTELEMENT:
      {
        //XXX TBI
        break;
      }
      case DOCUMENT_CONTEXTINFO:
      {
        //XXX TBI
        break;
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == document->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->SetProperty(cx, id, vp);
        }
      }
    }
  }

  return PR_TRUE;
}

//
// Document finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeDocument(JSContext *cx, JSObject *obj)
{
  nsIDOMDocument *document = (nsIDOMDocument*)JS_GetPrivate(cx, obj);
  
  if (nsnull != document) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == document->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    document->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
CreateDocumentContext(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateTextNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateComment(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreatePI(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateAttributeList(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
CreateTreeIterator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
GetElementsByTagName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Document
//
JSClass document = {
  "Document", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetDocumentProperty,     
  SetDocumentProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeDocument
};

//
// Document class properties
//
static JSPropertySpec documentProperties[] =
{
  {"documentType",    DOCUMENT_DOCUMENTTYPE,     JSPROP_ENUMERATE},
  {"documentElement", DOCUMENT_DOCUMENTELEMENT,  JSPROP_ENUMERATE},
  {"contextInfo",     DOCUMENT_CONTEXTINFO,      JSPROP_ENUMERATE},
  {0}
};

//
// Document class methods
//
static JSFunctionSpec documentMethods[] = {
  {"createDocumentContext",   CreateDocumentContext,  0},
  {"createElement",           CreateElement,          2},
  {"createTextNode",          CreateTextNode,         1},
  {"createComment",           CreateComment,          1},
  {"createPI",                CreatePI,               2},
  {"createAttribute",         CreateAttribute,        2},
  {"createAttributeList",     CreateAttributeList,    0},
  {"createTreeIterator",      CreateTreeIterator,     1},
  {"getElementsByTagName",    GetElementsByTagName,   0},
  {0}
};

//
// Document constructor
//
PR_STATIC_CALLBACK(JSBool)
Document(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// Document static property ids
//

//
// Document static properties
//

//
// Document static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitNodeClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitDocumentClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  jsval vp;
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    JSObject *parentProto = nsnull;
    NS_InitNodeClass(aContext, &parentProto);
    proto = JS_InitClass(aContext, // context
                          JS_GetGlobalObject(aContext), // global object
                          parentProto, // parent proto 
                          &document, // JSClass
                          Document, // JSNative ctor
                          0, // ctor args
                          documentProperties, // proto props
                          documentMethods, // proto funcs
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
// Document instance property ids
//

//
// Document instance properties
//

//
// New a document object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptDocument(JSContext *aContext, nsIDOMDocument *aDocument, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aDocument && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitDocumentClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &document, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aDocument);
    aDocument->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

