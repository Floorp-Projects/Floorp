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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMLinkStyle.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLLinkElementIID, NS_IDOMHTMLLINKELEMENT_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kILinkStyleIID, NS_IDOMLINKSTYLE_IID);

//
// HTMLLinkElement property ids
//
enum HTMLLinkElement_slots {
  HTMLLINKELEMENT_DISABLED = -1,
  HTMLLINKELEMENT_CHARSET = -2,
  HTMLLINKELEMENT_HREF = -3,
  HTMLLINKELEMENT_HREFLANG = -4,
  HTMLLINKELEMENT_MEDIA = -5,
  HTMLLINKELEMENT_REL = -6,
  HTMLLINKELEMENT_REV = -7,
  HTMLLINKELEMENT_TARGET = -8,
  HTMLLINKELEMENT_TYPE = -9,
  LINKSTYLE_SHEET = -10
};

/***********************************************************************/
//
// HTMLLinkElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLLinkElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLINKELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_CHARSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_CHARSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCharset(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_HREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_HREF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHref(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_HREFLANG, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHreflang(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_MEDIA, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMedia(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_REL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetRel(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_REV, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetRev(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_TARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTarget(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLLINKELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LINKSTYLE_SHEET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LINKSTYLE_SHEET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMStyleSheet* prop;
          nsIDOMLinkStyle* b;
          if (NS_OK == a->QueryInterface(kILinkStyleIID, (void **)&b)) {
            rv = b->GetSheet(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
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
// HTMLLinkElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLLinkElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLINKELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_DISABLED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetDisabled(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_CHARSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_CHARSET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCharset(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_HREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_HREF, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHref(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_HREFLANG, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHreflang(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_MEDIA, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMedia(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_REL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetRel(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_REV, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetRev(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_TARGET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTarget(prop);
          
        }
        break;
      }
      case HTMLLINKELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLLINKELEMENT_TYPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetType(prop);
          
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
// HTMLLinkElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLLinkElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLLinkElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLLinkElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLLinkElement
//
JSClass HTMLLinkElementClass = {
  "HTMLLinkElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLLinkElementProperty,
  SetHTMLLinkElementProperty,
  EnumerateHTMLLinkElement,
  ResolveHTMLLinkElement,
  JS_ConvertStub,
  FinalizeHTMLLinkElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLLinkElement class properties
//
static JSPropertySpec HTMLLinkElementProperties[] =
{
  {"disabled",    HTMLLINKELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"charset",    HTMLLINKELEMENT_CHARSET,    JSPROP_ENUMERATE},
  {"href",    HTMLLINKELEMENT_HREF,    JSPROP_ENUMERATE},
  {"hreflang",    HTMLLINKELEMENT_HREFLANG,    JSPROP_ENUMERATE},
  {"media",    HTMLLINKELEMENT_MEDIA,    JSPROP_ENUMERATE},
  {"rel",    HTMLLINKELEMENT_REL,    JSPROP_ENUMERATE},
  {"rev",    HTMLLINKELEMENT_REV,    JSPROP_ENUMERATE},
  {"target",    HTMLLINKELEMENT_TARGET,    JSPROP_ENUMERATE},
  {"type",    HTMLLINKELEMENT_TYPE,    JSPROP_ENUMERATE},
  {"sheet",    LINKSTYLE_SHEET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLLinkElement class methods
//
static JSFunctionSpec HTMLLinkElementMethods[] = 
{
  {0}
};


//
// HTMLLinkElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLLinkElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLLinkElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLLinkElement", &vp)) ||
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
                         &HTMLLinkElementClass,      // JSClass
                         HTMLLinkElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLLinkElementProperties,  // proto props
                         HTMLLinkElementMethods,     // proto funcs
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
// Method for creating a new HTMLLinkElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLLinkElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLLinkElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLLinkElement *aHTMLLinkElement;

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

  if (NS_OK != NS_InitHTMLLinkElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLLinkElementIID, (void **)&aHTMLLinkElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLLinkElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLLinkElement);
  }
  else {
    NS_RELEASE(aHTMLLinkElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
