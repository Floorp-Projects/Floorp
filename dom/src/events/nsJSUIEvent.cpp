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
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMRenderingContext.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINSUIEventIID, NS_IDOMNSUIEVENT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIUIEventIID, NS_IDOMUIEVENT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);

NS_DEF_PTR(nsIDOMNSUIEvent);
NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMUIEvent);
NS_DEF_PTR(nsIDOMRenderingContext);

//
// UIEvent property ids
//
enum UIEvent_slots {
  UIEVENT_SCREENX = -1,
  UIEVENT_SCREENY = -2,
  UIEVENT_CLIENTX = -3,
  UIEVENT_CLIENTY = -4,
  UIEVENT_ALTKEY = -5,
  UIEVENT_CTRLKEY = -6,
  UIEVENT_SHIFTKEY = -7,
  UIEVENT_METAKEY = -8,
  UIEVENT_CHARCODE = -9,
  UIEVENT_KEYCODE = -10,
  UIEVENT_BUTTON = -11,
  UIEVENT_CLICKCOUNT = -12,
  NSUIEVENT_LAYERX = -13,
  NSUIEVENT_LAYERY = -14,
  NSUIEVENT_PAGEX = -15,
  NSUIEVENT_PAGEY = -16,
  NSUIEVENT_WHICH = -17,
  NSUIEVENT_RANGEPARENT = -18,
  NSUIEVENT_RANGEOFFSET = -19,
  NSUIEVENT_RC = -20
};

