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
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

NS_DEF_PTR(nsIDOMHTMLMapElement);
NS_DEF_PTR(nsIDOMHTMLCollection);

//
// HTMLMapElement property ids
//
enum HTMLMapElement_slots {
  HTMLMAPELEMENT_AREAS = -1,
  HTMLMAPELEMENT_NAME = -2
};

/***********************************************************************/
//
// HTMLMapElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLMapElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLMapElement *a = (nsIDOMHTMLMapElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLMAPELEMENT_AREAS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlmapelement.areas", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (NS_SUCCEEDED(a->GetAreas(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLMAPELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlmapelement.name", PR_FALSE, &ok);
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
// HTMLMapElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLMapElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLMapElement *a = (nsIDOMHTMLMapElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLMAPELEMENT_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "htmlmapelement.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
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
// HTMLMapElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLMapElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLMapElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLMapElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLMapElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLMapElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLMapElement
//
JSClass HTMLMapElementClass = {
  "HTMLMapElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLMapElementProperty,
  SetHTMLMapElementProperty,
  EnumerateHTMLMapElement,
  ResolveHTMLMapElement,
  JS_ConvertStub,
  FinalizeHTMLMapElement
};


//
// HTMLMapElement class properties
//
static JSPropertySpec HTMLMapElementProperties[] =
{
  {"areas",    HTMLMAPELEMENT_AREAS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    HTMLMAPELEMENT_NAME,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLMapElement class methods
//
static JSFunctionSpec HTMLMapElementMethods[] = 
{
  {0}
};


//
// HTMLMapElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLMapElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLMapElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLMapElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLMapElement", &vp)) ||
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
                         &HTMLMapElementClass,      // JSClass
                         HTMLMapElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLMapElementProperties,  // proto props
                         HTMLMapElementMethods,     // proto funcs
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
// Method for creating a new HTMLMapElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLMapElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLMapElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLMapElement *aHTMLMapElement;

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

  if (NS_OK != NS_InitHTMLMapElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLMapElementIID, (void **)&aHTMLMapElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLMapElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLMapElement);
  }
  else {
    NS_RELEASE(aHTMLMapElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
