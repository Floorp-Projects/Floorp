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
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMMimeType.h"
#include "nsIDOMMimeTypeArray.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIMimeTypeIID, NS_IDOMMIMETYPE_IID);
static NS_DEFINE_IID(kIMimeTypeArrayIID, NS_IDOMMIMETYPEARRAY_IID);

NS_DEF_PTR(nsIDOMMimeType);
NS_DEF_PTR(nsIDOMMimeTypeArray);

//
// MimeTypeArray property ids
//
enum MimeTypeArray_slots {
  MIMETYPEARRAY_LENGTH = -1
};

/***********************************************************************/
//
// MimeTypeArray Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetMimeTypeArrayProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMimeTypeArray *a = (nsIDOMMimeTypeArray*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case MIMETYPEARRAY_LENGTH:
      {
        PRUint32 prop;
        if (NS_OK == a->GetLength(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
      {
        nsIDOMMimeType* prop;
        if (NS_OK == a->Item(JSVAL_TO_INT(id), &prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
      }
    }
  }
  else if (JSVAL_IS_STRING(id)) {
    nsIDOMMimeType* prop;
    nsAutoString name;

    JSString *jsstring = JS_ValueToString(cx, id);
    if (nsnull != jsstring) {
      name.SetString(JS_GetStringChars(jsstring));
    }
    else {
      name.SetString("");
    }

    if (NS_OK == a->NamedItem(name, &prop)) {
      if (NULL != prop) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
      }
      else {
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
      }
    }
    else {
      return JS_FALSE;
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// MimeTypeArray Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetMimeTypeArrayProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMimeTypeArray *a = (nsIDOMMimeTypeArray*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// MimeTypeArray finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeMimeTypeArray(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// MimeTypeArray enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateMimeTypeArray(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// MimeTypeArray resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveMimeTypeArray(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
MimeTypeArrayItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMMimeTypeArray *nativeThis = (nsIDOMMimeTypeArray*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMMimeType* nativeRet;
  PRUint32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Item(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function item requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method NamedItem
//
PR_STATIC_CALLBACK(JSBool)
MimeTypeArrayNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMMimeTypeArray *nativeThis = (nsIDOMMimeTypeArray*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMMimeType* nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->NamedItem(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function namedItem requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for MimeTypeArray
//
JSClass MimeTypeArrayClass = {
  "MimeTypeArray", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetMimeTypeArrayProperty,
  SetMimeTypeArrayProperty,
  EnumerateMimeTypeArray,
  ResolveMimeTypeArray,
  JS_ConvertStub,
  FinalizeMimeTypeArray
};


//
// MimeTypeArray class properties
//
static JSPropertySpec MimeTypeArrayProperties[] =
{
  {"length",    MIMETYPEARRAY_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// MimeTypeArray class methods
//
static JSFunctionSpec MimeTypeArrayMethods[] = 
{
  {"item",          MimeTypeArrayItem,     1},
  {"namedItem",          MimeTypeArrayNamedItem,     1},
  {0}
};


//
// MimeTypeArray constructor
//
PR_STATIC_CALLBACK(JSBool)
MimeTypeArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// MimeTypeArray class initialization
//
extern "C" NS_DOM nsresult NS_InitMimeTypeArrayClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "MimeTypeArray", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &MimeTypeArrayClass,      // JSClass
                         MimeTypeArray,            // JSNative ctor
                         0,             // ctor args
                         MimeTypeArrayProperties,  // proto props
                         MimeTypeArrayMethods,     // proto funcs
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
// Method for creating a new MimeTypeArray JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptMimeTypeArray(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptMimeTypeArray");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMMimeTypeArray *aMimeTypeArray;

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

  if (NS_OK != NS_InitMimeTypeArrayClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIMimeTypeArrayIID, (void **)&aMimeTypeArray);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &MimeTypeArrayClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aMimeTypeArray);
  }
  else {
    NS_RELEASE(aMimeTypeArray);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
