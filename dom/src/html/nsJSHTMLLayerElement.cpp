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
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLLayerElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLLayerElementIID, NS_IDOMHTMLLAYERELEMENT_IID);

NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMHTMLLayerElement);

//
// HTMLLayerElement property ids
//
enum HTMLLayerElement_slots {
  HTMLLAYERELEMENT_TOP = -1,
  HTMLLAYERELEMENT_LEFT = -2,
  HTMLLAYERELEMENT_VISIBILITY = -3,
  HTMLLAYERELEMENT_BACKGROUND = -4,
  HTMLLAYERELEMENT_BGCOLOR = -5,
  HTMLLAYERELEMENT_NAME = -6,
  HTMLLAYERELEMENT_ZINDEX = -7,
  HTMLLAYERELEMENT_DOCUMENT = -8
};

/***********************************************************************/
//
// HTMLLayerElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLLayerElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLayerElement *a = (nsIDOMHTMLLayerElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLAYERELEMENT_TOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.top", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetTop(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_LEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.left", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetLeft(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_VISIBILITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.visibility", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVisibility(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_BACKGROUND:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.background", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackground(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_BGCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.bgcolor", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBgColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.name", PR_FALSE, &ok);
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
      case HTMLLAYERELEMENT_ZINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.zindex", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetZIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLAYERELEMENT_DOCUMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.document", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMDocument* prop;
        if (NS_SUCCEEDED(a->GetDocument(&prop))) {
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
// HTMLLayerElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLLayerElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLayerElement *a = (nsIDOMHTMLLayerElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLAYERELEMENT_TOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.top", PR_TRUE, &ok);
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
      
        a->SetTop(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_LEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.left", PR_TRUE, &ok);
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
      
        a->SetLeft(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_VISIBILITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.visibility", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVisibility(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_BACKGROUND:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.background", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackground(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_BGCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.bgcolor", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBgColor(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLLAYERELEMENT_ZINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmllayerelement.zindex", PR_TRUE, &ok);
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
      
        a->SetZIndex(prop);
        
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
// HTMLLayerElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLLayerElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLLayerElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLLayerElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLLayerElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLLayerElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLLayerElement
//
JSClass HTMLLayerElementClass = {
  "HTMLLayerElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLLayerElementProperty,
  SetHTMLLayerElementProperty,
  EnumerateHTMLLayerElement,
  ResolveHTMLLayerElement,
  JS_ConvertStub,
  FinalizeHTMLLayerElement
};


//
// HTMLLayerElement class properties
//
static JSPropertySpec HTMLLayerElementProperties[] =
{
  {"top",    HTMLLAYERELEMENT_TOP,    JSPROP_ENUMERATE},
  {"left",    HTMLLAYERELEMENT_LEFT,    JSPROP_ENUMERATE},
  {"visibility",    HTMLLAYERELEMENT_VISIBILITY,    JSPROP_ENUMERATE},
  {"background",    HTMLLAYERELEMENT_BACKGROUND,    JSPROP_ENUMERATE},
  {"bgColor",    HTMLLAYERELEMENT_BGCOLOR,    JSPROP_ENUMERATE},
  {"name",    HTMLLAYERELEMENT_NAME,    JSPROP_ENUMERATE},
  {"zIndex",    HTMLLAYERELEMENT_ZINDEX,    JSPROP_ENUMERATE},
  {"document",    HTMLLAYERELEMENT_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLLayerElement class methods
//
static JSFunctionSpec HTMLLayerElementMethods[] = 
{
  {0}
};


//
// HTMLLayerElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLLayerElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLLayerElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLLayerElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLLayerElement", &vp)) ||
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
                         &HTMLLayerElementClass,      // JSClass
                         HTMLLayerElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLLayerElementProperties,  // proto props
                         HTMLLayerElementMethods,     // proto funcs
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
// Method for creating a new HTMLLayerElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLLayerElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLLayerElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLLayerElement *aHTMLLayerElement;

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

  if (NS_OK != NS_InitHTMLLayerElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLLayerElementIID, (void **)&aHTMLLayerElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLLayerElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLLayerElement);
  }
  else {
    NS_RELEASE(aHTMLLayerElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
