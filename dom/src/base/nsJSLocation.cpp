/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIDOMLocation.h"
#include "nsIDOMNSLocation.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kILocationIID, NS_IDOMLOCATION_IID);
static NS_DEFINE_IID(kINSLocationIID, NS_IDOMNSLOCATION_IID);

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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case LOCATION_HASH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HASH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHash(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_HOST:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HOST, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHost(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_HOSTNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HOSTNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHostname(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_PATHNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PATHNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPathname(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_PORT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PORT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPort(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_PROTOCOL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PROTOCOL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetProtocol(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LOCATION_SEARCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_SEARCH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSearch(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case LOCATION_HASH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HASH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHash(prop);
          
        }
        break;
      }
      case LOCATION_HOST:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HOST, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHost(prop);
          
        }
        break;
      }
      case LOCATION_HOSTNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_HOSTNAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHostname(prop);
          
        }
        break;
      }
      case LOCATION_PATHNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PATHNAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPathname(prop);
          
        }
        break;
      }
      case LOCATION_PORT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PORT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPort(prop);
          
        }
        break;
      }
      case LOCATION_PROTOCOL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_PROTOCOL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetProtocol(prop);
          
        }
        break;
      }
      case LOCATION_SEARCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_SEARCH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSearch(prop);
          
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
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
// Native method Assign
//
PR_STATIC_CALLBACK(JSBool)
LocationAssign(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMLocation *nativeThis = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_ASSIGN, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Assign(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
LocationToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMLocation *nativeThis = (nsIDOMLocation*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LOCATION_TOSTRING, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->ToString(nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsCOMPtr<nsIDOMNSLocation> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSLocationIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSLOCATION_RELOAD, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Reload(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsCOMPtr<nsIDOMNSLocation> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSLocationIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSLOCATION_REPLACE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Replace(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  FinalizeLocation,
  nsnull,
  nsJSUtils::nsCheckAccess
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
  {"assign",          LocationAssign,     1},
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
