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
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);

NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMEvent);

//
// Event property ids
//
enum Event_slots {
  EVENT_TYPE = -1,
  EVENT_TARGET = -2,
  EVENT_CURRENTNODE = -3,
  EVENT_EVENTPHASE = -4
};

/***********************************************************************/
//
// Event Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEvent *a = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case EVENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "event.type", &ok);
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
      case EVENT_TARGET:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "event.target", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMNode* prop;
        if (NS_SUCCEEDED(a->GetTarget(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CURRENTNODE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "event.currentnode", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMNode* prop;
        if (NS_SUCCEEDED(a->GetCurrentNode(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_EVENTPHASE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "event.eventphase", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_SUCCEEDED(a->GetEventPhase(&prop))) {
          *vp = INT_TO_JSVAL(prop);
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
// Event Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEvent *a = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case 0:
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
// Event finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeEvent(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Event enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateEvent(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Event resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method PreventBubble
//
PR_STATIC_CALLBACK(JSBool)
EventPreventBubble(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEvent *nativeThis = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "event.preventbubble", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->PreventBubble()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method PreventCapture
//
PR_STATIC_CALLBACK(JSBool)
EventPreventCapture(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEvent *nativeThis = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "event.preventcapture", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->PreventCapture()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method PreventDefault
//
PR_STATIC_CALLBACK(JSBool)
EventPreventDefault(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEvent *nativeThis = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "event.preventdefault", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->PreventDefault()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Event
//
JSClass EventClass = {
  "Event", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetEventProperty,
  SetEventProperty,
  EnumerateEvent,
  ResolveEvent,
  JS_ConvertStub,
  FinalizeEvent
};


//
// Event class properties
//
static JSPropertySpec EventProperties[] =
{
  {"type",    EVENT_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"target",    EVENT_TARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"currentNode",    EVENT_CURRENTNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"eventPhase",    EVENT_EVENTPHASE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Event class methods
//
static JSFunctionSpec EventMethods[] = 
{
  {"preventBubble",          EventPreventBubble,     0},
  {"preventCapture",          EventPreventCapture,     0},
  {"preventDefault",          EventPreventDefault,     0},
  {0}
};


//
// Event constructor
//
PR_STATIC_CALLBACK(JSBool)
Event(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Event class initialization
//
extern "C" NS_DOM nsresult NS_InitEventClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Event", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &EventClass,      // JSClass
                         Event,            // JSNative ctor
                         0,             // ctor args
                         EventProperties,  // proto props
                         EventMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "Event", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMEvent::BUBBLING_PHASE);
      JS_SetProperty(jscontext, constructor, "BUBBLING_PHASE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::CAPTURING_PHASE);
      JS_SetProperty(jscontext, constructor, "CAPTURING_PHASE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::AT_TARGET);
      JS_SetProperty(jscontext, constructor, "AT_TARGET", &vp);

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
// Method for creating a new Event JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptEvent");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMEvent *aEvent;

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

  if (NS_OK != NS_InitEventClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIEventIID, (void **)&aEvent);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &EventClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aEvent);
  }
  else {
    NS_RELEASE(aEvent);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
