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
#include "nsIDOMHTMLHRElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLHRElementIID, NS_IDOMHTMLHRELEMENT_IID);

//
// HTMLHRElement property ids
//
enum HTMLHRElement_slots {
  HTMLHRELEMENT_ALIGN = -1,
  HTMLHRELEMENT_NOSHADE = -2,
  HTMLHRELEMENT_SIZE = -3,
  HTMLHRELEMENT_WIDTH = -4
};

/***********************************************************************/
//
// HTMLHRElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLHRElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLHRElement *a = (nsIDOMHTMLHRElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLHRELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLHRELEMENT_NOSHADE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_NOSHADE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetNoShade(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLHRELEMENT_SIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_SIZE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSize(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLHRELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidth(prop);
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
// HTMLHRElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLHRElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLHRElement *a = (nsIDOMHTMLHRElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLHRELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLHRELEMENT_NOSHADE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_NOSHADE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetNoShade(prop);
          
        }
        break;
      }
      case HTMLHRELEMENT_SIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_SIZE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSize(prop);
          
        }
        break;
      }
      case HTMLHRELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLHRELEMENT_WIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidth(prop);
          
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
// HTMLHRElement class properties
//
static JSPropertySpec HTMLHRElementProperties[] =
{
  {"align",    HTMLHRELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"noShade",    HTMLHRELEMENT_NOSHADE,    JSPROP_ENUMERATE},
  {"size",    HTMLHRELEMENT_SIZE,    JSPROP_ENUMERATE},
  {"width",    HTMLHRELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLHRElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLHRElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLHRElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLHRElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLHRElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLHRElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLHRElement
//
JSClass HTMLHRElementClass = {
  "HTMLHRElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLHRElementProperty,
  SetHTMLHRElementProperty,
  EnumerateHTMLHRElement,
  ResolveHTMLHRElement,
  JS_ConvertStub,
  FinalizeHTMLHRElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLHRElement class methods
//
static JSFunctionSpec HTMLHRElementMethods[] = 
{
  {0}
};


//
// HTMLHRElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLHRElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLHRElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLHRElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLHRElement", &vp)) ||
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
                         &HTMLHRElementClass,      // JSClass
                         HTMLHRElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLHRElementProperties,  // proto props
                         HTMLHRElementMethods,     // proto funcs
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
// Method for creating a new HTMLHRElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLHRElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLHRElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLHRElement *aHTMLHRElement;

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

  if (NS_OK != NS_InitHTMLHRElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLHRElementIID, (void **)&aHTMLHRElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLHRElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLHRElement);
  }
  else {
    NS_RELEASE(aHTMLHRElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
