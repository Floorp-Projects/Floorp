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
#include "nsIScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIWebWidget.h"
#include "nsIDocument.h"
#include "nsString.h"

static NS_DEFINE_IID(kIScriptObjectIID, NS_ISCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

//
// Window property ids
//
enum window_slot {
  WINDOW_DOCUMENT           = -1,
};

/***********************************************************************/

//
// Window properties getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIWebWidget *window = (nsIWebWidget*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != window, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_DOCUMENT:
      {
        nsIDocument *document = window->GetDocument();
        // call the function
        if (nsnull != document) {

          // get the js object
          nsIScriptObjectOwner *owner = nsnull;
          if (NS_OK == document->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
            JSObject *object = nsnull;
            if (NS_OK == owner->GetScriptObject(cx, (void**)&object)) {
              // set the return value
              *vp = OBJECT_TO_JSVAL(object);
            }
            NS_RELEASE(owner);
          }
          NS_RELEASE(document);
        }
        break;
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == window->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          PRBool ret = object->GetProperty(cx, id, vp);
          NS_RELEASE(object);
          return ret;
        }
      }
    }
  }

  return PR_TRUE;
}

//
// Window properties setter
//
PR_STATIC_CALLBACK(JSBool)
SetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIWebWidget *window = (nsIWebWidget*)JS_GetPrivate(cx, obj);
  NS_ASSERTION(nsnull != window, "null pointer");

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_DOCUMENT:
      {
        break;
      }
      default:
      {
        nsIScriptObject *object;
        if (NS_OK == window->QueryInterface(kIScriptObjectIID, (void**)&object)) {
          PRBool ret = object->SetProperty(cx, id, vp);
          NS_RELEASE(object);
          return ret;
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
  /*
  nsIWebWidget *window = (nsIWebWidget*)JS_GetPrivate(cx, obj);

  if (nsnull != window) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == window->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->ResetScriptObject();
      NS_RELEASE(owner);
    }
  }
  */
}

/***********************************************************************/
//
// JS->Native functions
//
PR_STATIC_CALLBACK(JSBool)
Dump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  NS_ASSERTION(1 == argc, "wrong number of arguments");
  if (1 == argc) {
    // get the arguments
    if (JSVAL_IS_STRING(argv[0])) {
      JSString *jsstring1 = JSVAL_TO_STRING(argv[0]);
      char *print = JS_GetStringBytes(jsstring1);
      printf("%s", print);
      //delete print;
    }
  }

  return JS_TRUE;
}

/***********************************************************************/
//
// the jscript class for a DOM Window
//
JSClass window = {
  "Window", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  
  JS_PropertyStub,    
  GetWindowProperty,     
  SetWindowProperty,
  JS_EnumerateStub, 
  JS_ResolveStub,     
  JS_ConvertStub,      
  FinalizeWindow
};

//
// Window class properties
//
static JSPropertySpec windowProperties[] =
{
  {"document",        WINDOW_DOCUMENT,     JSPROP_ENUMERATE},
  {0}
};

//
// Window class methods
//
static JSFunctionSpec windowMethods[] = {
  {"dump",          Dump,         1},
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
// Window static property ids
//

//
// Window static properties
//

//
// Window static methods
//

/***********************************************************************/

//
// Init this object class
//
nsresult NS_InitWindowClass(JSContext *aContext, JSObject *aGlobalObject)
{
  // look in the global object for this class prototype
  JS_DefineProperties(aContext, aGlobalObject, windowProperties);
  JS_DefineFunctions(aContext, aGlobalObject, windowMethods);
  return NS_OK;
}

//
// Window instance property ids
//

//
// Window instance properties
//

//
// New a Window object in js, connect the native and js worlds
//
nsresult NS_NewGlobalWindow(JSContext *aContext, nsIWebWidget *aWindow, void **aJSObject)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aWindow && nsnull != aJSObject, "null arg");

  JSObject *global = ::JS_NewObject(aContext, &window, NULL, NULL);
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
    ::JS_SetPrivate(aContext, global, aWindow);

    *aJSObject = (void*)global;

    return NS_OK;
  }

  return NS_ERROR_FAILURE; 
}

