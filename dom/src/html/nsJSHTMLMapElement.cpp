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
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

//
// HTMLMapElement property ids
//
enum HTMLMapElement_slots {
  HTMLMAPELEMENT_AREAS = -1,
  HTMLMAPELEMENT_NAME = -2
};

/***********************************************************************/
//
// HTMLMapElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLMapElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLMapElement *a = (nsIDOMHTMLMapElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLMAPELEMENT_AREAS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLMAPELEMENT_AREAS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetAreas(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLMAPELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLMAPELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
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
// HTMLMapElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLMapElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLMapElement *a = (nsIDOMHTMLMapElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLMAPELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLMAPELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
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
// HTMLMapElement class properties
//
static JSPropertySpec HTMLMapElementProperties[] =
{
  {"areas",    HTMLMAPELEMENT_AREAS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    HTMLMAPELEMENT_NAME,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLMapElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLMapElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLMapElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLMapElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLMapElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLMapElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLMapElement
//
JSClass HTMLMapElementClass = {
  "HTMLMapElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLMapElementProperty,
  SetHTMLMapElementProperty,
  EnumerateHTMLMapElement,
  ResolveHTMLMapElement,
  JS_ConvertStub,
  FinalizeHTMLMapElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLMapElement class methods
//
static JSFunctionSpec HTMLMapElementMethods[] = 
{
  {0}
};


//
// HTMLMapElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLMapElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLMapElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLMapElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLMapElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitHTMLElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLMapElementClass,      // JSClass
                         HTMLMapElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLMapElementProperties,  // proto props
                         HTMLMapElementMethods,     // proto funcs
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
// Method for creating a new HTMLMapElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLMapElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLMapElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLMapElement *aHTMLMapElement;

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

  if (NS_OK != NS_InitHTMLMapElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLMapElementIID, (void **)&aHTMLMapElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLMapElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLMapElement);
  }
  else {
    NS_RELEASE(aHTMLMapElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
