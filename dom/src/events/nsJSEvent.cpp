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
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);

//
// Event property ids
//
enum Event_slots {
  EVENT_TYPE = -1,
  EVENT_TARGET = -2,
  EVENT_CURRENTTARGET = -3,
  EVENT_ORIGINALTARGET = -4,
  EVENT_EVENTPHASE = -5,
  EVENT_BUBBLES = -6,
  EVENT_CANCELABLE = -7,
  EVENT_TIMESTAMP = -8
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case EVENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case EVENT_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_TARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMEventTarget* prop;
          rv = a->GetTarget(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case EVENT_CURRENTTARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_CURRENTTARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMEventTarget* prop;
          rv = a->GetCurrentTarget(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case EVENT_ORIGINALTARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_ORIGINALTARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMEventTarget* prop;
          rv = a->GetOriginalTarget(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case EVENT_EVENTPHASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_EVENTPHASE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint16 prop;
          rv = a->GetEventPhase(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case EVENT_BUBBLES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_BUBBLES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetBubbles(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case EVENT_CANCELABLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_CANCELABLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetCancelable(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case EVENT_TIMESTAMP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_TIMESTAMP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint64 prop;
          rv = a->GetTimeStamp(&prop);
          if (NS_SUCCEEDED(rv)) {
#ifdef HAVE_LONG_LONG
            *vp = INT_TO_JSVAL(prop);
#else
            int i;
            LL_L2UI(i, prop);
            *vp = INT_TO_JSVAL(i);
#endif
          }
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

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case 0:
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}


//
// Event class properties
//
static JSPropertySpec EventProperties[] =
{
  {"type",    EVENT_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"target",    EVENT_TARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"currentTarget",    EVENT_CURRENTTARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"originalTarget",    EVENT_ORIGINALTARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"eventPhase",    EVENT_EVENTPHASE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"bubbles",    EVENT_BUBBLES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cancelable",    EVENT_CANCELABLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"timeStamp",    EVENT_TIMESTAMP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


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
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// Event resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method StopPropagation
//
PR_STATIC_CALLBACK(JSBool)
EventStopPropagation(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMEvent *nativeThis = (nsIDOMEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_STOPPROPAGATION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->StopPropagation();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
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
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTBUBBLE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTCAPTURE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_PREVENTDEFAULT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENT_INITEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  FinalizeEvent,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// Event class methods
//
static JSFunctionSpec EventMethods[] = 
{
  {"stopPropagation",          EventStopPropagation,     0},
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
      vp = INT_TO_JSVAL(nsIDOMEvent::CAPTURING_PHASE);
      JS_SetProperty(jscontext, constructor, "CAPTURING_PHASE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::AT_TARGET);
      JS_SetProperty(jscontext, constructor, "AT_TARGET", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::BUBBLING_PHASE);
      JS_SetProperty(jscontext, constructor, "BUBBLING_PHASE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEDOWN);
      JS_SetProperty(jscontext, constructor, "MOUSEDOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEUP);
      JS_SetProperty(jscontext, constructor, "MOUSEUP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEOVER);
      JS_SetProperty(jscontext, constructor, "MOUSEOVER", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEOUT);
      JS_SetProperty(jscontext, constructor, "MOUSEOUT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEMOVE);
      JS_SetProperty(jscontext, constructor, "MOUSEMOVE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOUSEDRAG);
      JS_SetProperty(jscontext, constructor, "MOUSEDRAG", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::CLICK);
      JS_SetProperty(jscontext, constructor, "CLICK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::DBLCLICK);
      JS_SetProperty(jscontext, constructor, "DBLCLICK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::KEYDOWN);
      JS_SetProperty(jscontext, constructor, "KEYDOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::KEYUP);
      JS_SetProperty(jscontext, constructor, "KEYUP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::KEYPRESS);
      JS_SetProperty(jscontext, constructor, "KEYPRESS", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::DRAGDROP);
      JS_SetProperty(jscontext, constructor, "DRAGDROP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::FOCUS);
      JS_SetProperty(jscontext, constructor, "FOCUS", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::BLUR);
      JS_SetProperty(jscontext, constructor, "BLUR", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::SELECT);
      JS_SetProperty(jscontext, constructor, "SELECT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::CHANGE);
      JS_SetProperty(jscontext, constructor, "CHANGE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::RESET);
      JS_SetProperty(jscontext, constructor, "RESET", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::SUBMIT);
      JS_SetProperty(jscontext, constructor, "SUBMIT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::SCROLL);
      JS_SetProperty(jscontext, constructor, "SCROLL", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::LOAD);
      JS_SetProperty(jscontext, constructor, "LOAD", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::UNLOAD);
      JS_SetProperty(jscontext, constructor, "UNLOAD", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::XFER_DONE);
      JS_SetProperty(jscontext, constructor, "XFER_DONE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::ABORT);
      JS_SetProperty(jscontext, constructor, "ABORT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::ERROR);
      JS_SetProperty(jscontext, constructor, "ERROR", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::LOCATE);
      JS_SetProperty(jscontext, constructor, "LOCATE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::MOVE);
      JS_SetProperty(jscontext, constructor, "MOVE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::RESIZE);
      JS_SetProperty(jscontext, constructor, "RESIZE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::FORWARD);
      JS_SetProperty(jscontext, constructor, "FORWARD", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::HELP);
      JS_SetProperty(jscontext, constructor, "HELP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::BACK);
      JS_SetProperty(jscontext, constructor, "BACK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::TEXT);
      JS_SetProperty(jscontext, constructor, "TEXT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::ALT_MASK);
      JS_SetProperty(jscontext, constructor, "ALT_MASK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::CONTROL_MASK);
      JS_SetProperty(jscontext, constructor, "CONTROL_MASK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::SHIFT_MASK);
      JS_SetProperty(jscontext, constructor, "SHIFT_MASK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::META_MASK);
      JS_SetProperty(jscontext, constructor, "META_MASK", &vp);

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
