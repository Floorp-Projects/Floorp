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
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMComposeAppCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsRepository.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIComposeAppCoreIID, NS_IDOMCOMPOSEAPPCORE_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMComposeAppCore);
NS_DEF_PTR(nsIDOMWindow);


/***********************************************************************/
//
// ComposeAppCore Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetComposeAppCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMComposeAppCore *a = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
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
// ComposeAppCore Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetComposeAppCoreProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMComposeAppCore *a = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case 0:
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
// ComposeAppCore finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeComposeAppCore(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// ComposeAppCore enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateComposeAppCore(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// ComposeAppCore resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveComposeAppCore(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method SetWindow
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreSetWindow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMWindowPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIWindowIID,
                                           "Window",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetWindow(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SetWindow requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CompleteCallback
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreCompleteCallback(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->CompleteCallback(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function CompleteCallback requires 1 parameter");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method NewMessage
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreNewMessage(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

   nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

   if (NS_OK != nativeThis->NewMessage(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function NewMessage requires 1 parameter");
    return JS_FALSE;
  }

  return JS_TRUE;
}

//
// Native method ReplyMessage
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreReplyMessage(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsISupports * b1;
  const nsString typeName;
 long b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

   nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

   rBool = nsJSUtils::nsConvertJSValToObject((nsISupports**)&b1, nsISupports::GetIID(),
                                  typeName,
                                  cx,
                                  argv[1]);

	b2 = argv[2];

   if (!rBool && NS_OK != nativeThis->ReplyMessage(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ReplyMessage requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ForwardMessage
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreForwardMessage(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsISupports * b1;
  const nsString typeName;
 long b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

   nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

   rBool = nsJSUtils::nsConvertJSValToObject((nsISupports**)&b1, nsISupports::GetIID(),
                                  typeName,
                                  cx,
                                  argv[1]);

	b2 = argv[2];

   if (!rBool && NS_OK != nativeThis->ForwardMessage(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function ForwardMessage requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SendMessage
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreSendMessage(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;
  nsAutoString b3;
  nsAutoString b4;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 5) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);
    nsJSUtils::nsConvertJSValToString(b2, cx, argv[2]);
    nsJSUtils::nsConvertJSValToString(b3, cx, argv[3]);
    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);

    if (NS_OK != nativeThis->SendMessage(b0, b1, b2, b3, b4)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function SendMessage requires 5 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SendMessage2
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCoreSendMessage2(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMComposeAppCore *nativeThis = (nsIDOMComposeAppCore*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->SendMessage2(&b0)) {
      return JS_FALSE;
    }

    *rval = (jsval) b0;
  }
  else {
    JS_ReportError(cx, "Function SendMessage requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for ComposeAppCore
//
JSClass ComposeAppCoreClass = {
  "ComposeAppCore", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetComposeAppCoreProperty,
  SetComposeAppCoreProperty,
  EnumerateComposeAppCore,
  ResolveComposeAppCore,
  JS_ConvertStub,
  FinalizeComposeAppCore
};


//
// ComposeAppCore class properties
//
static JSPropertySpec ComposeAppCoreProperties[] =
{
  {0}
};


//
// ComposeAppCore class methods
//
static JSFunctionSpec ComposeAppCoreMethods[] = 
{
  {"SetWindow",				ComposeAppCoreSetWindow,		1},
  {"CompleteCallback",		ComposeAppCoreCompleteCallback,	1},
  {"NewMessage",			ComposeAppCoreNewMessage,		1},
  {"ReplyMessage",			ComposeAppCoreReplyMessage,		3},
  {"ForwardMessage",		ComposeAppCoreForwardMessage,	3},
  {"SendMessage",			ComposeAppCoreSendMessage,		5},
  {"SendMessage2",			ComposeAppCoreSendMessage2,		0},
  {0}
};


//
// ComposeAppCore constructor
//
PR_STATIC_CALLBACK(JSBool)
ComposeAppCore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID classID;
  nsIScriptContext* context = (nsIScriptContext*)JS_GetContextPrivate(cx);
  nsIScriptNameSpaceManager* manager;
  nsIDOMComposeAppCore *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;

  static NS_DEFINE_IID(kIDOMComposeAppCoreIID, NS_IDOMCOMPOSEAPPCORE_IID);

  result = context->GetNameSpaceManager(&manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = manager->LookupName("ComposeAppCore", PR_TRUE, classID);
  NS_RELEASE(manager);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsRepository::CreateInstance(classID,
                                        nsnull,
                                        kIDOMComposeAppCoreIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  // XXX We should be calling Init() on the instance

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
// ComposeAppCore class initialization
//
nsresult NS_InitComposeAppCoreClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "ComposeAppCore", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {
 
	if (NS_OK != NS_InitBaseAppCoreClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &ComposeAppCoreClass,      // JSClass
                         ComposeAppCore,            // JSNative ctor
                         0,             // ctor args
                         ComposeAppCoreProperties,  // proto props
                         ComposeAppCoreMethods,     // proto funcs
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
// Method for creating a new ComposeAppCore JavaScript object
//
extern "C" nsresult NS_NewScriptComposeAppCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptComposeAppCore");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMComposeAppCore *aComposeAppCore;

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

  if (NS_OK != NS_InitComposeAppCoreClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIComposeAppCoreIID, (void **)&aComposeAppCore);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ComposeAppCoreClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aComposeAppCore);
  }
  else {
    NS_RELEASE(aComposeAppCore);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
