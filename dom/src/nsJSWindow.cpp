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

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMNavigator);
NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMWindow);

//
// Window property ids
//
enum Window_slots {
  WINDOW_WINDOW = -11,
  WINDOW_DOCUMENT = -12,
  WINDOW_NAVIGATOR = -13
};

/***********************************************************************/
//
// Window Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_WINDOW:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetWindow(&prop)) {
          // get the js object
          if (prop != nsnull) {
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == prop->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
              if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
                // set the return value
                *vp = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
            NS_RELEASE(prop);
          }
          else {
            *vp = JSVAL_NULL;
          }
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_DOCUMENT:
      {
        nsIDOMDocument* prop;
        if (NS_OK == a->GetDocument(&prop)) {
          // get the js object
          if (prop != nsnull) {
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == prop->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
              if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
                // set the return value
                *vp = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
            NS_RELEASE(prop);
          }
          else {
            *vp = JSVAL_NULL;
          }
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_NAVIGATOR:
      {
        nsIDOMNavigator* prop;
        if (NS_OK == a->GetNavigator(&prop)) {
          // get the js object
          if (prop != nsnull) {
            nsIScriptObjectOwner *owner = nsnull;
            if (NS_OK == prop->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
              JSObject *object = nsnull;
              nsIScriptContext *script_cx = (nsIScriptContext *)JS_GetContextPrivate(cx);
              if (NS_OK == owner->GetScriptObject(script_cx, (void**)&object)) {
                // set the return value
                *vp = OBJECT_TO_JSVAL(object);
              }
              NS_RELEASE(owner);
            }
            NS_RELEASE(prop);
          }
          else {
            *vp = JSVAL_NULL;
          }
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

  return PR_TRUE;
}

/***********************************************************************/
//
// Window Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
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

  return PR_TRUE;
}


//
// Window finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeWindow(JSContext *cx, JSObject *obj)
{
}


//
// Window enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateWindow(JSContext *cx, JSObject *obj)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  
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
// Window resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveWindow(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  
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


//
// Native method Dump
//
PR_STATIC_CALLBACK(JSBool)
WindowDump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != nativeThis, "null pointer");
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;
  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->Dump(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Alert
//
PR_STATIC_CALLBACK(JSBool)
WindowAlert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->Alert(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    return JS_FALSE;
  }

  return JS_TRUE;
}

/***********************************************************************/
//
// class for Window
//
JSClass WindowClass = {
  "Window", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetWindowProperty,
  SetWindowProperty,
  EnumerateWindow,
  ResolveWindow,
  JS_ConvertStub,
  FinalizeWindow
};


//
// Window class properties
//
static JSPropertySpec WindowProperties[] =
{
  {"window",    WINDOW_WINDOW,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"document",    WINDOW_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"navigator",    WINDOW_NAVIGATOR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Window class methods
//
static JSFunctionSpec WindowMethods[] = 
{
  {"dump",          WindowDump,     1},
  {"alert",          WindowAlert,     1},
  {0}
};


//
// Window constructor
//
PR_STATIC_CALLBACK(JSBool)
Window(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_TRUE;
}


//
// Window class initialization
//
nsresult NS_InitWindowClass(nsIScriptContext *aContext, 
                            nsIScriptGlobalObject *aGlobal)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *global = JS_GetGlobalObject(jscontext);

  JS_DefineProperties(jscontext, global, WindowProperties);
  JS_DefineFunctions(jscontext, global, WindowMethods);

  return NS_OK;
}


//
// Method for creating a new Window JavaScript object
//
extern "C" NS_DOM NS_NewScriptWindow(nsIScriptContext *aContext, nsIDOMWindow *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null arg");
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();

  JSObject *global = ::JS_NewObject(jscontext, &WindowClass, NULL, NULL);
  if (global) {
    // The global object has a to be defined in two step:
    // 1- create a generic object, with no prototype and no parent which
    //    will be passed to JS_InitStandardClasses. JS_InitStandardClasses 
    //    will make it the global object
    // 2- define the global object to be what you really want it to be.
    //
    // The js runtime is not fully initialized before JS_InitStandardClasses
    // is called, so part of the global object initialization has to be moved 
    // after JS_InitStandardClasses

    // assign "this" to the js object, don't AddRef
    ::JS_SetPrivate(jscontext, global, aSupports);

    *aReturn = (void*)global;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
