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
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMOption.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIOptionIID, NS_IDOMOPTION_IID);

//
// HTMLOptionElement property ids
//
enum HTMLOptionElement_slots {
  HTMLOPTIONELEMENT_FORM = -1,
  HTMLOPTIONELEMENT_DEFAULTSELECTED = -2,
  HTMLOPTIONELEMENT_TEXT = -3,
  HTMLOPTIONELEMENT_INDEX = -4,
  HTMLOPTIONELEMENT_DISABLED = -5,
  HTMLOPTIONELEMENT_LABEL = -6,
  HTMLOPTIONELEMENT_SELECTED = -7,
  HTMLOPTIONELEMENT_VALUE = -8
};

/***********************************************************************/
//
// HTMLOptionElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLOptionElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLOptionElement *a = (nsIDOMHTMLOptionElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLOPTIONELEMENT_FORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_FORM, PR_FALSE);
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
      case HTMLOPTIONELEMENT_DEFAULTSELECTED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_DEFAULTSELECTED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDefaultSelected(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_TEXT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_TEXT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetText(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_INDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_INDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_LABEL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_LABEL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLabel(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_SELECTED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_SELECTED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetSelected(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOPTIONELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_VALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetValue(prop);
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
// HTMLOptionElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLOptionElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLOptionElement *a = (nsIDOMHTMLOptionElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLOPTIONELEMENT_DEFAULTSELECTED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_DEFAULTSELECTED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetDefaultSelected(prop);
          
        }
        break;
      }
      case HTMLOPTIONELEMENT_TEXT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_TEXT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetText(prop);
          
        }
        break;
      }
      case HTMLOPTIONELEMENT_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_DISABLED, PR_TRUE);
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
      case HTMLOPTIONELEMENT_LABEL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_LABEL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLabel(prop);
          
        }
        break;
      }
      case HTMLOPTIONELEMENT_SELECTED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_SELECTED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetSelected(prop);
          
        }
        break;
      }
      case HTMLOPTIONELEMENT_VALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOPTIONELEMENT_VALUE, PR_TRUE);
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
// HTMLOptionElement class properties
//
static JSPropertySpec HTMLOptionElementProperties[] =
{
  {"form",    HTMLOPTIONELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"defaultSelected",    HTMLOPTIONELEMENT_DEFAULTSELECTED,    JSPROP_ENUMERATE},
  {"text",    HTMLOPTIONELEMENT_TEXT,    JSPROP_ENUMERATE},
  {"index",    HTMLOPTIONELEMENT_INDEX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"disabled",    HTMLOPTIONELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"label",    HTMLOPTIONELEMENT_LABEL,    JSPROP_ENUMERATE},
  {"selected",    HTMLOPTIONELEMENT_SELECTED,    JSPROP_ENUMERATE},
  {"value",    HTMLOPTIONELEMENT_VALUE,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLOptionElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLOptionElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLOptionElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLOptionElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLOptionElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLOptionElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLOptionElement
//
JSClass HTMLOptionElementClass = {
  "HTMLOptionElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLOptionElementProperty,
  SetHTMLOptionElementProperty,
  EnumerateHTMLOptionElement,
  ResolveHTMLOptionElement,
  JS_ConvertStub,
  FinalizeHTMLOptionElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLOptionElement class methods
//
static JSFunctionSpec HTMLOptionElementMethods[] = 
{
  {0}
};


//
// HTMLOptionElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLOptionElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID interfaceID, classID;
  PRBool isConstructor;
  nsIDOMHTMLOptionElement *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
  static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);

  nsCOMPtr<nsIScriptContext> scriptCX;
  nsJSUtils::nsGetStaticScriptContext(cx, obj, getter_AddRefs(scriptCX));
  if (!scriptCX) {
    return JS_FALSE;
  }

  nsCOMPtr<nsIScriptNameSpaceManager> manager;
  scriptCX->GetNameSpaceManager(getter_AddRefs(manager));
  if (!manager) {
    return JS_FALSE;
  }

  result = manager->LookupName(NS_ConvertASCIItoUCS2("HTMLOptionElement"), isConstructor, interfaceID, classID);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMHTMLOptionElementIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nativeThis->QueryInterface(kIJSNativeInitializerIID, (void **)&initializer);
  if (NS_OK == result) {
    result = initializer->Initialize(cx, obj, argc, argv);
    NS_RELEASE(initializer);

    if (NS_OK != result) {
      NS_RELEASE(nativeThis);
      return JS_FALSE;
    }
  }

  result = nativeThis->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);
  if (NS_OK != result) {
    NS_RELEASE(nativeThis);
    return JS_FALSE;
  }

  owner->SetScriptObject((void *)obj);
  JS_SetPrivate(cx, obj, nativeThis);

  NS_RELEASE(owner);
  return JS_TRUE;
}

//
// HTMLOptionElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLOptionElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLOptionElement", &vp)) ||
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
                         &HTMLOptionElementClass,      // JSClass
                         HTMLOptionElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLOptionElementProperties,  // proto props
                         HTMLOptionElementMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    JS_AliasProperty(jscontext, global, "HTMLOptionElement", "Option");
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
// Method for creating a new HTMLOptionElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLOptionElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLOptionElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLOptionElement *aHTMLOptionElement;

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

  if (NS_OK != NS_InitHTMLOptionElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLOptionElementIID, (void **)&aHTMLOptionElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLOptionElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLOptionElement);
  }
  else {
    NS_RELEASE(aHTMLOptionElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
