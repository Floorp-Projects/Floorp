/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);

//
// Event property ids
//
enum Event_slots {
  EVENT_TYPE = -1,
  EVENT_TARGET = -2,
  EVENT_CURRENTNODE = -3,
  EVENT_EVENTPHASE = -4,
  EVENT_BUBBLES = -5,
  EVENT_CANCELABLE = -6
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
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case EVENT_TYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_TYPE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsAutoString prop;
        nsresult result = NS_OK;
        result = a->GetType(prop);
        if (NS_SUCCEEDED(result)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case EVENT_TARGET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_TARGET, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMNode* prop;
        nsresult result = NS_OK;
        result = a->GetTarget(&prop);
        if (NS_SUCCEEDED(result)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case EVENT_CURRENTNODE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_CURRENTNODE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        nsIDOMNode* prop;
        nsresult result = NS_OK;
        result = a->GetCurrentNode(&prop);
        if (NS_SUCCEEDED(result)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case EVENT_EVENTPHASE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_EVENTPHASE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRUint16 prop;
        nsresult result = NS_OK;
        result = a->GetEventPhase(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case EVENT_BUBBLES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_BUBBLES, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRBool prop;
        nsresult result = NS_OK;
        result = a->GetBubbles(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      case EVENT_CANCELABLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_CANCELABLE, PR_FALSE, &ok);
        if (!ok) {
          return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
        }
        PRBool prop;
        nsresult result = NS_OK;
        result = a->GetCancelable(&prop);
        if (NS_SUCCEEDED(result)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return nsJSUtils::nsReportError(cx, obj, result);
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
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
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
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
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

  *rval = JSVAL_NULL;

  {
    PRBool ok;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTBUBBLE, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
    }
  }


    result = nativeThis->PreventBubble();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

  *rval = JSVAL_NULL;

  {
    PRBool ok;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTCAPTURE, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
    }
  }


    result = nativeThis->PreventCapture();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

  *rval = JSVAL_NULL;

  {
    PRBool ok;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTDEFAULT, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
    }
  }


    result = nativeThis->PreventDefault();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method InitEvent
//
PR_STATIC_CALLBACK(JSBool)
EventInitEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEvent *nativeThis = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

  *rval = JSVAL_NULL;

  {
    PRBool ok;
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, secMan,
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECMAN_ERR);
    }
    secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_INITEVENT, PR_FALSE, &ok);
    if (!ok) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_SECURITY_ERR);
    }
  }

    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToBool(&b1, cx, argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->InitEvent(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  {"bubbles",    EVENT_BUBBLES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cancelable",    EVENT_CANCELABLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
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
  {"initEvent",          EventInitEvent,     3},
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
