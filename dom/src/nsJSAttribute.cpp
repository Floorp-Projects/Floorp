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

//
// Attribute property ids
//
enum attribute_slot {
  ATTRIBUTE_VALUE             = -1,
  ATTRIBUTE_SPECIFIED         = -2,
};

/***********************************************************************/

//
// Attribute properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetAttributeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMAttribute *attribute = (nsIDOMAttribute*)JS_GetPrivate(cx, obj);
  // NS_ASSERTION(nsnull != attribute, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case ATTRIBUTE_VALUE:
      {
        nsAutoString string;
        if (NS_OK == attribute->GetValue(string)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, string.GetUnicode(), string.Length());
          *vp = STRING_TO_JSVAL(jsstring);
        }

        break;
      }
      case ATTRIBUTE_SPECIFIED:
      {
        if (NS_OK == attribute->GetSpecified()) {
          *vp = JSVAL_TRUE;
        }
        else {
          *vp = JSVAL_FALSE;
        }

        break;
      }
      default:
        nsIScriptObject *object;
        if (NS_OK == attribute->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->GetProperty(cx, id, vp);
        }
    }
  }

  return JS_TRUE;
}

//
// Attribute properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetAttributeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMAttribute *attribute = (nsIDOMAttribute*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attribute, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case ATTRIBUTE_VALUE:
      {
        if (JSVAL_IS_STRING(*vp)) {
          JSString *jsstring = JSVAL_TO_STRING(*vp);
          nsAutoString string = JS_GetStringChars(jsstring);
          attribute->SetValue(string);
        }
        break;
      }
      case ATTRIBUTE_SPECIFIED:
      {
        if (JSVAL_IS_BOOLEAN(*vp)) {
          attribute->SetSpecified(JSVAL_TO_BOOLEAN(*vp));
        }
        break;
      }
      default:
        nsIScriptObject *object;
        if (NS_OK == attribute->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          return object->GetProperty(cx, id, vp);
        }
    }
  }

  return JS_TRUE;
}

//
// Attribute finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeAttribute(JSContext *cx, JSObject *obj)
{
  nsIDOMAttribute *attribute = (nsIDOMAttribute*)JS_GetPrivate(cx, obj);
  
  if (nsnull != attribute) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == attribute->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }

    attribute->Release();
  }
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
GetName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttribute *attribute = (nsIDOMAttribute*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attribute, "null pointer");
  nsAutoString string;
  if (NS_OK == attribute->GetName(string)) {
    JSString *jsstring = JS_NewUCStringCopyN(cx, string.GetUnicode(), string.Length());
    *rval = STRING_TO_JSVAL(jsstring);
  }
  return JS_TRUE;
}

PR_STATIC_CALLBACK(JSBool)
ToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMAttribute *attribute = (nsIDOMAttribute*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != attribute, "null pointer");
  nsAutoString string;
  if (NS_OK == attribute->ToString(string)) {
    JSString *jsstring = JS_NewUCStringCopyN(cx, string.GetUnicode(), string.Length());
    *rval = STRING_TO_JSVAL(jsstring);
  }
  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Attribute
//
JSClass attribute = {
  "Attribute", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetAttributeProperty,     
  SetAttributeProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeAttribute
};

//
// Attribute class properties
//
static JSPropertySpec attributeProperties[] =
{
  {"value",           ATTRIBUTE_VALUE,          JSPROP_ENUMERATE},
  {"specified",       ATTRIBUTE_SPECIFIED,      JSPROP_ENUMERATE},
  {0}
};

//
// Attribute class methods
//
static JSFunctionSpec attributeMethods[] = {
  {"getName",         GetName,          0},
  {"toString",        ToString,         0},
  {0}
};

//
// Attribute constructor
//
PR_STATIC_CALLBACK(JSBool)
Attribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}

//
// Attribute static property ids
//

//
// Attribute static properties
//

//
// Attribute static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitAttributeClass(JSContext *aContext, JSObject **aPrototype)
{
  // look in the global object for this class prototype
  jsval vp;
  static JSObject *proto = nsnull;

  if (nsnull == proto) {
    proto = JS_InitClass(aContext, // context
                          JS_GetGlobalObject(aContext), // global object
                          NULL, // parent proto 
                          &attribute, // JSClass
                          Attribute, // JSNative ctor
                          0, // ctor args
                          attributeProperties, // proto props
                          attributeMethods, // proto funcs
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
// Attribute instance property ids
//

//
// Attribute instance properties
//

//
// New a attribute object in js, connect the native and js worlds
//
extern "C" NS_DOM nsresult NS_NewScriptAttribute(JSContext *aContext, nsIDOMAttribute *aAttribute, JSObject *aParent, JSObject **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aAttribute && nsnull != aJSObject, "null arg");
  JSObject *proto;
  NS_InitAttributeClass(aContext, &proto);

  // create a js object for this class
  *aJSObject = JS_NewObject(aContext, &attribute, proto, aParent);
  if (nsnull != *aJSObject) {
    // define the instance specific properties

    // connect the native object to the js object
    JS_SetPrivate(aContext, *aJSObject, aAttribute);
    aAttribute->AddRef();
  }
  else return NS_ERROR_FAILURE; 

  return NS_OK;
}

