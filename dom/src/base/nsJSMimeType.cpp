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
#include "nsIDOMPlugin.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIMimeTypeIID, NS_IDOMMIMETYPE_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IDOMPLUGIN_IID);

NS_DEF_PTR(nsIDOMMimeType);
NS_DEF_PTR(nsIDOMPlugin);

//
// MimeType property ids
//
enum MimeType_slots {
  MIMETYPE_DESCRIPTION = -1,
  MIMETYPE_ENABLEDPLUGIN = -2,
  MIMETYPE_SUFFIXES = -3,
  MIMETYPE_TYPE = -4
};

/***********************************************************************/
//
// MimeType Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetMimeTypeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMimeType *a = (nsIDOMMimeType*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case MIMETYPE_DESCRIPTION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetDescription(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case MIMETYPE_ENABLEDPLUGIN:
      {
        nsIDOMPlugin* prop;
        if (NS_OK == a->GetEnabledPlugin(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case MIMETYPE_SUFFIXES:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSuffixes(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case MIMETYPE_TYPE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetType(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// MimeType Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetMimeTypeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMimeType *a = (nsIDOMMimeType*)JS_GetPrivate(cx, obj);

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
// MimeType finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeMimeType(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// MimeType enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateMimeType(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// MimeType resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveMimeType(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for MimeType
//
JSClass MimeTypeClass = {
  "MimeType", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetMimeTypeProperty,
  SetMimeTypeProperty,
  EnumerateMimeType,
  ResolveMimeType,
  JS_ConvertStub,
  FinalizeMimeType
};


//
// MimeType class properties
//
static JSPropertySpec MimeTypeProperties[] =
{
  {"description",    MIMETYPE_DESCRIPTION,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"enabledPlugin",    MIMETYPE_ENABLEDPLUGIN,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"suffixes",    MIMETYPE_SUFFIXES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"type",    MIMETYPE_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// MimeType class methods
//
static JSFunctionSpec MimeTypeMethods[] = 
{
  {0}
};


//
// MimeType constructor
//
PR_STATIC_CALLBACK(JSBool)
MimeType(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// MimeType class initialization
//
extern "C" NS_DOM nsresult NS_InitMimeTypeClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "MimeType", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &MimeTypeClass,      // JSClass
                         MimeType,            // JSNative ctor
                         0,             // ctor args
                         MimeTypeProperties,  // proto props
                         MimeTypeMethods,     // proto funcs
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
// Method for creating a new MimeType JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptMimeType(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptMimeType");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMMimeType *aMimeType;

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

  if (NS_OK != NS_InitMimeTypeClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIMimeTypeIID, (void **)&aMimeType);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &MimeTypeClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aMimeType);
  }
  else {
    NS_RELEASE(aMimeType);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
