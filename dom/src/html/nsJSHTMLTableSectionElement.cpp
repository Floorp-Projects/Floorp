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
#include "nsCOMPtr.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableSectionElementIID, NS_IDOMHTMLTABLESECTIONELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

NS_DEF_PTR(nsIDOMHTMLElement);
NS_DEF_PTR(nsIDOMHTMLTableSectionElement);
NS_DEF_PTR(nsIDOMHTMLCollection);

//
// HTMLTableSectionElement property ids
//
enum HTMLTableSectionElement_slots {
  HTMLTABLESECTIONELEMENT_ALIGN = -1,
  HTMLTABLESECTIONELEMENT_CH = -2,
  HTMLTABLESECTIONELEMENT_CHOFF = -3,
  HTMLTABLESECTIONELEMENT_VALIGN = -4,
  HTMLTABLESECTIONELEMENT_ROWS = -5
};

/***********************************************************************/
//
// HTMLTableSectionElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTableSectionElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableSectionElement *a = (nsIDOMHTMLTableSectionElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLTABLESECTIONELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.align", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLESECTIONELEMENT_CH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.ch", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCh(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLESECTIONELEMENT_CHOFF:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.choff", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetChOff(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLESECTIONELEMENT_VALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.valign", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLESECTIONELEMENT_ROWS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.rows", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (NS_SUCCEEDED(a->GetRows(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLTableSectionElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTableSectionElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableSectionElement *a = (nsIDOMHTMLTableSectionElement*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLTABLESECTIONELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.align", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLTABLESECTIONELEMENT_CH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.ch", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCh(prop);
        
        break;
      }
      case HTMLTABLESECTIONELEMENT_CHOFF:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.choff", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetChOff(prop);
        
        break;
      }
      case HTMLTABLESECTIONELEMENT_VALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.valign", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVAlign(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// HTMLTableSectionElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTableSectionElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTableSectionElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTableSectionElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLTableSectionElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTableSectionElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method InsertRow
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableSectionElementInsertRow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableSectionElement *nativeThis = (nsIDOMHTMLTableSectionElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMHTMLElement* nativeRet;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.insertrow",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function insertRow requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->InsertRow(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method DeleteRow
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableSectionElementDeleteRow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableSectionElement *nativeThis = (nsIDOMHTMLTableSectionElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltablesectionelement.deleterow",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function deleteRow requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->DeleteRow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLTableSectionElement
//
JSClass HTMLTableSectionElementClass = {
  "HTMLTableSectionElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTableSectionElementProperty,
  SetHTMLTableSectionElementProperty,
  EnumerateHTMLTableSectionElement,
  ResolveHTMLTableSectionElement,
  JS_ConvertStub,
  FinalizeHTMLTableSectionElement
};


//
// HTMLTableSectionElement class properties
//
static JSPropertySpec HTMLTableSectionElementProperties[] =
{
  {"align",    HTMLTABLESECTIONELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"ch",    HTMLTABLESECTIONELEMENT_CH,    JSPROP_ENUMERATE},
  {"chOff",    HTMLTABLESECTIONELEMENT_CHOFF,    JSPROP_ENUMERATE},
  {"vAlign",    HTMLTABLESECTIONELEMENT_VALIGN,    JSPROP_ENUMERATE},
  {"rows",    HTMLTABLESECTIONELEMENT_ROWS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLTableSectionElement class methods
//
static JSFunctionSpec HTMLTableSectionElementMethods[] = 
{
  {"insertRow",          HTMLTableSectionElementInsertRow,     1},
  {"deleteRow",          HTMLTableSectionElementDeleteRow,     1},
  {0}
};


//
// HTMLTableSectionElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableSectionElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTableSectionElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTableSectionElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTableSectionElement", &vp)) ||
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
                         &HTMLTableSectionElementClass,      // JSClass
                         HTMLTableSectionElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTableSectionElementProperties,  // proto props
                         HTMLTableSectionElementMethods,     // proto funcs
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
// Method for creating a new HTMLTableSectionElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTableSectionElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTableSectionElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTableSectionElement *aHTMLTableSectionElement;

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

  if (NS_OK != NS_InitHTMLTableSectionElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTableSectionElementIID, (void **)&aHTMLTableSectionElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTableSectionElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTableSectionElement);
  }
  else {
    NS_RELEASE(aHTMLTableSectionElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
