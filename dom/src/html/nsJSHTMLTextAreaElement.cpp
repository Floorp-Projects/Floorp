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
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIControllers.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kINSHTMLTextAreaElementIID, NS_IDOMNSHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIControllersIID, NS_ICONTROLLERS_IID);

//
// HTMLTextAreaElement property ids
//
enum HTMLTextAreaElement_slots {
  HTMLTEXTAREAELEMENT_DEFAULTVALUE = -1,
  HTMLTEXTAREAELEMENT_FORM = -2,
  HTMLTEXTAREAELEMENT_ACCESSKEY = -3,
  HTMLTEXTAREAELEMENT_COLS = -4,
  HTMLTEXTAREAELEMENT_DISABLED = -5,
  HTMLTEXTAREAELEMENT_NAME = -6,
  HTMLTEXTAREAELEMENT_READONLY = -7,
  HTMLTEXTAREAELEMENT_ROWS = -8,
  HTMLTEXTAREAELEMENT_TABINDEX = -9,
  HTMLTEXTAREAELEMENT_TYPE = -10,
  HTMLTEXTAREAELEMENT_VALUE = -11,
  NSHTMLTEXTAREAELEMENT_CONTROLLERS = -12
};

/***********************************************************************/
//
// HTMLTextAreaElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTextAreaElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTextAreaElement *a = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTEXTAREAELEMENT_DEFAULTVALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_DEFAULTVALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDefaultValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_FORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_FORM, PR_FALSE);
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
      case HTMLTEXTAREAELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_ACCESSKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAccessKey(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_COLS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_COLS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetCols(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_READONLY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_READONLY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetReadOnly(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_ROWS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_ROWS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetRows(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_TABINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetTabIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_VALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NSHTMLTEXTAREAELEMENT_CONTROLLERS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLTEXTAREAELEMENT_CONTROLLERS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIControllers* prop;
          nsIDOMNSHTMLTextAreaElement* b;
          if (NS_OK == a->QueryInterface(kINSHTMLTextAreaElementIID, (void **)&b)) {
            rv = b->GetControllers(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIControllers), cx, obj, vp);
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
// HTMLTextAreaElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTextAreaElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTextAreaElement *a = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTEXTAREAELEMENT_DEFAULTVALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_DEFAULTVALUE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDefaultValue(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_ACCESSKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_ACCESSKEY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAccessKey(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_COLS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_COLS, PR_TRUE);
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
      
          rv = a->SetCols(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_DISABLED, PR_TRUE);
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
      case HTMLTEXTAREAELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_READONLY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_READONLY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetReadOnly(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_ROWS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_ROWS, PR_TRUE);
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
      
          rv = a->SetRows(prop);
          
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_TABINDEX, PR_TRUE);
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
      case HTMLTEXTAREAELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_VALUE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetValue(prop);
          
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
// HTMLTextAreaElement class properties
//
static JSPropertySpec HTMLTextAreaElementProperties[] =
{
  {"defaultValue",    HTMLTEXTAREAELEMENT_DEFAULTVALUE,    JSPROP_ENUMERATE},
  {"form",    HTMLTEXTAREAELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"accessKey",    HTMLTEXTAREAELEMENT_ACCESSKEY,    JSPROP_ENUMERATE},
  {"cols",    HTMLTEXTAREAELEMENT_COLS,    JSPROP_ENUMERATE},
  {"disabled",    HTMLTEXTAREAELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"name",    HTMLTEXTAREAELEMENT_NAME,    JSPROP_ENUMERATE},
  {"readOnly",    HTMLTEXTAREAELEMENT_READONLY,    JSPROP_ENUMERATE},
  {"rows",    HTMLTEXTAREAELEMENT_ROWS,    JSPROP_ENUMERATE},
  {"tabIndex",    HTMLTEXTAREAELEMENT_TABINDEX,    JSPROP_ENUMERATE},
  {"type",    HTMLTEXTAREAELEMENT_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"value",    HTMLTEXTAREAELEMENT_VALUE,    JSPROP_ENUMERATE},
  {"controllers",    NSHTMLTEXTAREAELEMENT_CONTROLLERS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLTextAreaElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTextAreaElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTextAreaElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTextAreaElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLTextAreaElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTextAreaElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
HTMLTextAreaElementBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTextAreaElement *nativeThis = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_BLUR, PR_FALSE);
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
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
HTMLTextAreaElementFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTextAreaElement *nativeThis = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_FOCUS, PR_FALSE);
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
// Native method Select
//
PR_STATIC_CALLBACK(JSBool)
HTMLTextAreaElementSelect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTextAreaElement *nativeThis = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTEXTAREAELEMENT_SELECT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Select();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLTextAreaElement
//
JSClass HTMLTextAreaElementClass = {
  "HTMLTextAreaElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTextAreaElementProperty,
  SetHTMLTextAreaElementProperty,
  EnumerateHTMLTextAreaElement,
  ResolveHTMLTextAreaElement,
  JS_ConvertStub,
  FinalizeHTMLTextAreaElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLTextAreaElement class methods
//
static JSFunctionSpec HTMLTextAreaElementMethods[] = 
{
  {"blur",          HTMLTextAreaElementBlur,     0},
  {"focus",          HTMLTextAreaElementFocus,     0},
  {"select",          HTMLTextAreaElementSelect,     0},
  {0}
};


//
// HTMLTextAreaElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTextAreaElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTextAreaElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTextAreaElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTextAreaElement", &vp)) ||
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
                         &HTMLTextAreaElementClass,      // JSClass
                         HTMLTextAreaElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTextAreaElementProperties,  // proto props
                         HTMLTextAreaElementMethods,     // proto funcs
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
// Method for creating a new HTMLTextAreaElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTextAreaElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTextAreaElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTextAreaElement *aHTMLTextAreaElement;

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

  if (NS_OK != NS_InitHTMLTextAreaElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTextAreaElementIID, (void **)&aHTMLTextAreaElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTextAreaElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTextAreaElement);
  }
  else {
    NS_RELEASE(aHTMLTextAreaElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
