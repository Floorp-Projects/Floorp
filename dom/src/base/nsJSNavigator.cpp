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
#include "nsIDOMNavigator.h"
#include "nsIDOMPluginArray.h"
#include "nsIDOMMimeTypeArray.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIPluginArrayIID, NS_IDOMPLUGINARRAY_IID);
static NS_DEFINE_IID(kIMimeTypeArrayIID, NS_IDOMMIMETYPEARRAY_IID);

//
// Navigator property ids
//
enum Navigator_slots {
  NAVIGATOR_APPCODENAME = -1,
  NAVIGATOR_APPNAME = -2,
  NAVIGATOR_APPVERSION = -3,
  NAVIGATOR_LANGUAGE = -4,
  NAVIGATOR_MIMETYPES = -5,
  NAVIGATOR_PLATFORM = -6,
  NAVIGATOR_OSCPU = -7,
  NAVIGATOR_VENDOR = -8,
  NAVIGATOR_VENDORSUB = -9,
  NAVIGATOR_PRODUCT = -10,
  NAVIGATOR_PRODUCTSUB = -11,
  NAVIGATOR_PLUGINS = -12,
  NAVIGATOR_SECURITYPOLICY = -13,
  NAVIGATOR_USERAGENT = -14,
  NAVIGATOR_COOKIEENABLED = -15
};

/***********************************************************************/
//
// Navigator Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetNavigatorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNavigator *a = (nsIDOMNavigator*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case NAVIGATOR_APPCODENAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_APPCODENAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAppCodeName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_APPNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_APPNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAppName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_APPVERSION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_APPVERSION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAppVersion(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_LANGUAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_LANGUAGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLanguage(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_MIMETYPES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_MIMETYPES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMMimeTypeArray* prop;
          rv = a->GetMimeTypes(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case NAVIGATOR_PLATFORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_PLATFORM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPlatform(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_OSCPU:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_OSCPU, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOscpu(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_VENDOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_VENDOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVendor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_VENDORSUB:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_VENDORSUB, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVendorSub(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_PRODUCT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_PRODUCT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetProduct(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_PRODUCTSUB:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_PRODUCTSUB, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetProductSub(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_PLUGINS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_PLUGINS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMPluginArray* prop;
          rv = a->GetPlugins(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case NAVIGATOR_SECURITYPOLICY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_SECURITYPOLICY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSecurityPolicy(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_USERAGENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_USERAGENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetUserAgent(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NAVIGATOR_COOKIEENABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_COOKIEENABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetCookieEnabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
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
// Navigator Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetNavigatorProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMNavigator *a = (nsIDOMNavigator*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case 0:
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
// Navigator class properties
//
static JSPropertySpec NavigatorProperties[] =
{
  {"appCodeName",    NAVIGATOR_APPCODENAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"appName",    NAVIGATOR_APPNAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"appVersion",    NAVIGATOR_APPVERSION,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"language",    NAVIGATOR_LANGUAGE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"mimeTypes",    NAVIGATOR_MIMETYPES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"platform",    NAVIGATOR_PLATFORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"oscpu",    NAVIGATOR_OSCPU,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"vendor",    NAVIGATOR_VENDOR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"vendorSub",    NAVIGATOR_VENDORSUB,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"product",    NAVIGATOR_PRODUCT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"productSub",    NAVIGATOR_PRODUCTSUB,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"plugins",    NAVIGATOR_PLUGINS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"securityPolicy",    NAVIGATOR_SECURITYPOLICY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"userAgent",    NAVIGATOR_USERAGENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cookieEnabled",    NAVIGATOR_COOKIEENABLED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Navigator finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeNavigator(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Navigator enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateNavigator(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// Navigator resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveNavigator(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method JavaEnabled
//
PR_STATIC_CALLBACK(JSBool)
NavigatorJavaEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNavigator *nativeThis = (nsIDOMNavigator*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRBool nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_JAVAENABLED, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->JavaEnabled(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method TaintEnabled
//
PR_STATIC_CALLBACK(JSBool)
NavigatorTaintEnabled(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNavigator *nativeThis = (nsIDOMNavigator*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRBool nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_TAINTENABLED, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->TaintEnabled(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method Preference
//
PR_STATIC_CALLBACK(JSBool)
NavigatorPreference(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMNavigator *nativeThis = (nsIDOMNavigator*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  jsval nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NAVIGATOR_PREFERENCE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Preference(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = nativeRet;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Navigator
//
JSClass NavigatorClass = {
  "Navigator", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetNavigatorProperty,
  SetNavigatorProperty,
  EnumerateNavigator,
  ResolveNavigator,
  JS_ConvertStub,
  FinalizeNavigator,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// Navigator class methods
//
static JSFunctionSpec NavigatorMethods[] = 
{
  {"javaEnabled",          NavigatorJavaEnabled,     0},
  {"taintEnabled",          NavigatorTaintEnabled,     0},
  {"preference",          NavigatorPreference,     0},
  {0}
};


//
// Navigator constructor
//
PR_STATIC_CALLBACK(JSBool)
Navigator(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Navigator class initialization
//
extern "C" NS_DOM nsresult NS_InitNavigatorClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Navigator", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &NavigatorClass,      // JSClass
                         Navigator,            // JSNative ctor
                         0,             // ctor args
                         NavigatorProperties,  // proto props
                         NavigatorMethods,     // proto funcs
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
// Method for creating a new Navigator JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptNavigator(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptNavigator");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMNavigator *aNavigator;

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

  if (NS_OK != NS_InitNavigatorClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kINavigatorIID, (void **)&aNavigator);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &NavigatorClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aNavigator);
  }
  else {
    NS_RELEASE(aNavigator);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
