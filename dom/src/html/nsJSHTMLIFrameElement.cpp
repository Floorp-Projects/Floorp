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
#include "nsIDOMHTMLIFrameElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLIFrameElementIID, NS_IDOMHTMLIFRAMEELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLIFrameElement);

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
  HTMLIFRAMEELEMENT_WIDTH = -10
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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLIFRAMEELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.align", PR_FALSE, &ok);
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
      case HTMLIFRAMEELEMENT_FRAMEBORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.frameborder", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFrameBorder(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.height", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_LONGDESC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.longdesc", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetLongDesc(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.marginheight", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.marginwidth", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.name", PR_FALSE, &ok);
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
      case HTMLIFRAMEELEMENT_SCROLLING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.scrolling", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetScrolling(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_SRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.src", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSrc(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIFRAMEELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.width", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWidth(prop))) {
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
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLIFRAMEELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.align", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_FRAMEBORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.frameborder", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFrameBorder(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.height", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHeight(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_LONGDESC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.longdesc", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLongDesc(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.marginheight", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginHeight(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_MARGINWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.marginwidth", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginWidth(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_SCROLLING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.scrolling", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetScrolling(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_SRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.src", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSrc(prop);
        
        break;
      }
      case HTMLIFRAMEELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmliframeelement.width", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWidth(prop);
        
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
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLIFrameElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLIFrameElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
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
  FinalizeHTMLIFrameElement
};


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
  {0}
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