/***********************************************************************/
//
// UIEvent Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetUIEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMUIEvent *a = (nsIDOMUIEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case UIEVENT_SCREENX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.screenx", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetScreenX(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_SCREENY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.screeny", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetScreenY(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_CLIENTX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.clientx", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetClientX(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_CLIENTY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.clienty", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetClientY(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_ALTKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.altkey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetAltKey(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_CTRLKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.ctrlkey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetCtrlKey(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_SHIFTKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.shiftkey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetShiftKey(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_METAKEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.metakey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetMetaKey(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_CHARCODE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.charcode", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint32 prop;
        if (NS_SUCCEEDED(a->GetCharCode(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_KEYCODE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.keycode", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint32 prop;
        if (NS_SUCCEEDED(a->GetKeyCode(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_BUTTON:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.button", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_SUCCEEDED(a->GetButton(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case UIEVENT_CLICKCOUNT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "uievent.clickcount", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint16 prop;
        if (NS_SUCCEEDED(a->GetClickcount(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_LAYERX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.layerx", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetLayerX(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_LAYERY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.layery", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetLayerY(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_PAGEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.pagex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetPageX(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_PAGEY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.pagey", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetPageY(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_WHICH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.which", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRUint32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetWhich(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_RANGEPARENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.rangeparent", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMNode* prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetRangeParent(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_RANGEOFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.rangeoffset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetRangeOffset(&prop))) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSUIEVENT_RC:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "nsuievent.rc", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMRenderingContext* prop;
        nsIDOMNSUIEvent* b;
        if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
          if(NS_SUCCEEDED(b->GetRc(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSUIEvent");
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
// UIEvent Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetUIEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMUIEvent *a = (nsIDOMUIEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// UIEvent finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeUIEvent(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// UIEvent enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateUIEvent(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// UIEvent resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveUIEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for UIEvent
//
JSClass UIEventClass = {
  "UIEvent", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetUIEventProperty,
  SetUIEventProperty,
  EnumerateUIEvent,
  ResolveUIEvent,
  JS_ConvertStub,
  FinalizeUIEvent
};


//
// UIEvent class properties
//
static JSPropertySpec UIEventProperties[] =
{
  {"screenX",    UIEVENT_SCREENX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"screenY",    UIEVENT_SCREENY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"clientX",    UIEVENT_CLIENTX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"clientY",    UIEVENT_CLIENTY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"altKey",    UIEVENT_ALTKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"ctrlKey",    UIEVENT_CTRLKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"shiftKey",    UIEVENT_SHIFTKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"metaKey",    UIEVENT_METAKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"charCode",    UIEVENT_CHARCODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"keyCode",    UIEVENT_KEYCODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"button",    UIEVENT_BUTTON,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"clickcount",    UIEVENT_CLICKCOUNT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"layerX",    NSUIEVENT_LAYERX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"layerY",    NSUIEVENT_LAYERY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pageX",    NSUIEVENT_PAGEX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pageY",    NSUIEVENT_PAGEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"which",    NSUIEVENT_WHICH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rangeParent",    NSUIEVENT_RANGEPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rangeOffset",    NSUIEVENT_RANGEOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rc",    NSUIEVENT_RC,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// UIEvent class methods
//
static JSFunctionSpec UIEventMethods[] = 
{
  {0}
};


//
// UIEvent constructor
//
PR_STATIC_CALLBACK(JSBool)
UIEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// UIEvent class initialization
//
extern "C" NS_DOM nsresult NS_InitUIEventClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "UIEvent", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitEventClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &UIEventClass,      // JSClass
                         UIEvent,            // JSNative ctor
                         0,             // ctor args
                         UIEventProperties,  // proto props
                         UIEventMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "UIEvent", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_CANCEL);
      JS_SetProperty(jscontext, constructor, "VK_CANCEL", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_BACK);
      JS_SetProperty(jscontext, constructor, "VK_BACK", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_TAB);
      JS_SetProperty(jscontext, constructor, "VK_TAB", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_CLEAR);
      JS_SetProperty(jscontext, constructor, "VK_CLEAR", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_RETURN);
      JS_SetProperty(jscontext, constructor, "VK_RETURN", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_ENTER);
      JS_SetProperty(jscontext, constructor, "VK_ENTER", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SHIFT);
      JS_SetProperty(jscontext, constructor, "VK_SHIFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_CONTROL);
      JS_SetProperty(jscontext, constructor, "VK_CONTROL", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_ALT);
      JS_SetProperty(jscontext, constructor, "VK_ALT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_PAUSE);
      JS_SetProperty(jscontext, constructor, "VK_PAUSE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_CAPS_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_CAPS_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_ESCAPE);
      JS_SetProperty(jscontext, constructor, "VK_ESCAPE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SPACE);
      JS_SetProperty(jscontext, constructor, "VK_SPACE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_PAGE_UP);
      JS_SetProperty(jscontext, constructor, "VK_PAGE_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_PAGE_DOWN);
      JS_SetProperty(jscontext, constructor, "VK_PAGE_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_END);
      JS_SetProperty(jscontext, constructor, "VK_END", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_HOME);
      JS_SetProperty(jscontext, constructor, "VK_HOME", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_LEFT);
      JS_SetProperty(jscontext, constructor, "VK_LEFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_UP);
      JS_SetProperty(jscontext, constructor, "VK_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_RIGHT);
      JS_SetProperty(jscontext, constructor, "VK_RIGHT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_DOWN);
      JS_SetProperty(jscontext, constructor, "VK_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_PRINTSCREEN);
      JS_SetProperty(jscontext, constructor, "VK_PRINTSCREEN", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_INSERT);
      JS_SetProperty(jscontext, constructor, "VK_INSERT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_DELETE);
      JS_SetProperty(jscontext, constructor, "VK_DELETE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_0);
      JS_SetProperty(jscontext, constructor, "VK_0", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_1);
      JS_SetProperty(jscontext, constructor, "VK_1", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_2);
      JS_SetProperty(jscontext, constructor, "VK_2", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_3);
      JS_SetProperty(jscontext, constructor, "VK_3", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_4);
      JS_SetProperty(jscontext, constructor, "VK_4", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_5);
      JS_SetProperty(jscontext, constructor, "VK_5", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_6);
      JS_SetProperty(jscontext, constructor, "VK_6", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_7);
      JS_SetProperty(jscontext, constructor, "VK_7", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_8);
      JS_SetProperty(jscontext, constructor, "VK_8", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_9);
      JS_SetProperty(jscontext, constructor, "VK_9", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SEMICOLON);
      JS_SetProperty(jscontext, constructor, "VK_SEMICOLON", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_EQUALS);
      JS_SetProperty(jscontext, constructor, "VK_EQUALS", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_A);
      JS_SetProperty(jscontext, constructor, "VK_A", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_B);
      JS_SetProperty(jscontext, constructor, "VK_B", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_C);
      JS_SetProperty(jscontext, constructor, "VK_C", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_D);
      JS_SetProperty(jscontext, constructor, "VK_D", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_E);
      JS_SetProperty(jscontext, constructor, "VK_E", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F);
      JS_SetProperty(jscontext, constructor, "VK_F", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_G);
      JS_SetProperty(jscontext, constructor, "VK_G", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_H);
      JS_SetProperty(jscontext, constructor, "VK_H", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_I);
      JS_SetProperty(jscontext, constructor, "VK_I", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_J);
      JS_SetProperty(jscontext, constructor, "VK_J", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_K);
      JS_SetProperty(jscontext, constructor, "VK_K", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_L);
      JS_SetProperty(jscontext, constructor, "VK_L", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_M);
      JS_SetProperty(jscontext, constructor, "VK_M", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_N);
      JS_SetProperty(jscontext, constructor, "VK_N", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_O);
      JS_SetProperty(jscontext, constructor, "VK_O", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_P);
      JS_SetProperty(jscontext, constructor, "VK_P", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_Q);
      JS_SetProperty(jscontext, constructor, "VK_Q", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_R);
      JS_SetProperty(jscontext, constructor, "VK_R", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_S);
      JS_SetProperty(jscontext, constructor, "VK_S", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_T);
      JS_SetProperty(jscontext, constructor, "VK_T", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_U);
      JS_SetProperty(jscontext, constructor, "VK_U", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_V);
      JS_SetProperty(jscontext, constructor, "VK_V", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_W);
      JS_SetProperty(jscontext, constructor, "VK_W", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_X);
      JS_SetProperty(jscontext, constructor, "VK_X", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_Y);
      JS_SetProperty(jscontext, constructor, "VK_Y", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_Z);
      JS_SetProperty(jscontext, constructor, "VK_Z", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD0);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD0", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD1);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD1", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD2);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD2", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD3);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD3", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD4);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD4", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD5);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD5", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD6);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD6", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD7);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD7", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD8);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD8", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUMPAD9);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD9", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_MULTIPLY);
      JS_SetProperty(jscontext, constructor, "VK_MULTIPLY", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_ADD);
      JS_SetProperty(jscontext, constructor, "VK_ADD", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SEPARATOR);
      JS_SetProperty(jscontext, constructor, "VK_SEPARATOR", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SUBTRACT);
      JS_SetProperty(jscontext, constructor, "VK_SUBTRACT", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_DECIMAL);
      JS_SetProperty(jscontext, constructor, "VK_DECIMAL", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_DIVIDE);
      JS_SetProperty(jscontext, constructor, "VK_DIVIDE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F1);
      JS_SetProperty(jscontext, constructor, "VK_F1", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F2);
      JS_SetProperty(jscontext, constructor, "VK_F2", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F3);
      JS_SetProperty(jscontext, constructor, "VK_F3", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F4);
      JS_SetProperty(jscontext, constructor, "VK_F4", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F5);
      JS_SetProperty(jscontext, constructor, "VK_F5", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F6);
      JS_SetProperty(jscontext, constructor, "VK_F6", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F7);
      JS_SetProperty(jscontext, constructor, "VK_F7", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F8);
      JS_SetProperty(jscontext, constructor, "VK_F8", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F9);
      JS_SetProperty(jscontext, constructor, "VK_F9", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F10);
      JS_SetProperty(jscontext, constructor, "VK_F10", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F11);
      JS_SetProperty(jscontext, constructor, "VK_F11", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F12);
      JS_SetProperty(jscontext, constructor, "VK_F12", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F13);
      JS_SetProperty(jscontext, constructor, "VK_F13", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F14);
      JS_SetProperty(jscontext, constructor, "VK_F14", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F15);
      JS_SetProperty(jscontext, constructor, "VK_F15", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F16);
      JS_SetProperty(jscontext, constructor, "VK_F16", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F17);
      JS_SetProperty(jscontext, constructor, "VK_F17", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F18);
      JS_SetProperty(jscontext, constructor, "VK_F18", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F19);
      JS_SetProperty(jscontext, constructor, "VK_F19", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F20);
      JS_SetProperty(jscontext, constructor, "VK_F20", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F21);
      JS_SetProperty(jscontext, constructor, "VK_F21", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F22);
      JS_SetProperty(jscontext, constructor, "VK_F22", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F23);
      JS_SetProperty(jscontext, constructor, "VK_F23", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_F24);
      JS_SetProperty(jscontext, constructor, "VK_F24", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_NUM_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_NUM_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SCROLL_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_SCROLL_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_COMMA);
      JS_SetProperty(jscontext, constructor, "VK_COMMA", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_PERIOD);
      JS_SetProperty(jscontext, constructor, "VK_PERIOD", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_SLASH);
      JS_SetProperty(jscontext, constructor, "VK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_BACK_QUOTE);
      JS_SetProperty(jscontext, constructor, "VK_BACK_QUOTE", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_OPEN_BRACKET);
      JS_SetProperty(jscontext, constructor, "VK_OPEN_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_BACK_SLASH);
      JS_SetProperty(jscontext, constructor, "VK_BACK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_CLOSE_BRACKET);
      JS_SetProperty(jscontext, constructor, "VK_CLOSE_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMUIEvent::VK_QUOTE);
      JS_SetProperty(jscontext, constructor, "VK_QUOTE", &vp);

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
// Method for creating a new UIEvent JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptUIEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptUIEvent");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMUIEvent *aUIEvent;

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

  if (NS_OK != NS_InitUIEventClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIUIEventIID, (void **)&aUIEvent);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &UIEventClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aUIEvent);
  }
  else {
    NS_RELEASE(aUIEvent);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
