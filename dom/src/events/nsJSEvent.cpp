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
#include "nsIDOMNode.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMRenderingContext.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kINSEventIID, NS_IDOMNSEVENT_IID);
static NS_DEFINE_IID(kIRenderingContextIID, NS_IDOMRENDERINGCONTEXT_IID);

NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMEvent);
NS_DEF_PTR(nsIDOMNSEvent);
NS_DEF_PTR(nsIDOMRenderingContext);

//
// Event property ids
//
enum Event_slots {
  EVENT_TYPE = -1,
  EVENT_TEXT = -2,
  EVENT_COMMITTEXT = -3,
  EVENT_TARGET = -4,
  EVENT_SCREENX = -5,
  EVENT_SCREENY = -6,
  EVENT_CLIENTX = -7,
  EVENT_CLIENTY = -8,
  EVENT_ALTKEY = -9,
  EVENT_CTRLKEY = -10,
  EVENT_SHIFTKEY = -11,
  EVENT_METAKEY = -12,
  EVENT_CANCELBUBBLE = -13,
  EVENT_CHARCODE = -14,
  EVENT_KEYCODE = -15,
  EVENT_BUTTON = -16,
  NSEVENT_LAYERX = -17,
  NSEVENT_LAYERY = -18,
  NSEVENT_PAGEX = -19,
  NSEVENT_PAGEY = -20,
  NSEVENT_WHICH = -21,
  NSEVENT_RC = -22
};

