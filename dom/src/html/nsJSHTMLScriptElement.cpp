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
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLScriptElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLScriptElementIID, NS_IDOMHTMLSCRIPTELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLScriptElement);

//
// HTMLScriptElement property ids
//
enum HTMLScriptElement_slots {
  HTMLSCRIPTELEMENT_TEXT = -1,
  HTMLSCRIPTELEMENT_HTMLFOR = -2,
  HTMLSCRIPTELEMENT_EVENT = -3,
  HTMLSCRIPTELEMENT_CHARSET = -4,
  HTMLSCRIPTELEMENT_DEFER = -5,
  HTMLSCRIPTELEMENT_SRC = -6,
  HTMLSCRIPTELEMENT_TYPE = -7
};

/***********************************************************************/
//
// HTMLScriptElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLScriptElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLScriptElement *a = (nsIDOMHTMLScriptElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLSCRIPTELEMENT_TEXT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetText(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_HTMLFOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetHtmlFor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_EVENT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetEvent(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_CHARSET:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCharset(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_DEFER:
      {
        PRBool prop;
        if (NS_OK == a->GetDefer(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_SRC:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSrc(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLSCRIPTELEMENT_TYPE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetType(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
      {
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->GetProperty(cx, id, vp);
          NS_RELEASE(object);
          return rval;
        }
      }
    }
  }
  else {
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      PRBool rval;
      rval =  object->GetProperty(cx, id, vp);
      NS_RELEASE(object);
      return rval;
    }
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// HTMLScriptElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLScriptElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLScriptElement *a = (nsIDOMHTMLScriptElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLSCRIPTELEMENT_TEXT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetText(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_HTMLFOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetHtmlFor(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_EVENT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetEvent(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_CHARSET:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCharset(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_DEFER:
      {
        PRBool prop;
        JSBool temp;
        if (JSVAL_IS_BOOLEAN(*vp) && JS_ValueToBoolean(cx, *vp, &temp)) {
          prop = (PRBool)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a boolean");
          return JS_FALSE;
        }
      
        a->SetDefer(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_SRC:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSrc(prop);
        
        break;
      }
      case HTMLSCRIPTELEMENT_TYPE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetType(prop);
        
        break;
      }
      default:
      {
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->SetProperty(cx, id, vp);
          NS_RELEASE(object);
          return rval;
        }
      }
    }
  }
  else {
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      PRBool rval;
      rval =  object->SetProperty(cx, id, vp);
      NS_RELEASE(object);
      return rval;
    }
  }

  return PR_TRUE;
}


//
// HTMLScriptElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLScriptElement(JSContext *cx, JSObject *obj)
{
  nsIDOMHTMLScriptElement *a = (nsIDOMHTMLScriptElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == a->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->SetScriptObject(nsnull);
      NS_RELEASE(owner);
    }

    NS_RELEASE(a);
  }
}


//
// HTMLScriptElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLScriptElement(JSContext *cx, JSObject *obj)
{
  nsIDOMHTMLScriptElement *a = (nsIDOMHTMLScriptElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->EnumerateProperty(cx);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


//
// HTMLScriptElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLScriptElement(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMHTMLScriptElement *a = (nsIDOMHTMLScriptElement*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->Resolve(cx, id);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLScriptElement
//
JSClass HTMLScriptElementClass = {
  "HTMLScriptElement", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLScriptElementProperty,
  SetHTMLScriptElementProperty,
  EnumerateHTMLScriptElement,
  ResolveHTMLScriptElement,
  JS_ConvertStub,
  FinalizeHTMLScriptElement
};


//
// HTMLScriptElement class properties
//
static JSPropertySpec HTMLScriptElementProperties[] =
{
  {"text",    HTMLSCRIPTELEMENT_TEXT,    JSPROP_ENUMERATE},
  {"htmlFor",    HTMLSCRIPTELEMENT_HTMLFOR,    JSPROP_ENUMERATE},
  {"event",    HTMLSCRIPTELEMENT_EVENT,    JSPROP_ENUMERATE},
  {"charset",    HTMLSCRIPTELEMENT_CHARSET,    JSPROP_ENUMERATE},
  {"defer",    HTMLSCRIPTELEMENT_DEFER,    JSPROP_ENUMERATE},
  {"src",    HTMLSCRIPTELEMENT_SRC,    JSPROP_ENUMERATE},
  {"type",    HTMLSCRIPTELEMENT_TYPE,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLScriptElement class methods
//
static JSFunctionSpec HTMLScriptElementMethods[] = 
{
  {0}
};


//
// HTMLScriptElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLScriptElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLScriptElement class initialization
//
nsresult NS_InitHTMLScriptElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLScriptElement", &vp)) ||
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
                         &HTMLScriptElementClass,      // JSClass
                         HTMLScriptElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLScriptElementProperties,  // proto props
                         HTMLScriptElementMethods,     // proto funcs
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
// Method for creating a new HTMLScriptElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLScriptElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLScriptElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLScriptElement *aHTMLScriptElement;

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

  if (NS_OK != NS_InitHTMLScriptElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLScriptElementIID, (void **)&aHTMLScriptElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLScriptElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLScriptElement);
  }
  else {
    NS_RELEASE(aHTMLScriptElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
