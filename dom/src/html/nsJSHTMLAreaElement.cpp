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
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMNSHTMLAreaElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLAreaElementIID, NS_IDOMHTMLAREAELEMENT_IID);
static NS_DEFINE_IID(kINSHTMLAreaElementIID, NS_IDOMNSHTMLAREAELEMENT_IID);

//
// HTMLAreaElement property ids
//
enum HTMLAreaElement_slots {
  HTMLAREAELEMENT_ACCESSKEY = -1,
  HTMLAREAELEMENT_ALT = -2,
  HTMLAREAELEMENT_COORDS = -3,
  HTMLAREAELEMENT_HREF = -4,
  HTMLAREAELEMENT_NOHREF = -5,
  HTMLAREAELEMENT_SHAPE = -6,
  HTMLAREAELEMENT_TABINDEX = -7,
  HTMLAREAELEMENT_TARGET = -8,
  NSHTMLAREAELEMENT_PROTOCOL = -9,
  NSHTMLAREAELEMENT_HOST = -10,
  NSHTMLAREAELEMENT_HOSTNAME = -11,
  NSHTMLAREAELEMENT_PATHNAME = -12,
  NSHTMLAREAELEMENT_SEARCH = -13,
  NSHTMLAREAELEMENT_PORT = -14,
  NSHTMLAREAELEMENT_HASH = -15
};

/***********************************************************************/
//
// HTMLAreaElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLAreaElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLAreaElement *a = (nsIDOMHTMLAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLAREAELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_ACCESSKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAccessKey(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_ALT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlt(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_COORDS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_COORDS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCoords(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_HREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_HREF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHref(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_NOHREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_NOHREF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetNoHref(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_SHAPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_SHAPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetShape(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_TABINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetTabIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLAREAELEMENT_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_TARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTarget(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_PROTOCOL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_PROTOCOL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetProtocol(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_HOST:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_HOST, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetHost(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_HOSTNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_HOSTNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetHostname(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_PATHNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_PATHNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetPathname(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_SEARCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_SEARCH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetSearch(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_PORT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_PORT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetPort(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSHTMLAREAELEMENT_HASH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLAREAELEMENT_HASH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLAreaElementIID, (void **)&b)) {
            rv = b->GetHash(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
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
// HTMLAreaElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLAreaElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLAreaElement *a = (nsIDOMHTMLAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLAREAELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_ACCESSKEY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAccessKey(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_ALT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlt(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_COORDS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_COORDS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCoords(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_HREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_HREF, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHref(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_NOHREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_NOHREF, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetNoHref(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_SHAPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_SHAPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetShape(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_TABINDEX, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          rv = a->SetTabIndex(prop);
          
        }
        break;
      }
      case HTMLAREAELEMENT_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAREAELEMENT_TARGET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTarget(prop);
          
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
// HTMLAreaElement class properties
//
static JSPropertySpec HTMLAreaElementProperties[] =
{
  {"accessKey",    HTMLAREAELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"alt",    HTMLAREAELEMENT_ALT,    JSPROP_ENUMERATE},
  {"coords",    HTMLAREAELEMENT_COORDS,    JSPROP_ENUMERATE},
  {"href",    HTMLAREAELEMENT_HREF,    JSPROP_ENUMERATE},
  {"noHref",    HTMLAREAELEMENT_NOHREF,    JSPROP_ENUMERATE},
  {"shape",    HTMLAREAELEMENT_SHAPE,    JSPROP_ENUMERATE},
  {"tabIndex",    HTMLAREAELEMENT_TABINDEX,    JSPROP_ENUMERATE},
  {"target",    HTMLAREAELEMENT_TARGET,    JSPROP_ENUMERATE},
  {"protocol",    NSHTMLAREAELEMENT_PROTOCOL,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"host",    NSHTMLAREAELEMENT_HOST,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"hostname",    NSHTMLAREAELEMENT_HOSTNAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pathname",    NSHTMLAREAELEMENT_PATHNAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"search",    NSHTMLAREAELEMENT_SEARCH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"port",    NSHTMLAREAELEMENT_PORT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"hash",    NSHTMLAREAELEMENT_HASH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLAreaElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLAreaElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLAreaElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLAreaElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLAreaElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLAreaElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLAreaElement
//
JSClass HTMLAreaElementClass = {
  "HTMLAreaElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLAreaElementProperty,
  SetHTMLAreaElementProperty,
  EnumerateHTMLAreaElement,
  ResolveHTMLAreaElement,
  JS_ConvertStub,
  FinalizeHTMLAreaElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLAreaElement class methods
//
static JSFunctionSpec HTMLAreaElementMethods[] = 
{
  {0}
};


//
// HTMLAreaElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLAreaElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLAreaElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLAreaElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLAreaElement", &vp)) ||
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
                         &HTMLAreaElementClass,      // JSClass
                         HTMLAreaElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLAreaElementProperties,  // proto props
                         HTMLAreaElementMethods,     // proto funcs
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
// Method for creating a new HTMLAreaElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLAreaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLAreaElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLAreaElement *aHTMLAreaElement;

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

  if (NS_OK != NS_InitHTMLAreaElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLAreaElementIID, (void **)&aHTMLAreaElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLAreaElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLAreaElement);
  }
  else {
    NS_RELEASE(aHTMLAreaElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
