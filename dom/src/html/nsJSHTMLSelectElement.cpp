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
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNSHTMLSelectElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kINSHTMLSelectElementIID, NS_IDOMNSHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

NS_DEF_PTR(nsIDOMHTMLSelectElement);
NS_DEF_PTR(nsIDOMHTMLElement);
NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMNSHTMLSelectElement);
NS_DEF_PTR(nsIDOMHTMLCollection);

//
// HTMLSelectElement property ids
//
enum HTMLSelectElement_slots {
  HTMLSELECTELEMENT_TYPE = -1,
  HTMLSELECTELEMENT_SELECTEDINDEX = -2,
  HTMLSELECTELEMENT_VALUE = -3,
  HTMLSELECTELEMENT_LENGTH = -4,
  HTMLSELECTELEMENT_FORM = -5,
  HTMLSELECTELEMENT_OPTIONS = -6,
  HTMLSELECTELEMENT_DISABLED = -7,
  HTMLSELECTELEMENT_MULTIPLE = -8,
  HTMLSELECTELEMENT_NAME = -9,
  HTMLSELECTELEMENT_SIZE = -10,
  HTMLSELECTELEMENT_TABINDEX = -11
};

/***********************************************************************/
//
// HTMLSelectElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLSelectElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLSelectElement *a = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLSELECTELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.type", &ok);
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
      case HTMLSELECTELEMENT_SELECTEDINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.selectedindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetSelectedIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSELECTELEMENT_VALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.value", &ok);
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
      case HTMLSELECTELEMENT_LENGTH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.length", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint32 prop;
        if (NS_SUCCEEDED(a->GetLength(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSELECTELEMENT_FORM:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.form", &ok);
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
      case HTMLSELECTELEMENT_OPTIONS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.options", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (NS_SUCCEEDED(a->GetOptions(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSELECTELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.disabled", &ok);
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
      case HTMLSELECTELEMENT_MULTIPLE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.multiple", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetMultiple(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSELECTELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.name", &ok);
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
      case HTMLSELECTELEMENT_SIZE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.size", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetSize(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSELECTELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.tabindex", &ok);
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
      default:
      {
        nsIDOMNode* prop;
        nsIDOMNSHTMLSelectElement* b;
        if (NS_OK == a->QueryInterface(kINSHTMLSelectElementIID, (void **)&b)) {
          if (NS_OK == b->Item(JSVAL_TO_INT(id), &prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSHTMLSelectElement");
          return JS_FALSE;
        }
      }
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLSelectElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLSelectElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLSelectElement *a = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLSELECTELEMENT_SELECTEDINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.selectedindex", &ok);
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
      
        a->SetSelectedIndex(prop);
        
        break;
      }
      case HTMLSELECTELEMENT_VALUE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.value", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetValue(prop);
        
        break;
      }
      case HTMLSELECTELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.disabled", &ok);
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
      case HTMLSELECTELEMENT_MULTIPLE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.multiple", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetMultiple(prop);
        
        break;
      }
      case HTMLSELECTELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.name", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLSELECTELEMENT_SIZE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.size", &ok);
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
      
        a->SetSize(prop);
        
        break;
      }
      case HTMLSELECTELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.tabindex", &ok);
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
// HTMLSelectElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLSelectElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLSelectElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLSelectElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLSelectElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLSelectElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Add
//
PR_STATIC_CALLBACK(JSBool)
HTMLSelectElementAdd(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLSelectElement *nativeThis = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMHTMLElementPtr b0;
  nsIDOMHTMLElementPtr b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.add", &ok);
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
    if (argc < 2) {
      JS_ReportError(cx, "Function add requires 2 parameters");
      return JS_FALSE;
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIHTMLElementIID,
                                           "HTMLElement",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIHTMLElementIID,
                                           "HTMLElement",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Add(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Remove
//
PR_STATIC_CALLBACK(JSBool)
HTMLSelectElementRemove(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLSelectElement *nativeThis = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.remove", &ok);
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
    if (argc < 1) {
      JS_ReportError(cx, "Function remove requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Remove(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
HTMLSelectElementBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLSelectElement *nativeThis = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.blur", &ok);
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
HTMLSelectElementFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLSelectElement *nativeThis = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmlselectelement.focus", &ok);
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
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLSelectElementItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLSelectElement *privateThis = (nsIDOMHTMLSelectElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMNSHTMLSelectElement *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLSelectElementIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type NSHTMLSelectElement");
    return JS_FALSE;
  }

  nsIDOMNode* nativeRet;
  PRUint32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "nshtmlselectelement.item", &ok);
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
    if (argc < 1) {
      JS_ReportError(cx, "Function item requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Item(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLSelectElement
//
JSClass HTMLSelectElementClass = {
  "HTMLSelectElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLSelectElementProperty,
  SetHTMLSelectElementProperty,
  EnumerateHTMLSelectElement,
  ResolveHTMLSelectElement,
  JS_ConvertStub,
  FinalizeHTMLSelectElement
};


//
// HTMLSelectElement class properties
//
static JSPropertySpec HTMLSelectElementProperties[] =
{
  {"type",    HTMLSELECTELEMENT_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"selectedIndex",    HTMLSELECTELEMENT_SELECTEDINDEX,    JSPROP_ENUMERATE},
  {"value",    HTMLSELECTELEMENT_VALUE,    JSPROP_ENUMERATE},
  {"length",    HTMLSELECTELEMENT_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"form",    HTMLSELECTELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"options",    HTMLSELECTELEMENT_OPTIONS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"disabled",    HTMLSELECTELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"multiple",    HTMLSELECTELEMENT_MULTIPLE,    JSPROP_ENUMERATE},
  {"name",    HTMLSELECTELEMENT_NAME,    JSPROP_ENUMERATE},
  {"size",    HTMLSELECTELEMENT_SIZE,    JSPROP_ENUMERATE},
  {"tabIndex",    HTMLSELECTELEMENT_TABINDEX,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLSelectElement class methods
//
static JSFunctionSpec HTMLSelectElementMethods[] = 
{
  {"add",          HTMLSelectElementAdd,     2},
  {"remove",          HTMLSelectElementRemove,     1},
  {"blur",          HTMLSelectElementBlur,     0},
  {"focus",          HTMLSelectElementFocus,     0},
  {"item",          NSHTMLSelectElementItem,     1},
  {0}
};


//
// HTMLSelectElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLSelectElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLSelectElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLSelectElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLSelectElement", &vp)) ||
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
                         &HTMLSelectElementClass,      // JSClass
                         HTMLSelectElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLSelectElementProperties,  // proto props
                         HTMLSelectElementMethods,     // proto funcs
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
// Method for creating a new HTMLSelectElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLSelectElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLSelectElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLSelectElement *aHTMLSelectElement;

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

  if (NS_OK != NS_InitHTMLSelectElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLSelectElementIID, (void **)&aHTMLSelectElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLSelectElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLSelectElement);
  }
  else {
    NS_RELEASE(aHTMLSelectElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
