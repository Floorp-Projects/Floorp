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
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLIFrameElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLIFrameElementIID, NS_IDOMHTMLIFRAMEELEMENT_IID);

//
// HTMLIFrameElement property ids
//
enum HTMLIFrameElement_slots {
  HTMLIFRAMEELEMENT_ALIGN = -1,
  HTMLIFRAMEELEMENT_FRAMEBORDER = -2,
  HTMLIFRAMEELEMENT_HEIGHT = -3,
  HTMLIFRAMEELEMENT_LONGDESC = -4,
  HTMLIFRAMEELEMENT_MARGINHEIGHT = -5,
  HTMLIFRAMEELEMENT_MARGINWIDTH = -6,
  HTMLIFRAMEELEMENT_NAME = -7,
  HTMLIFRAMEELEMENT_SCROLLING = -8,
  HTMLIFRAMEELEMENT_SRC = -9,
  HTMLIFRAMEELEMENT_WIDTH = -10,
  HTMLIFRAMEELEMENT_CONTENTDOCUMENT = -11
};

/***********************************************************************/
//
// HTMLIFrameElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLIFrameElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLIFrameElement *a = (nsIDOMHTMLIFrameElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIFRAMEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_FRAMEBORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_FRAMEBORDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFrameBorder(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_LONGDESC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_LONGDESC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLongDesc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_MARGINHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_MARGINWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_SCROLLING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_SCROLLING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetScrolling(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_SRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSrc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIFRAMEELEMENT_CONTENTDOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_CONTENTDOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          rv = a->GetContentDocument(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
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
// HTMLIFrameElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLIFrameElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLIFrameElement *a = (nsIDOMHTMLIFrameElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIFRAMEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_FRAMEBORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_FRAMEBORDER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFrameBorder(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_LONGDESC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_LONGDESC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLongDesc(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_MARGINHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginHeight(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_MARGINWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginWidth(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_SCROLLING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_SCROLLING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetScrolling(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_SRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSrc(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_WIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidth(prop);
          
        }
        break;
      }
      case HTMLIFRAMEELEMENT_CONTENTDOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIFRAMEELEMENT_CONTENTDOCUMENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIDocumentIID, NS_ConvertASCIItoUCS2("Document"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetContentDocument(prop);
          NS_IF_RELEASE(prop);
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
// HTMLIFrameElement class properties
//
static JSPropertySpec HTMLIFrameElementProperties[] =
{
  {"align",    HTMLIFRAMEELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"frameBorder",    HTMLIFRAMEELEMENT_FRAMEBORDER,    JSPROP_ENUMERATE},
  {"height",    HTMLIFRAMEELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"longDesc",    HTMLIFRAMEELEMENT_LONGDESC,    JSPROP_ENUMERATE},
  {"marginHeight",    HTMLIFRAMEELEMENT_MARGINHEIGHT,    JSPROP_ENUMERATE},
  {"marginWidth",    HTMLIFRAMEELEMENT_MARGINWIDTH,    JSPROP_ENUMERATE},
  {"name",    HTMLIFRAMEELEMENT_NAME,    JSPROP_ENUMERATE},
  {"scrolling",    HTMLIFRAMEELEMENT_SCROLLING,    JSPROP_ENUMERATE},
  {"src",    HTMLIFRAMEELEMENT_SRC,    JSPROP_ENUMERATE},
  {"width",    HTMLIFRAMEELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {"contentDocument",    HTMLIFRAMEELEMENT_CONTENTDOCUMENT,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLIFrameElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLIFrameElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLIFrameElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLIFrameElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLIFrameElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLIFrameElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLIFrameElement
//
JSClass HTMLIFrameElementClass = {
  "HTMLIFrameElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLIFrameElementProperty,
  SetHTMLIFrameElementProperty,
  EnumerateHTMLIFrameElement,
  ResolveHTMLIFrameElement,
  JS_ConvertStub,
  FinalizeHTMLIFrameElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLIFrameElement class methods
//
static JSFunctionSpec HTMLIFrameElementMethods[] = 
{
  {0}
};


//
// HTMLIFrameElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLIFrameElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLIFrameElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLIFrameElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLIFrameElement", &vp)) ||
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
                         &HTMLIFrameElementClass,      // JSClass
                         HTMLIFrameElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLIFrameElementProperties,  // proto props
                         HTMLIFrameElementMethods,     // proto funcs
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
// Method for creating a new HTMLIFrameElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLIFrameElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLIFrameElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLIFrameElement *aHTMLIFrameElement;

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

  if (NS_OK != NS_InitHTMLIFrameElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLIFrameElementIID, (void **)&aHTMLIFrameElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLIFrameElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLIFrameElement);
  }
  else {
    NS_RELEASE(aHTMLIFrameElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
