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
#include "nsIDOMWindowCollection.h"
#include "nsIDOMWindow.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

//
// WindowCollection property ids
//
enum WindowCollection_slots {
  WINDOWCOLLECTION_LENGTH = -1
};

/***********************************************************************/
//
// WindowCollection Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowCollectionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowCollection *a = (nsIDOMWindowCollection*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case WINDOWCOLLECTION_LENGTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWCOLLECTION_LENGTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetLength(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      default:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWCOLLECTION_ITEM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->Item(JSVAL_TO_INT(id), &prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
      }
    }
  }

  if (checkNamedItem) {
    nsIDOMWindow* prop;
    nsAutoString name;
    nsresult result = NS_OK;

    JSString *jsstring = JS_ValueToString(cx, id);
    if (nsnull != jsstring) {
      name.Assign(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(jsstring)));
    }
    else {
      name.SetLength(0);
    }

    result = a->NamedItem(name, &prop);
    if (NS_SUCCEEDED(result)) {
      if (NULL != prop) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
      }
      else {
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
      }
    }
    else {
      return nsJSUtils::nsReportError(cx, obj, result);
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
// WindowCollection Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetWindowCollectionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowCollection *a = (nsIDOMWindowCollection*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
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
// WindowCollection finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeWindowCollection(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// WindowCollection enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateWindowCollection(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// WindowCollection resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveWindowCollection(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
WindowCollectionItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindowCollection *nativeThis = (nsIDOMWindowCollection*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMWindow* nativeRet;
  PRUint32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWCOLLECTION_ITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->Item(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method NamedItem
//
PR_STATIC_CALLBACK(JSBool)
WindowCollectionNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindowCollection *nativeThis = (nsIDOMWindowCollection*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMWindow* nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWCOLLECTION_NAMEDITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->NamedItem(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for WindowCollection
//
JSClass WindowCollectionClass = {
  "WindowCollection", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetWindowCollectionProperty,
  SetWindowCollectionProperty,
  EnumerateWindowCollection,
  ResolveWindowCollection,
  JS_ConvertStub,
  FinalizeWindowCollection,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// WindowCollection class properties
//
static JSPropertySpec WindowCollectionProperties[] =
{
  {"length",    WINDOWCOLLECTION_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// WindowCollection class methods
//
static JSFunctionSpec WindowCollectionMethods[] = 
{
  {"item",          WindowCollectionItem,     1},
  {"namedItem",          WindowCollectionNamedItem,     1},
  {0}
};


//
// WindowCollection constructor
//
PR_STATIC_CALLBACK(JSBool)
WindowCollection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// WindowCollection class initialization
//
extern "C" NS_DOM nsresult NS_InitWindowCollectionClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "WindowCollection", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &WindowCollectionClass,      // JSClass
                         WindowCollection,            // JSNative ctor
                         0,             // ctor args
                         WindowCollectionProperties,  // proto props
                         WindowCollectionMethods,     // proto funcs
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
// Method for creating a new WindowCollection JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptWindowCollection(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptWindowCollection");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMWindowCollection *aWindowCollection;

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

  if (NS_OK != NS_InitWindowCollectionClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIWindowCollectionIID, (void **)&aWindowCollection);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &WindowCollectionClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aWindowCollection);
  }
  else {
    NS_RELEASE(aWindowCollection);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
