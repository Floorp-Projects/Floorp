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
// Plugin property ids
//
enum Plugin_slots {
  PLUGIN_DESCRIPTION = -1,
  PLUGIN_FILENAME = -2,
  PLUGIN_NAME = -3,
  PLUGIN_LENGTH = -4
};

/***********************************************************************/
//
// Plugin Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetPluginProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMPlugin *a = (nsIDOMPlugin*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case PLUGIN_DESCRIPTION:
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
      case PLUGIN_FILENAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFilename(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case PLUGIN_NAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case PLUGIN_LENGTH:
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
// Plugin Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetPluginProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMPlugin *a = (nsIDOMPlugin*)JS_GetPrivate(cx, obj);

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
// Plugin finalizer
//
PR_STATIC_CALLBACK(void)
FinalizePlugin(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Plugin enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumeratePlugin(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Plugin resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolvePlugin(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
PluginItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMPlugin *nativeThis = (nsIDOMPlugin*)JS_GetPrivate(cx, obj);
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
PluginNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMPlugin *nativeThis = (nsIDOMPlugin*)JS_GetPrivate(cx, obj);
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
// class for Plugin
//
JSClass PluginClass = {
  "Plugin", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetPluginProperty,
  SetPluginProperty,
  EnumeratePlugin,
  ResolvePlugin,
  JS_ConvertStub,
  FinalizePlugin
};


//
// Plugin class properties
//
static JSPropertySpec PluginProperties[] =
{
  {"description",    PLUGIN_DESCRIPTION,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"filename",    PLUGIN_FILENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    PLUGIN_NAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"length",    PLUGIN_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Plugin class methods
//
static JSFunctionSpec PluginMethods[] = 
{
  {"item",          PluginItem,     1},
  {"namedItem",          PluginNamedItem,     1},
  {0}
};


//
// Plugin constructor
//
PR_STATIC_CALLBACK(JSBool)
Plugin(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Plugin class initialization
//
extern "C" NS_DOM nsresult NS_InitPluginClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Plugin", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &PluginClass,      // JSClass
                         Plugin,            // JSNative ctor
                         0,             // ctor args
                         PluginProperties,  // proto props
                         PluginMethods,     // proto funcs
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
// Method for creating a new Plugin JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptPlugin(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptPlugin");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMPlugin *aPlugin;

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

  if (NS_OK != NS_InitPluginClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIPluginIID, (void **)&aPlugin);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &PluginClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aPlugin);
  }
  else {
    NS_RELEASE(aPlugin);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
