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
#include "nsIDOMHTMLAppletElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLAppletElementIID, NS_IDOMHTMLAPPLETELEMENT_IID);

//
// HTMLAppletElement property ids
//
enum HTMLAppletElement_slots {
  HTMLAPPLETELEMENT_ALIGN = -1,
  HTMLAPPLETELEMENT_ALT = -2,
  HTMLAPPLETELEMENT_ARCHIVE = -3,
  HTMLAPPLETELEMENT_CODE = -4,
  HTMLAPPLETELEMENT_CODEBASE = -5,
  HTMLAPPLETELEMENT_HEIGHT = -6,
  HTMLAPPLETELEMENT_HSPACE = -7,
  HTMLAPPLETELEMENT_NAME = -8,
  HTMLAPPLETELEMENT_OBJECT = -9,
  HTMLAPPLETELEMENT_VSPACE = -10,
  HTMLAPPLETELEMENT_WIDTH = -11
};

/***********************************************************************/
//
// HTMLAppletElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLAppletElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLAppletElement *a = (nsIDOMHTMLAppletElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLAPPLETELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ALT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlt(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_ARCHIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ARCHIVE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetArchive(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_CODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_CODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCode(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_CODEBASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_CODEBASE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCodeBase(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_HSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHspace(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_OBJECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_OBJECT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetObject(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_VSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVspace(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLAPPLETELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_WIDTH, PR_FALSE);
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
// HTMLAppletElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLAppletElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLAppletElement *a = (nsIDOMHTMLAppletElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLAPPLETELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ALT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlt(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_ARCHIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_ARCHIVE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetArchive(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_CODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_CODE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCode(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_CODEBASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_CODEBASE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCodeBase(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_HSPACE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHspace(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_OBJECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_OBJECT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetObject(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_VSPACE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVspace(prop);
          
        }
        break;
      }
      case HTMLAPPLETELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLAPPLETELEMENT_WIDTH, PR_TRUE);
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
// HTMLAppletElement class properties
//
static JSPropertySpec HTMLAppletElementProperties[] =
{
  {"align",    HTMLAPPLETELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"alt",    HTMLAPPLETELEMENT_ALT,    JSPROP_ENUMERATE},
  {"archive",    HTMLAPPLETELEMENT_ARCHIVE,    JSPROP_ENUMERATE},
  {"code",    HTMLAPPLETELEMENT_CODE,    JSPROP_ENUMERATE},
  {"codeBase",    HTMLAPPLETELEMENT_CODEBASE,    JSPROP_ENUMERATE},
  {"height",    HTMLAPPLETELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"hspace",    HTMLAPPLETELEMENT_HSPACE,    JSPROP_ENUMERATE},
  {"name",    HTMLAPPLETELEMENT_NAME,    JSPROP_ENUMERATE},
  {"object",    HTMLAPPLETELEMENT_OBJECT,    JSPROP_ENUMERATE},
  {"vspace",    HTMLAPPLETELEMENT_VSPACE,    JSPROP_ENUMERATE},
  {"width",    HTMLAPPLETELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLAppletElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLAppletElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLAppletElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLAppletElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLAppletElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLAppletElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLAppletElement
//
JSClass HTMLAppletElementClass = {
  "HTMLAppletElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLAppletElementProperty,
  SetHTMLAppletElementProperty,
  EnumerateHTMLAppletElement,
  ResolveHTMLAppletElement,
  JS_ConvertStub,
  FinalizeHTMLAppletElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLAppletElement class methods
//
static JSFunctionSpec HTMLAppletElementMethods[] = 
{
  {0}
};


//
// HTMLAppletElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLAppletElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLAppletElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLAppletElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLAppletElement", &vp)) ||
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
                         &HTMLAppletElementClass,      // JSClass
                         HTMLAppletElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLAppletElementProperties,  // proto props
                         HTMLAppletElementMethods,     // proto funcs
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
// Method for creating a new HTMLAppletElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLAppletElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLAppletElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLAppletElement *aHTMLAppletElement;

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

  if (NS_OK != NS_InitHTMLAppletElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLAppletElementIID, (void **)&aHTMLAppletElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLAppletElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLAppletElement);
  }
  else {
    NS_RELEASE(aHTMLAppletElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
