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
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMCSSStyleDeclaration.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);

//
// HTMLElement property ids
//
enum HTMLElement_slots {
  HTMLELEMENT_ID = -1,
  HTMLELEMENT_TITLE = -2,
  HTMLELEMENT_LANG = -3,
  HTMLELEMENT_DIR = -4,
  HTMLELEMENT_CLASSNAME = -5,
  HTMLELEMENT_STYLE = -6,
  HTMLELEMENT_OFFSETTOP = -7,
  HTMLELEMENT_OFFSETLEFT = -8,
  HTMLELEMENT_OFFSETWIDTH = -9,
  HTMLELEMENT_OFFSETHEIGHT = -10,
  HTMLELEMENT_OFFSETPARENT = -11,
  HTMLELEMENT_INNERHTML = -12
};

/***********************************************************************/
//
// HTMLElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLElement *a = (nsIDOMHTMLElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLELEMENT_ID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_ID, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetId(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_TITLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTitle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_LANG:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_LANG, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLang(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_DIR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_DIR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDir(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_CLASSNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_CLASSNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetClassName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_STYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_STYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCSSStyleDeclaration* prop;
          rv = a->GetStyle(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_OFFSETTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_OFFSETTOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOffsetTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLELEMENT_OFFSETLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_OFFSETLEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOffsetLeft(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLELEMENT_OFFSETWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_OFFSETWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOffsetWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLELEMENT_OFFSETHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_OFFSETHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOffsetHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLELEMENT_OFFSETPARENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_OFFSETPARENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMElement* prop;
          rv = a->GetOffsetParent(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLELEMENT_INNERHTML:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_INNERHTML, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetInnerHTML(prop);
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
// HTMLElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLElement *a = (nsIDOMHTMLElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLELEMENT_ID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_ID, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetId(prop);
          
        }
        break;
      }
      case HTMLELEMENT_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_TITLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTitle(prop);
          
        }
        break;
      }
      case HTMLELEMENT_LANG:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_LANG, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLang(prop);
          
        }
        break;
      }
      case HTMLELEMENT_DIR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_DIR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDir(prop);
          
        }
        break;
      }
      case HTMLELEMENT_CLASSNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_CLASSNAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetClassName(prop);
          
        }
        break;
      }
      case HTMLELEMENT_INNERHTML:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLELEMENT_INNERHTML, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetInnerHTML(prop);
          
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
// HTMLElement class properties
//
static JSPropertySpec HTMLElementProperties[] =
{
  {"id",    HTMLELEMENT_ID,    JSPROP_ENUMERATE},
  {"title",    HTMLELEMENT_TITLE,    JSPROP_ENUMERATE},
  {"lang",    HTMLELEMENT_LANG,    JSPROP_ENUMERATE},
  {"dir",    HTMLELEMENT_DIR,    JSPROP_ENUMERATE},
  {"className",    HTMLELEMENT_CLASSNAME,    JSPROP_ENUMERATE},
  {"style",    HTMLELEMENT_STYLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"offsetTop",    HTMLELEMENT_OFFSETTOP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"offsetLeft",    HTMLELEMENT_OFFSETLEFT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"offsetWidth",    HTMLELEMENT_OFFSETWIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"offsetHeight",    HTMLELEMENT_OFFSETHEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"offsetParent",    HTMLELEMENT_OFFSETPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"innerHTML",    HTMLELEMENT_INNERHTML,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLElement
//
JSClass HTMLElementClass = {
  "HTMLElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLElementProperty,
  SetHTMLElementProperty,
  EnumerateHTMLElement,
  ResolveHTMLElement,
  JS_ConvertStub,
  FinalizeHTMLElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLElement class methods
//
static JSFunctionSpec HTMLElementMethods[] = 
{
  {0}
};


//
// HTMLElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLElementClass,      // JSClass
                         HTMLElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLElementProperties,  // proto props
                         HTMLElementMethods,     // proto funcs
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
// Method for creating a new HTMLElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLElement *aHTMLElement;

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

  if (NS_OK != NS_InitHTMLElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLElementIID, (void **)&aHTMLElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLElement);
  }
  else {
    NS_RELEASE(aHTMLElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
