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
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIDOMXULElement.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIRDFResource.h"
#include "nsIBoxObject.h"
#include "nsIControllers.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIXULElementIID, NS_IDOMXULELEMENT_IID);
static NS_DEFINE_IID(kIXULTemplateBuilderIID, NS_IXULTEMPLATEBUILDER_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIBoxObjectIID, NS_IBOXOBJECT_IID);
static NS_DEFINE_IID(kIControllersIID, NS_ICONTROLLERS_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

//
// XULElement property ids
//
enum XULElement_slots {
  XULELEMENT_ID = -1,
  XULELEMENT_CLASSNAME = -2,
  XULELEMENT_STYLE = -3,
  XULELEMENT_DATABASE = -4,
  XULELEMENT_BUILDER = -5,
  XULELEMENT_RESOURCE = -6,
  XULELEMENT_CONTROLLERS = -7,
  XULELEMENT_BOXOBJECT = -8
};

/***********************************************************************/
//
// XULElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULElement *a = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULELEMENT_ID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_ID, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetId(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULELEMENT_CLASSNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_CLASSNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetClassName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case XULELEMENT_STYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_STYLE, PR_FALSE);
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
      case XULELEMENT_DATABASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_DATABASE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIRDFCompositeDataSource* prop;
          rv = a->GetDatabase(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIRDFCompositeDataSource), cx, obj, vp);
          }
        }
        break;
      }
      case XULELEMENT_BUILDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_BUILDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIXULTemplateBuilder* prop;
          rv = a->GetBuilder(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIXULTemplateBuilder), cx, obj, vp);
          }
        }
        break;
      }
      case XULELEMENT_RESOURCE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_RESOURCE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIRDFResource* prop;
          rv = a->GetResource(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIRDFResource), cx, obj, vp);
          }
        }
        break;
      }
      case XULELEMENT_CONTROLLERS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_CONTROLLERS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIControllers* prop;
          rv = a->GetControllers(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIControllers), cx, obj, vp);
          }
        }
        break;
      }
      case XULELEMENT_BOXOBJECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_BOXOBJECT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIBoxObject* prop;
          rv = a->GetBoxObject(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIBoxObject), cx, obj, vp);
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
// XULElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULElement *a = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULELEMENT_ID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_ID, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetId(prop);
          
        }
        break;
      }
      case XULELEMENT_CLASSNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_CLASSNAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetClassName(prop);
          
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
// XULElement class properties
//
static JSPropertySpec XULElementProperties[] =
{
  {"id",    XULELEMENT_ID,    JSPROP_ENUMERATE},
  {"className",    XULELEMENT_CLASSNAME,    JSPROP_ENUMERATE},
  {"style",    XULELEMENT_STYLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"database",    XULELEMENT_DATABASE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"builder",    XULELEMENT_BUILDER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"resource",    XULELEMENT_RESOURCE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"controllers",    XULELEMENT_CONTROLLERS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"boxObject",    XULELEMENT_BOXOBJECT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// XULElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// XULElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method AddBroadcastListener
//
PR_STATIC_CALLBACK(JSBool)
XULElementAddBroadcastListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  nsCOMPtr<nsIDOMElement> b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_ADDBROADCASTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b1),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->AddBroadcastListener(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RemoveBroadcastListener
//
PR_STATIC_CALLBACK(JSBool)
XULElementRemoveBroadcastListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  nsCOMPtr<nsIDOMElement> b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_REMOVEBROADCASTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b1),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RemoveBroadcastListener(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method DoCommand
//
PR_STATIC_CALLBACK(JSBool)
XULElementDoCommand(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_DOCOMMAND, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DoCommand();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
XULElementFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_FOCUS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Focus();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
XULElementBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_BLUR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Blur();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Click
//
PR_STATIC_CALLBACK(JSBool)
XULElementClick(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_CLICK, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Click();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetElementsByAttribute
//
PR_STATIC_CALLBACK(JSBool)
XULElementGetElementsByAttribute(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULElement *nativeThis = (nsIDOMXULElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMNodeList* nativeRet;
  nsAutoString b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULELEMENT_GETELEMENTSBYATTRIBUTE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->GetElementsByAttribute(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULElement
//
JSClass XULElementClass = {
  "XULElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULElementProperty,
  SetXULElementProperty,
  EnumerateXULElement,
  ResolveXULElement,
  JS_ConvertStub,
  FinalizeXULElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULElement class methods
//
static JSFunctionSpec XULElementMethods[] = 
{
  {"addBroadcastListener",          XULElementAddBroadcastListener,     2},
  {"removeBroadcastListener",          XULElementRemoveBroadcastListener,     2},
  {"doCommand",          XULElementDoCommand,     0},
  {"focus",          XULElementFocus,     0},
  {"blur",          XULElementBlur,     0},
  {"click",          XULElementClick,     0},
  {"getElementsByAttribute",          XULElementGetElementsByAttribute,     2},
  {0}
};


//
// XULElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULElement", &vp)) ||
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
                         &XULElementClass,      // JSClass
                         XULElement,            // JSNative ctor
                         0,             // ctor args
                         XULElementProperties,  // proto props
                         XULElementMethods,     // proto funcs
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
// Method for creating a new XULElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULElement *aXULElement;

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

  if (NS_OK != NS_InitXULElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULElementIID, (void **)&aXULElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULElement);
  }
  else {
    NS_RELEASE(aXULElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
