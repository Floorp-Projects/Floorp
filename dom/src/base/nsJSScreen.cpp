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
#include "nsIDOMScreen.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIScreenIID, NS_IDOMSCREEN_IID);

//
// Screen property ids
//
enum Screen_slots {
  SCREEN_TOP = -1,
  SCREEN_LEFT = -2,
  SCREEN_WIDTH = -3,
  SCREEN_HEIGHT = -4,
  SCREEN_PIXELDEPTH = -5,
  SCREEN_COLORDEPTH = -6,
  SCREEN_AVAILWIDTH = -7,
  SCREEN_AVAILHEIGHT = -8,
  SCREEN_AVAILLEFT = -9,
  SCREEN_AVAILTOP = -10
};

/***********************************************************************/
//
// Screen Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetScreenProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMScreen *a = (nsIDOMScreen*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case SCREEN_TOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_TOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_LEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_LEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetLeft(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_PIXELDEPTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_PIXELDEPTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetPixelDepth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_COLORDEPTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_COLORDEPTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetColorDepth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_AVAILWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_AVAILWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetAvailWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_AVAILHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_AVAILHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetAvailHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_AVAILLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_AVAILLEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetAvailLeft(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case SCREEN_AVAILTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SCREEN_AVAILTOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetAvailTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
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
// Screen Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetScreenProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMScreen *a = (nsIDOMScreen*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// Screen class properties
//
static JSPropertySpec ScreenProperties[] =
{
  {"top",    SCREEN_TOP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"left",    SCREEN_LEFT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"width",    SCREEN_WIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"height",    SCREEN_HEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pixelDepth",    SCREEN_PIXELDEPTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"colorDepth",    SCREEN_COLORDEPTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"availWidth",    SCREEN_AVAILWIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"availHeight",    SCREEN_AVAILHEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"availLeft",    SCREEN_AVAILLEFT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"availTop",    SCREEN_AVAILTOP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Screen finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeScreen(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Screen enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateScreen(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// Screen resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveScreen(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for Screen
//
JSClass ScreenClass = {
  "Screen", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetScreenProperty,
  SetScreenProperty,
  EnumerateScreen,
  ResolveScreen,
  JS_ConvertStub,
  FinalizeScreen,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// Screen class methods
//
static JSFunctionSpec ScreenMethods[] = 
{
  {0}
};


//
// Screen constructor
//
PR_STATIC_CALLBACK(JSBool)
Screen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Screen class initialization
//
extern "C" NS_DOM nsresult NS_InitScreenClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Screen", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &ScreenClass,      // JSClass
                         Screen,            // JSNative ctor
                         0,             // ctor args
                         ScreenProperties,  // proto props
                         ScreenMethods,     // proto funcs
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
// Method for creating a new Screen JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptScreen(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptScreen");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMScreen *aScreen;

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

  if (NS_OK != NS_InitScreenClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIScreenIID, (void **)&aScreen);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ScreenClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aScreen);
  }
  else {
    NS_RELEASE(aScreen);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
