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
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLLINKELEMENT_DISABLED:
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
      case HTMLLINKELEMENT_CHARSET:
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
      case HTMLLINKELEMENT_HREF:
      {
        nsAutoString prop;
        if (NS_OK == a->GetHref(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        nsAutoString prop;
        if (NS_OK == a->GetHreflang(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMedia(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRel(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRev(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTarget(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLLINKELEMENT_TYPE:
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
// HTMLLinkElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLLinkElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case HTMLLINKELEMENT_DISABLED:
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
      
        a->SetDisabled(prop);
        
        break;
      }
      case HTMLLINKELEMENT_CHARSET:
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
      case HTMLLINKELEMENT_HREF:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetHref(prop);
        
        break;
      }
      case HTMLLINKELEMENT_HREFLANG:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetHreflang(prop);
        
        break;
      }
      case HTMLLINKELEMENT_MEDIA:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMedia(prop);
        
        break;
      }
      case HTMLLINKELEMENT_REL:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetRel(prop);
        
        break;
      }
      case HTMLLINKELEMENT_REV:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetRev(prop);
        
        break;
      }
      case HTMLLINKELEMENT_TARGET:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTarget(prop);
        
        break;
      }
      case HTMLLINKELEMENT_TYPE:
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
// HTMLLinkElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)JS_GetPrivate(cx, obj);
  
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
// HTMLLinkElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLLinkElement(JSContext *cx, JSObject *obj)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)JS_GetPrivate(cx, obj);
  
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
// HTMLLinkElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLLinkElement(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMHTMLLinkElement *a = (nsIDOMHTMLLinkElement*)JS_GetPrivate(cx, obj);
  
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
// class for HTMLLinkElement
//
JSClass HTMLLinkElementClass = {
  "HTMLLinkElement", 
  JSCLASS_HAS_PRIVATE,
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
nsresult NS_InitHTMLLinkElementClass(nsIScriptContext *aContext, void **aPrototype)
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
