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
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLLabelElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLLabelElementIID, NS_IDOMHTMLLABELELEMENT_IID);

//
// HTMLLabelElement property ids
//
enum HTMLLabelElement_slots {
  HTMLLABELELEMENT_FORM = -1,
  HTMLLABELELEMENT_ACCESSKEY = -2,
  HTMLLABELELEMENT_HTMLFOR = -3
};

/***********************************************************************/
//
// HTMLLabelElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLLabelElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLabelElement *a = (nsIDOMHTMLLabelElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLABELELEMENT_FORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLABELELEMENT_FORM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLFormElement* prop;
          rv = a->GetForm(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLLABELELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLABELELEMENT_ACCESSKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAccessKey(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLABELELEMENT_HTMLFOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLABELELEMENT_HTMLFOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHtmlFor(prop);
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
// HTMLLabelElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLLabelElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLabelElement *a = (nsIDOMHTMLLabelElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLABELELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLABELELEMENT_ACCESSKEY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAccessKey(prop);
          
        }
        break;
      }
      case HTMLLABELELEMENT_HTMLFOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLABELELEMENT_HTMLFOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHtmlFor(prop);
          
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
// HTMLLabelElement class properties
//
static JSPropertySpec HTMLLabelElementProperties[] =
{
  {"form",    HTMLLABELELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"accessKey",    HTMLLABELELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"htmlFor",    HTMLLABELELEMENT_HTMLFOR,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLLabelElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLLabelElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLLabelElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLLabelElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLLabelElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLLabelElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLLabelElement
//
JSClass HTMLLabelElementClass = {
  "HTMLLabelElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLLabelElementProperty,
  SetHTMLLabelElementProperty,
  EnumerateHTMLLabelElement,
  ResolveHTMLLabelElement,
  JS_ConvertStub,
  FinalizeHTMLLabelElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLLabelElement class methods
//
static JSFunctionSpec HTMLLabelElementMethods[] = 
{
  {0}
};


//
// HTMLLabelElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLLabelElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLLabelElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLLabelElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLLabelElement", &vp)) ||
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
                         &HTMLLabelElementClass,      // JSClass
                         HTMLLabelElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLLabelElementProperties,  // proto props
                         HTMLLabelElementMethods,     // proto funcs
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
// Method for creating a new HTMLLabelElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLLabelElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLLabelElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLLabelElement *aHTMLLabelElement;

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

  if (NS_OK != NS_InitHTMLLabelElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLLabelElementIID, (void **)&aHTMLLabelElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLLabelElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLLabelElement);
  }
  else {
    NS_RELEASE(aHTMLLabelElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
