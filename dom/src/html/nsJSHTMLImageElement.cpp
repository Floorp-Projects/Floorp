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
#include "nsIDOMImage.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIImageIID, NS_IDOMIMAGE_IID);
static NS_DEFINE_IID(kIHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

NS_DEF_PTR(nsIDOMImage);
NS_DEF_PTR(nsIDOMHTMLImageElement);

//
// HTMLImageElement property ids
//
enum HTMLImageElement_slots {
  HTMLIMAGEELEMENT_LOWSRC = -1,
  HTMLIMAGEELEMENT_NAME = -2,
  HTMLIMAGEELEMENT_ALIGN = -3,
  HTMLIMAGEELEMENT_ALT = -4,
  HTMLIMAGEELEMENT_BORDER = -5,
  HTMLIMAGEELEMENT_HEIGHT = -6,
  HTMLIMAGEELEMENT_HSPACE = -7,
  HTMLIMAGEELEMENT_ISMAP = -8,
  HTMLIMAGEELEMENT_LONGDESC = -9,
  HTMLIMAGEELEMENT_SRC = -10,
  HTMLIMAGEELEMENT_USEMAP = -11,
  HTMLIMAGEELEMENT_VSPACE = -12,
  HTMLIMAGEELEMENT_WIDTH = -13
};

/***********************************************************************/
//
// HTMLImageElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLImageElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLImageElement *a = (nsIDOMHTMLImageElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIMAGEELEMENT_LOWSRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.lowsrc", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetLowSrc(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.name", PR_FALSE, &ok);
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
      case HTMLIMAGEELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.align", PR_FALSE, &ok);
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
      case HTMLIMAGEELEMENT_ALT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.alt", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAlt(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_BORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.border", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorder(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.height", PR_FALSE, &ok);
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
      case HTMLIMAGEELEMENT_HSPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.hspace", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHspace(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_ISMAP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.ismap", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetIsMap(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_LONGDESC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.longdesc", PR_FALSE, &ok);
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
      case HTMLIMAGEELEMENT_SRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.src", PR_FALSE, &ok);
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
      case HTMLIMAGEELEMENT_USEMAP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.usemap", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetUseMap(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_VSPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.vspace", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVspace(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLIMAGEELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.width", PR_FALSE, &ok);
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
// HTMLImageElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLImageElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLImageElement *a = (nsIDOMHTMLImageElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIMAGEELEMENT_LOWSRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.lowsrc", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLowSrc(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_ALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.align", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_ALT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.alt", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlt(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_BORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.border", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorder(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.height", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHeight(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_HSPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.hspace", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHspace(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_ISMAP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.ismap", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetIsMap(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_LONGDESC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.longdesc", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLongDesc(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_SRC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.src", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSrc(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_USEMAP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.usemap", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetUseMap(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_VSPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.vspace", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVspace(prop);
        
        break;
      }
      case HTMLIMAGEELEMENT_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlimageelement.width", PR_TRUE, &ok);
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
// HTMLImageElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLImageElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLImageElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLImageElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLImageElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLImageElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLImageElement
//
JSClass HTMLImageElementClass = {
  "HTMLImageElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLImageElementProperty,
  SetHTMLImageElementProperty,
  EnumerateHTMLImageElement,
  ResolveHTMLImageElement,
  JS_ConvertStub,
  FinalizeHTMLImageElement
};


//
// HTMLImageElement class properties
//
static JSPropertySpec HTMLImageElementProperties[] =
{
  {"lowSrc",    HTMLIMAGEELEMENT_LOWSRC,    JSPROP_ENUMERATE},
  {"name",    HTMLIMAGEELEMENT_NAME,    JSPROP_ENUMERATE},
  {"align",    HTMLIMAGEELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"alt",    HTMLIMAGEELEMENT_ALT,    JSPROP_ENUMERATE},
  {"border",    HTMLIMAGEELEMENT_BORDER,    JSPROP_ENUMERATE},
  {"height",    HTMLIMAGEELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"hspace",    HTMLIMAGEELEMENT_HSPACE,    JSPROP_ENUMERATE},
  {"isMap",    HTMLIMAGEELEMENT_ISMAP,    JSPROP_ENUMERATE},
  {"longDesc",    HTMLIMAGEELEMENT_LONGDESC,    JSPROP_ENUMERATE},
  {"src",    HTMLIMAGEELEMENT_SRC,    JSPROP_ENUMERATE},
  {"useMap",    HTMLIMAGEELEMENT_USEMAP,    JSPROP_ENUMERATE},
  {"vspace",    HTMLIMAGEELEMENT_VSPACE,    JSPROP_ENUMERATE},
  {"width",    HTMLIMAGEELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLImageElement class methods
//
static JSFunctionSpec HTMLImageElementMethods[] = 
{
  {0}
};


//
// HTMLImageElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLImageElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMHTMLImageElement *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
  static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("HTMLImageElement", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMHTMLImageElementIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nativeThis->QueryInterface(kIJSNativeInitializerIID, (void **)&initializer);
  if (NS_OK == result) {
    result = initializer->Initialize(cx, argc, argv);
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
// HTMLImageElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLImageElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLImageElement", &vp)) ||
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
                         &HTMLImageElementClass,      // JSClass
                         HTMLImageElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLImageElementProperties,  // proto props
                         HTMLImageElementMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    JS_AliasProperty(jscontext, global, "HTMLImageElement", "Image");
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
// Method for creating a new HTMLImageElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLImageElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLImageElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLImageElement *aHTMLImageElement;

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

  if (NS_OK != NS_InitHTMLImageElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLImageElementIID, (void **)&aHTMLImageElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLImageElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLImageElement);
  }
  else {
    NS_RELEASE(aHTMLImageElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
