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
#include "nsIDOMHTMLLinkElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLLinkElementIID, NS_IDOMHTMLLINKELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLLinkElement);

//
// HTMLLinkElement property ids
//
enum HTMLLinkElement_slots {
  HTMLLINKELEMENT_DISABLED = -1,
  HTMLLINKELEMENT_CHARSET = -2,
  HTMLLINKELEMENT_HREF = -3,
  HTMLLINKELEMENT_HREFLANG = -4,
  HTMLLINKELEMENT_MEDIA = -5,
  HTMLLINKELEMENT_REL = -6,
  HTMLLINKELEMENT_REV = -7,
  HTMLLINKELEMENT_TARGET = -8,
  HTMLLINKELEMENT_TYPE = -9
};

/***********************************************************************/
//
// HTMLLinkElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLLinkElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLINKELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.disabled", &ok);
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
      case HTMLLINKELEMENT_CHARSET:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.charset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCharset(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_HREF:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.href", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHref(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.hreflang", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHreflang(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.media", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMedia(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.rel", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetRel(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.rev", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetRev(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.target", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTarget(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.type", &ok);
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
// HTMLLinkElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLLinkElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLLINKELEMENT_DISABLED:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.disabled", &ok);
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
      case HTMLLINKELEMENT_CHARSET:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.charset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCharset(prop);
        
        break;
      }
      case HTMLLINKELEMENT_HREF:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.href", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHref(prop);
        
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.hreflang", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHreflang(prop);
        
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.media", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMedia(prop);
        
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.rel", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetRel(prop);
        
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.rev", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetRev(prop);
        
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.target", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTarget(prop);
        
        break;
      }
      case HTMLLINKELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmllinkelement.type", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetType(prop);
        
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
// HTMLLinkElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLLinkElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLLinkElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLLinkElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLLinkElement
//
JSClass HTMLLinkElementClass = {
  "HTMLLinkElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLLinkElementProperty,
  SetHTMLLinkElementProperty,
  EnumerateHTMLLinkElement,
  ResolveHTMLLinkElement,
  JS_ConvertStub,
  FinalizeHTMLLinkElement
};


//
// HTMLLinkElement class properties
//
static JSPropertySpec HTMLLinkElementProperties[] =
{
  {"disabled",    HTMLLINKELEMENT_DISABLED,    JSPROP_ENUMERATE},
  {"charset",    HTMLLINKELEMENT_CHARSET,    JSPROP_ENUMERATE},
  {"href",    HTMLLINKELEMENT_HREF,    JSPROP_ENUMERATE},
  {"hreflang",    HTMLLINKELEMENT_HREFLANG,    JSPROP_ENUMERATE},
  {"media",    HTMLLINKELEMENT_MEDIA,    JSPROP_ENUMERATE},
  {"rel",    HTMLLINKELEMENT_REL,    JSPROP_ENUMERATE},
  {"rev",    HTMLLINKELEMENT_REV,    JSPROP_ENUMERATE},
  {"target",    HTMLLINKELEMENT_TARGET,    JSPROP_ENUMERATE},
  {"type",    HTMLLINKELEMENT_TYPE,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLLinkElement class methods
//
static JSFunctionSpec HTMLLinkElementMethods[] = 
{
  {0}
};


//
// HTMLLinkElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLLinkElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLLinkElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLLinkElement", &vp)) ||
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
                         &HTMLLinkElementClass,      // JSClass
                         HTMLLinkElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLLinkElementProperties,  // proto props
                         HTMLLinkElementMethods,     // proto funcs
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
// Method for creating a new HTMLLinkElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLLinkElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLLinkElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLLinkElement *aHTMLLinkElement;

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

  if (NS_OK != NS_InitHTMLLinkElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLLinkElementIID, (void **)&aHTMLLinkElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLLinkElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLLinkElement);
  }
  else {
    NS_RELEASE(aHTMLLinkElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
