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
#include "nsIDOMText.h"
#include "nsString.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

//
// Text property ids
//
enum text_slot {
  TEXT_DATA                     = -1
};

/***********************************************************************/

//
// Text properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetTextProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMText *text = (nsIDOMText*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != text, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case TEXT_DATA:
      {
        nsAutoString string;
        if (NS_OK == text->GetData(string)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, string, string.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == text->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->GetProperty(cx, id, vp);
        }
      }
    }
  }

  return JS_TRUE;
}

//
// Text properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetTextProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMText *text = (nsIDOMText*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != text, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case TEXT_DATA:
      {
        if (JSVAL_IS_STRING(*vp)) {
          JSString *jsstring = JSVAL_TO_STRING(*vp);
          nsAutoString string = JS_GetStringChars(jsstring);
          text->SetData(string);
        }
        break;
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == text->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->SetProperty(cx, id, vp);
        }
      }
    }
  }

  return JS_TRUE;
}

//
// Finalize Text
//
PR_STATIC_CALLBACK(void)
FinalizeText(JSContext *cx, JSObject *obj)
{
  nsIDOMText *text = (nsIDOMText*)JS_GetPrivate(cx, obj);
  
  if (nsnull != text) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == text->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    text->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
Append(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMText *text = (nsIDOMText*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != text, "null pointer");
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      nsAutoString string1 = JS_GetStringChars(jsstring1);

      // call the function
      text->Append(string1);
    }
  }

  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Insert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMText *text = (nsIDOMText*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != text, "null pointer");
  NS_ASSERTION(2 == argc, "wrong number of arguments");
  if (2 == argc) {
    // get the arguments
    if (JSVAL_IS_INT(argv[0]) &&
        JSVAL_IS_STRING(argv[1])) {
      JSString *jsstring2 = JSVAL_TO_STRING(argv[1]);
      nsAutoString string2 = JS_GetStringChars(jsstring2);

      // call the function
      text->Insert(JSVAL_TO_INT(argv[0]), string2);
    }
  }
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Delete(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Replace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
Splice(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  //XXX TBI
  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Text
//
JSClass text = {
  "Text", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetTextProperty,     
  SetTextProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeText
};

//
// Text class properties
//
static JSPropertySpec textProperties[] =
{
  {"data",            TEXT_DATA,             JSPROP_ENUMERATE},
  {0}
};

//
// Text class methods
//
static JSFunctionSpec textMethods[] = {
  {"append",          Append,     1},
  {"insert",          Insert,     2},
  {"delete",          Delete,     2},
  {"replace",         Replace,    3},
  {"splice",          Splice,     3},
  {0}
};

//
// Text constructor
//
PR_STATIC_CALLBACK(JSBool)
Text(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// Text static property ids
//

//
// Text static properties
//

//
// Text static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitNodeClass(JSContext *aContext, JSObject **aPrototype);
nsresult NS_InitTextClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  jsval vp;
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    JSObject *parentProto;
    NS_InitNodeClass(aContext, &parentProto);
    JSObject *proto;
    proto = JS_InitClass(aContext, // context
                        JS_GetGlobalObject(aContext), // global object
                        parentProto, // parent proto 
                        &text, // JSClass
                        Text, // JSNative ctor
                        0, // ctor args
                        textProperties, // proto props
                        textMethods, // proto funcs
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
// Text instance property ids
//

//
// Text instance properties
//

//
// New a text object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptText(JSContext *aContext, nsIDOMText *aText, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aText && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitTextClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &text, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aText);
    aText->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