/***********************************************************************/
//
// Event Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEvent *a = (nsIDOMEvent*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case EVENT_TYPE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetType(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_TEXT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetText(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_COMMITTEXT:
      {
        PRBool prop;
        if (NS_OK == a->GetCommitText(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_TARGET:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetTarget(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_SCREENX:
      {
        PRInt32 prop;
        if (NS_OK == a->GetScreenX(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_SCREENY:
      {
        PRInt32 prop;
        if (NS_OK == a->GetScreenY(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CLIENTX:
      {
        PRInt32 prop;
        if (NS_OK == a->GetClientX(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CLIENTY:
      {
        PRInt32 prop;
        if (NS_OK == a->GetClientY(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_ALTKEY:
      {
        PRBool prop;
        if (NS_OK == a->GetAltKey(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CTRLKEY:
      {
        PRBool prop;
        if (NS_OK == a->GetCtrlKey(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_SHIFTKEY:
      {
        PRBool prop;
        if (NS_OK == a->GetShiftKey(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_METAKEY:
      {
        PRBool prop;
        if (NS_OK == a->GetMetaKey(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CANCELBUBBLE:
      {
        PRBool prop;
        if (NS_OK == a->GetCancelBubble(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_CHARCODE:
      {
        PRUint32 prop;
        if (NS_OK == a->GetCharCode(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_KEYCODE:
      {
        PRUint32 prop;
        if (NS_OK == a->GetKeyCode(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case EVENT_BUTTON:
      {
        PRUint32 prop;
        if (NS_OK == a->GetButton(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_LAYERX:
      {
        PRInt32 prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetLayerX(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_LAYERY:
      {
        PRInt32 prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetLayerY(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_PAGEX:
      {
        PRInt32 prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetPageX(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_PAGEY:
      {
        PRInt32 prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetPageY(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_WHICH:
      {
        PRUint32 prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetWhich(&prop)) {
          *vp = INT_TO_JSVAL(prop);
            NS_RELEASE(b);
          }
          else {
            NS_RELEASE(b);
            return JS_FALSE;
          }
        }
        else {
          JS_ReportError(cx, "Object must be of type NSEvent");
          return JS_FALSE;
        }
        break;
      }
      case NSEVENT_RC:
      {
        nsIDOMRenderingContext* prop;
        nsIDOMNSEvent* b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          if(NS_OK == b->GetRc(&prop)) {
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
          JS_ReportError(cx, "Object must be of type NSEvent");
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
// Event Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMEvent *a = (nsIDOMEvent*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case EVENT_TYPE:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetType(prop);
        
        break;
      }
      case EVENT_COMMITTEXT:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetCommitText(prop);
        
        break;
      }
      case EVENT_TARGET:
      {
        nsIDOMNode* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kINodeIID, "Node",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetTarget(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      case EVENT_SCREENX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetScreenX(prop);
        
        break;
      }
      case EVENT_SCREENY:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetScreenY(prop);
        
        break;
      }
      case EVENT_CLIENTX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetClientX(prop);
        
        break;
      }
      case EVENT_CLIENTY:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetClientY(prop);
        
        break;
      }
      case EVENT_ALTKEY:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetAltKey(prop);
        
        break;
      }
      case EVENT_CTRLKEY:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetCtrlKey(prop);
        
        break;
      }
      case EVENT_SHIFTKEY:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetShiftKey(prop);
        
        break;
      }
      case EVENT_METAKEY:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetMetaKey(prop);
        
        break;
      }
      case EVENT_CANCELBUBBLE:
      {
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetCancelBubble(prop);
        
        break;
      }
      case EVENT_CHARCODE:
      {
        PRUint32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetCharCode(prop);
        
        break;
      }
      case EVENT_KEYCODE:
      {
        PRUint32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetKeyCode(prop);
        
        break;
      }
      case EVENT_BUTTON:
      {
        PRUint32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetButton(prop);
        
        break;
      }
      case NSEVENT_LAYERX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        nsIDOMNSEvent *b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          b->SetLayerX(prop);
          NS_RELEASE(b);
        }
        else {
           
           JS_ReportError(cx, "Object must be of type NSEvent");
           return JS_FALSE;
        }
        
        break;
      }
      case NSEVENT_LAYERY:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        nsIDOMNSEvent *b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          b->SetLayerY(prop);
          NS_RELEASE(b);
        }
        else {
           
           JS_ReportError(cx, "Object must be of type NSEvent");
           return JS_FALSE;
        }
        
        break;
      }
      case NSEVENT_PAGEX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        nsIDOMNSEvent *b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          b->SetPageX(prop);
          NS_RELEASE(b);
        }
        else {
           
           JS_ReportError(cx, "Object must be of type NSEvent");
           return JS_FALSE;
        }
        
        break;
      }
      case NSEVENT_PAGEY:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        nsIDOMNSEvent *b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          b->SetPageY(prop);
          NS_RELEASE(b);
        }
        else {
           
           JS_ReportError(cx, "Object must be of type NSEvent");
           return JS_FALSE;
        }
        
        break;
      }
      case NSEVENT_WHICH:
      {
        PRUint32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRUint32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        nsIDOMNSEvent *b;
        if (NS_OK == a->QueryInterface(kINSEventIID, (void **)&b)) {
          b->SetWhich(prop);
          NS_RELEASE(b);
        }
        else {
           
           JS_ReportError(cx, "Object must be of type NSEvent");
           return JS_FALSE;
        }
        
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


/***********************************************************************/
//
// class for Event
//
JSClass EventClass = {
  "Event", 
  JSCLASS_HAS_PRIVATE,
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
  {"type",    EVENT_TYPE,    JSPROP_ENUMERATE},
  {"text",    EVENT_TEXT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"commitText",    EVENT_COMMITTEXT,    JSPROP_ENUMERATE},
  {"target",    EVENT_TARGET,    JSPROP_ENUMERATE},
  {"screenX",    EVENT_SCREENX,    JSPROP_ENUMERATE},
  {"screenY",    EVENT_SCREENY,    JSPROP_ENUMERATE},
  {"clientX",    EVENT_CLIENTX,    JSPROP_ENUMERATE},
  {"clientY",    EVENT_CLIENTY,    JSPROP_ENUMERATE},
  {"altKey",    EVENT_ALTKEY,    JSPROP_ENUMERATE},
  {"ctrlKey",    EVENT_CTRLKEY,    JSPROP_ENUMERATE},
  {"shiftKey",    EVENT_SHIFTKEY,    JSPROP_ENUMERATE},
  {"metaKey",    EVENT_METAKEY,    JSPROP_ENUMERATE},
  {"cancelBubble",    EVENT_CANCELBUBBLE,    JSPROP_ENUMERATE},
  {"charCode",    EVENT_CHARCODE,    JSPROP_ENUMERATE},
  {"keyCode",    EVENT_KEYCODE,    JSPROP_ENUMERATE},
  {"button",    EVENT_BUTTON,    JSPROP_ENUMERATE},
  {"layerX",    NSEVENT_LAYERX,    JSPROP_ENUMERATE},
  {"layerY",    NSEVENT_LAYERY,    JSPROP_ENUMERATE},
  {"pageX",    NSEVENT_PAGEX,    JSPROP_ENUMERATE},
  {"pageY",    NSEVENT_PAGEY,    JSPROP_ENUMERATE},
  {"which",    NSEVENT_WHICH,    JSPROP_ENUMERATE},
  {"rc",    NSEVENT_RC,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Event class methods
//
static JSFunctionSpec EventMethods[] = 
{
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
      vp = INT_TO_JSVAL(nsIDOMEvent::VK_CANCEL);
      JS_SetProperty(jscontext, constructor, "VK_CANCEL", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_BACK);
      JS_SetProperty(jscontext, constructor, "VK_BACK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_TAB);
      JS_SetProperty(jscontext, constructor, "VK_TAB", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_CLEAR);
      JS_SetProperty(jscontext, constructor, "VK_CLEAR", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_RETURN);
      JS_SetProperty(jscontext, constructor, "VK_RETURN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_ENTER);
      JS_SetProperty(jscontext, constructor, "VK_ENTER", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SHIFT);
      JS_SetProperty(jscontext, constructor, "VK_SHIFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_CONTROL);
      JS_SetProperty(jscontext, constructor, "VK_CONTROL", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_ALT);
      JS_SetProperty(jscontext, constructor, "VK_ALT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_PAUSE);
      JS_SetProperty(jscontext, constructor, "VK_PAUSE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_CAPS_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_CAPS_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_ESCAPE);
      JS_SetProperty(jscontext, constructor, "VK_ESCAPE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SPACE);
      JS_SetProperty(jscontext, constructor, "VK_SPACE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_PAGE_UP);
      JS_SetProperty(jscontext, constructor, "VK_PAGE_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_PAGE_DOWN);
      JS_SetProperty(jscontext, constructor, "VK_PAGE_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_END);
      JS_SetProperty(jscontext, constructor, "VK_END", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_HOME);
      JS_SetProperty(jscontext, constructor, "VK_HOME", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_LEFT);
      JS_SetProperty(jscontext, constructor, "VK_LEFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_UP);
      JS_SetProperty(jscontext, constructor, "VK_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_RIGHT);
      JS_SetProperty(jscontext, constructor, "VK_RIGHT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_DOWN);
      JS_SetProperty(jscontext, constructor, "VK_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_PRINTSCREEN);
      JS_SetProperty(jscontext, constructor, "VK_PRINTSCREEN", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_INSERT);
      JS_SetProperty(jscontext, constructor, "VK_INSERT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_DELETE);
      JS_SetProperty(jscontext, constructor, "VK_DELETE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_0);
      JS_SetProperty(jscontext, constructor, "VK_0", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_1);
      JS_SetProperty(jscontext, constructor, "VK_1", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_2);
      JS_SetProperty(jscontext, constructor, "VK_2", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_3);
      JS_SetProperty(jscontext, constructor, "VK_3", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_4);
      JS_SetProperty(jscontext, constructor, "VK_4", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_5);
      JS_SetProperty(jscontext, constructor, "VK_5", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_6);
      JS_SetProperty(jscontext, constructor, "VK_6", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_7);
      JS_SetProperty(jscontext, constructor, "VK_7", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_8);
      JS_SetProperty(jscontext, constructor, "VK_8", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_9);
      JS_SetProperty(jscontext, constructor, "VK_9", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SEMICOLON);
      JS_SetProperty(jscontext, constructor, "VK_SEMICOLON", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_EQUALS);
      JS_SetProperty(jscontext, constructor, "VK_EQUALS", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_A);
      JS_SetProperty(jscontext, constructor, "VK_A", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_B);
      JS_SetProperty(jscontext, constructor, "VK_B", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_C);
      JS_SetProperty(jscontext, constructor, "VK_C", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_D);
      JS_SetProperty(jscontext, constructor, "VK_D", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_E);
      JS_SetProperty(jscontext, constructor, "VK_E", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F);
      JS_SetProperty(jscontext, constructor, "VK_F", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_G);
      JS_SetProperty(jscontext, constructor, "VK_G", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_H);
      JS_SetProperty(jscontext, constructor, "VK_H", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_I);
      JS_SetProperty(jscontext, constructor, "VK_I", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_J);
      JS_SetProperty(jscontext, constructor, "VK_J", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_K);
      JS_SetProperty(jscontext, constructor, "VK_K", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_L);
      JS_SetProperty(jscontext, constructor, "VK_L", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_M);
      JS_SetProperty(jscontext, constructor, "VK_M", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_N);
      JS_SetProperty(jscontext, constructor, "VK_N", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_O);
      JS_SetProperty(jscontext, constructor, "VK_O", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_P);
      JS_SetProperty(jscontext, constructor, "VK_P", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_Q);
      JS_SetProperty(jscontext, constructor, "VK_Q", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_R);
      JS_SetProperty(jscontext, constructor, "VK_R", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_S);
      JS_SetProperty(jscontext, constructor, "VK_S", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_T);
      JS_SetProperty(jscontext, constructor, "VK_T", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_U);
      JS_SetProperty(jscontext, constructor, "VK_U", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_V);
      JS_SetProperty(jscontext, constructor, "VK_V", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_W);
      JS_SetProperty(jscontext, constructor, "VK_W", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_X);
      JS_SetProperty(jscontext, constructor, "VK_X", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_Y);
      JS_SetProperty(jscontext, constructor, "VK_Y", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_Z);
      JS_SetProperty(jscontext, constructor, "VK_Z", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD0);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD0", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD1);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD1", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD2);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD2", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD3);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD3", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD4);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD4", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD5);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD5", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD6);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD6", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD7);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD7", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD8);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD8", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUMPAD9);
      JS_SetProperty(jscontext, constructor, "VK_NUMPAD9", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_MULTIPLY);
      JS_SetProperty(jscontext, constructor, "VK_MULTIPLY", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_ADD);
      JS_SetProperty(jscontext, constructor, "VK_ADD", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SEPARATOR);
      JS_SetProperty(jscontext, constructor, "VK_SEPARATOR", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SUBTRACT);
      JS_SetProperty(jscontext, constructor, "VK_SUBTRACT", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_DECIMAL);
      JS_SetProperty(jscontext, constructor, "VK_DECIMAL", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_DIVIDE);
      JS_SetProperty(jscontext, constructor, "VK_DIVIDE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F1);
      JS_SetProperty(jscontext, constructor, "VK_F1", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F2);
      JS_SetProperty(jscontext, constructor, "VK_F2", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F3);
      JS_SetProperty(jscontext, constructor, "VK_F3", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F4);
      JS_SetProperty(jscontext, constructor, "VK_F4", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F5);
      JS_SetProperty(jscontext, constructor, "VK_F5", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F6);
      JS_SetProperty(jscontext, constructor, "VK_F6", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F7);
      JS_SetProperty(jscontext, constructor, "VK_F7", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F8);
      JS_SetProperty(jscontext, constructor, "VK_F8", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F9);
      JS_SetProperty(jscontext, constructor, "VK_F9", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F10);
      JS_SetProperty(jscontext, constructor, "VK_F10", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F11);
      JS_SetProperty(jscontext, constructor, "VK_F11", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F12);
      JS_SetProperty(jscontext, constructor, "VK_F12", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F13);
      JS_SetProperty(jscontext, constructor, "VK_F13", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F14);
      JS_SetProperty(jscontext, constructor, "VK_F14", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F15);
      JS_SetProperty(jscontext, constructor, "VK_F15", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F16);
      JS_SetProperty(jscontext, constructor, "VK_F16", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F17);
      JS_SetProperty(jscontext, constructor, "VK_F17", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F18);
      JS_SetProperty(jscontext, constructor, "VK_F18", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F19);
      JS_SetProperty(jscontext, constructor, "VK_F19", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F20);
      JS_SetProperty(jscontext, constructor, "VK_F20", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F21);
      JS_SetProperty(jscontext, constructor, "VK_F21", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F22);
      JS_SetProperty(jscontext, constructor, "VK_F22", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F23);
      JS_SetProperty(jscontext, constructor, "VK_F23", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_F24);
      JS_SetProperty(jscontext, constructor, "VK_F24", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_NUM_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_NUM_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SCROLL_LOCK);
      JS_SetProperty(jscontext, constructor, "VK_SCROLL_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_COMMA);
      JS_SetProperty(jscontext, constructor, "VK_COMMA", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_PERIOD);
      JS_SetProperty(jscontext, constructor, "VK_PERIOD", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_SLASH);
      JS_SetProperty(jscontext, constructor, "VK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_BACK_QUOTE);
      JS_SetProperty(jscontext, constructor, "VK_BACK_QUOTE", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_OPEN_BRACKET);
      JS_SetProperty(jscontext, constructor, "VK_OPEN_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_BACK_SLASH);
      JS_SetProperty(jscontext, constructor, "VK_BACK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_CLOSE_BRACKET);
      JS_SetProperty(jscontext, constructor, "VK_CLOSE_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMEvent::VK_QUOTE);
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
