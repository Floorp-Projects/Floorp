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
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMLocation.h"
#include "nsIDOMNSLocation.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kILocationIID, NS_IDOMLOCATION_IID);
static NS_DEFINE_IID(kINSLocationIID, NS_IDOMNSLOCATION_IID);

NS_DEF_PTR(nsIDOMLocation);
NS_DEF_PTR(nsIDOMNSLocation);

//
// Location property ids
//
enum Location_slots {
  LOCATION_HASH = -1,
  LOCATION_HOST = -2,
  LOCATION_HOSTNAME = -3,
  LOCATION_PATHNAME = -4,
  LOCATION_PORT = -5,
  LOCATION_PROTOCOL = -6,
  LOCATION_SEARCH = -7
};

/***********************************************************************/
//
// Location Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetLocationProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMLocation *a = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case LOCATION_HASH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.hash", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHash(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_HOST:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.host", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHost(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_HOSTNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.hostname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHostname(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_PATHNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.pathname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPathname(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_PORT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.port", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPort(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_PROTOCOL:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.protocol", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetProtocol(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case LOCATION_SEARCH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.search", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSearch(prop))) {
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
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Location Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetLocationProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMLocation *a = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case LOCATION_HASH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.hash", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHash(prop);
        
        break;
      }
      case LOCATION_HOST:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.host", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHost(prop);
        
        break;
      }
      case LOCATION_HOSTNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.hostname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHostname(prop);
        
        break;
      }
      case LOCATION_PATHNAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.pathname", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPathname(prop);
        
        break;
      }
      case LOCATION_PORT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.port", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPort(prop);
        
        break;
      }
      case LOCATION_PROTOCOL:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.protocol", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetProtocol(prop);
        
        break;
      }
      case LOCATION_SEARCH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "location.search", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSearch(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// Location finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeLocation(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Location enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateLocation(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Location resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveLocation(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
LocationToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMLocation *nativeThis = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "location.tostring", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->ToString(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Reload
//
PR_STATIC_CALLBACK(JSBool)
NSLocationReload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMLocation *privateThis = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNSLocation *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kINSLocationIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type NSLocation");
    return JS_FALSE;
  }


  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "nslocation.reload", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Reload(cx, argv+0, argc-0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Replace
//
PR_STATIC_CALLBACK(JSBool)
NSLocationReplace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMLocation *privateThis = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNSLocation *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kINSLocationIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type NSLocation");
    return JS_FALSE;
  }


  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "nslocation.replace", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Replace(cx, argv+0, argc-0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Location
//
JSClass LocationClass = {
  "Location", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetLocationProperty,
  SetLocationProperty,
  EnumerateLocation,
  ResolveLocation,
  JS_ConvertStub,
  FinalizeLocation
};


//
// Location class properties
//
static JSPropertySpec LocationProperties[] =
{
  {"hash",    LOCATION_HASH,    JSPROP_ENUMERATE},
  {"host",    LOCATION_HOST,    JSPROP_ENUMERATE},
  {"hostname",    LOCATION_HOSTNAME,    JSPROP_ENUMERATE},
  {"pathname",    LOCATION_PATHNAME,    JSPROP_ENUMERATE},
  {"port",    LOCATION_PORT,    JSPROP_ENUMERATE},
  {"protocol",    LOCATION_PROTOCOL,    JSPROP_ENUMERATE},
  {"search",    LOCATION_SEARCH,    JSPROP_ENUMERATE},
  {0}
};


//
// Location class methods
//
static JSFunctionSpec LocationMethods[] = 
{
  {"toString",          LocationToString,     0},
  {"reload",          NSLocationReload,     0},
  {"replace",          NSLocationReplace,     0},
  {0}
};


//
// Location constructor
//
PR_STATIC_CALLBACK(JSBool)
Location(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Location class initialization
//
extern "C" NS_DOM nsresult NS_InitLocationClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Location", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &LocationClass,      // JSClass
                         Location,            // JSNative ctor
                         0,             // ctor args
                         LocationProperties,  // proto props
                         LocationMethods,     // proto funcs
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
// Method for creating a new Location JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptLocation(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptLocation");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMLocation *aLocation;

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

  if (NS_OK != NS_InitLocationClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kILocationIID, (void **)&aLocation);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &LocationClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aLocation);
  }
  else {
    NS_RELEASE(aLocation);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
