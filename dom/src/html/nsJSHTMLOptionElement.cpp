/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLOptionElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMHTMLOptionElement);

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
  nsIDOMHTMLOptionElement *a = (nsIDOMHTMLOptionElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLOPTIONELEMENT_FORM:
      {
        nsIDOMHTMLFormElement* prop;
        if (NS_OK == a->GetForm(&prop)) {
          // get the js object
          nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_DEFAULTSELECTED:
      {
        PRBool prop;
        if (NS_OK == a->GetDefaultSelected(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_TEXT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetText(prop)) {
          nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_INDEX:
      {
        PRInt32 prop;
        if (NS_OK == a->GetIndex(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_DISABLED:
      {
        PRBool prop;
        if (NS_OK == a->GetDisabled(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_LABEL:
      {
        nsAutoString prop;
        if (NS_OK == a->GetLabel(prop)) {
          nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_SELECTED:
      {
        PRBool prop;
        if (NS_OK == a->GetSelected(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOPTIONELEMENT_VALUE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetValue(prop)) {
          nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLOptionElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLOptionElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLOptionElement *a = (nsIDOMHTMLOptionElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLOPTIONELEMENT_DEFAULTSELECTED:
      {
        PRBool prop;
        if (PR_FALSE == nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDefaultSelected(prop);
        
        break;
      }
      case HTMLOPTIONELEMENT_INDEX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetIndex(prop);
        
        break;
      }
      case HTMLOPTIONELEMENT_DISABLED:
      {
        PRBool prop;
        if (PR_FALSE == nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDisabled(prop);
        
        break;
      }
      case HTMLOPTIONELEMENT_LABEL:
      {
        nsAutoString prop;
        nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLabel(prop);
        
        break;
      }
      case HTMLOPTIONELEMENT_VALUE:
      {
        nsAutoString prop;
        nsConvertJSValToString(prop, cx, *vp);
      
        a->SetValue(prop);
        
        break;
      }
      default:
        return nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// HTMLOptionElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLOptionElement(JSContext *cx, JSObject *obj)
{
  nsGenericFinalize(cx, obj);
}


//
// HTMLOptionElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLOptionElement(JSContext *cx, JSObject *obj)
{
  return nsGenericEnumerate(cx, obj);
}


//
// HTMLOptionElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLOptionElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLOptionElement
//
JSClass HTMLOptionElementClass = {
  "HTMLOptionElement", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLOptionElementProperty,
  SetHTMLOptionElementProperty,
  EnumerateHTMLOptionElement,
  ResolveHTMLOptionElement,
  JS_ConvertStub,
  FinalizeHTMLOptionElement
};


//
// HTMLOptionElement class properties
//
static JSPropertySpec HTMLOptionElementProperties[] =
{
  {"form",    HTMLOPTIONELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"defaultSelected",    HTMLOPTIONELEMENT_DEFAULTSELECTED,    JSPROP_ENUMERATE},
  {"text",    HTMLOPTIONELEMENT_TEXT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"index",    HTMLOPTIONELEMENT_INDEX,    JSPROP_ENUMERATE},
  {"disabled",    HTMLOPTIONELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"label",    HTMLOPTIONELEMENT_LABEL,    JSPROP_ENUMERATE},
  {"selected",    HTMLOPTIONELEMENT_SELECTED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"value",    HTMLOPTIONELEMENT_VALUE,    JSPROP_ENUMERATE},
  {0}
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
  return JS_FALSE;
}


//
// HTMLOptionElement class initialization
//
nsresult NS_InitHTMLOptionElementClass(nsIScriptContext *aContext, void **aPrototype)
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
