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
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMHTMLTextAreaElement);

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
  HTMLTEXTAREAELEMENT_VALUE = -11
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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLTEXTAREAELEMENT_DEFAULTVALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.defaultvalue", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetDefaultValue(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_FORM:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.form", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLFormElement* prop;
        if (NS_SUCCEEDED(a->GetForm(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_ACCESSKEY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.accesskey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAccessKey(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_COLS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.cols", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetCols(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.disabled", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetDisabled(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.name", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetName(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_READONLY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.readonly", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetReadOnly(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_ROWS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.rows", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetRows(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.tabindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetTabIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.type", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetType(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTEXTAREAELEMENT_VALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.value", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetValue(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLTEXTAREAELEMENT_DEFAULTVALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.defaultvalue", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetDefaultValue(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_ACCESSKEY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.accesskey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAccessKey(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_COLS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.cols", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetCols(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.disabled", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDisabled(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.name", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_READONLY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.readonly", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetReadOnly(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_ROWS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.rows", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetRows(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.tabindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetTabIndex(prop);
        
        break;
      }
      case HTMLTEXTAREAELEMENT_VALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.value", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetValue(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


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
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLTextAreaElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTextAreaElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
HTMLTextAreaElementBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTextAreaElement *nativeThis = (nsIDOMHTMLTextAreaElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.blur", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Blur()) {
      return JS_FALSE;
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

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.focus", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Focus()) {
      return JS_FALSE;
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

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltextareaelement.select", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Select()) {
      return JS_FALSE;
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
  FinalizeHTMLTextAreaElement
};


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
  {0}
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
