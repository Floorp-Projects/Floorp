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
#include "nsIDOMMouseEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMEventTarget.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIMouseEventIID, NS_IDOMMOUSEEVENT_IID);
static NS_DEFINE_IID(kIKeyEventIID, NS_IDOMKEYEVENT_IID);
static NS_DEFINE_IID(kIAbstractViewIID, NS_IDOMABSTRACTVIEW_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);

//
// KeyEvent property ids
//
enum KeyEvent_slots {
  KEYEVENT_CHARCODE = -1,
  KEYEVENT_KEYCODE = -2,
  KEYEVENT_ALTKEY = -3,
  KEYEVENT_CTRLKEY = -4,
  KEYEVENT_SHIFTKEY = -5,
  KEYEVENT_METAKEY = -6,
  MOUSEEVENT_SCREENX = -7,
  MOUSEEVENT_SCREENY = -8,
  MOUSEEVENT_CLIENTX = -9,
  MOUSEEVENT_CLIENTY = -10,
  MOUSEEVENT_BUTTON = -11,
  MOUSEEVENT_RELATEDTARGET = -12
};

/***********************************************************************/
//
// KeyEvent Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetKeyEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMKeyEvent *a = (nsIDOMKeyEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case KEYEVENT_CHARCODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_CHARCODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetCharCode(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case KEYEVENT_KEYCODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_KEYCODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetKeyCode(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case KEYEVENT_ALTKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_ALTKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetAltKey(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case KEYEVENT_CTRLKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_CTRLKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetCtrlKey(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case KEYEVENT_SHIFTKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_SHIFTKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetShiftKey(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case KEYEVENT_METAKEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_METAKEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetMetaKey(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case MOUSEEVENT_SCREENX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_SCREENX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetScreenX(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case MOUSEEVENT_SCREENY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_SCREENY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetScreenY(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case MOUSEEVENT_CLIENTX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_CLIENTX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetClientX(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case MOUSEEVENT_CLIENTY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_CLIENTY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetClientY(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case MOUSEEVENT_BUTTON:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_BUTTON, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint16 prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetButton(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case MOUSEEVENT_RELATEDTARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_RELATEDTARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMEventTarget* prop;
          nsIDOMMouseEvent* b;
          if (NS_OK == a->QueryInterface(kIMouseEventIID, (void **)&b)) {
            rv = b->GetRelatedTarget(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
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
// KeyEvent Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetKeyEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMKeyEvent *a = (nsIDOMKeyEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// KeyEvent finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeKeyEvent(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// KeyEvent enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateKeyEvent(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// KeyEvent resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveKeyEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method InitKeyEvent
//
PR_STATIC_CALLBACK(JSBool)
KeyEventInitKeyEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMKeyEvent *nativeThis = (nsIDOMKeyEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  PRBool b3;
  PRBool b4;
  PRBool b5;
  PRBool b6;
  PRUint32 b7;
  PRUint32 b8;
  nsCOMPtr<nsIDOMAbstractView> b9;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_KEYEVENT_INITKEYEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 10) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToBool(&b1, cx, argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b4, cx, argv[4])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b5, cx, argv[5])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b6, cx, argv[6])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[7], (int32 *)&b7)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[8], (int32 *)&b8)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b9),
                                           kIAbstractViewIID,
                                           NS_ConvertASCIItoUCS2("AbstractView"),
                                           cx,
                                           argv[9])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->InitKeyEvent(b0, b1, b2, b3, b4, b5, b6, b7, b8, b9);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method InitMouseEvent
//
PR_STATIC_CALLBACK(JSBool)
MouseEventInitMouseEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMKeyEvent *privateThis = (nsIDOMKeyEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMMouseEvent> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIMouseEventIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  PRBool b3;
  PRBool b4;
  PRInt32 b5;
  PRInt32 b6;
  PRInt32 b7;
  PRInt32 b8;
  PRUint32 b9;
  PRUint32 b10;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MOUSEEVENT_INITMOUSEEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 11) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToBool(&b1, cx, argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b4, cx, argv[4])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[5], (int32 *)&b5)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[6], (int32 *)&b6)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[7], (int32 *)&b7)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[8], (int32 *)&b8)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[9], (int32 *)&b9)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[10], (int32 *)&b10)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->InitMouseEvent(b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for KeyEvent
//
JSClass KeyEventClass = {
  "KeyEvent", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetKeyEventProperty,
  SetKeyEventProperty,
  EnumerateKeyEvent,
  ResolveKeyEvent,
  JS_ConvertStub,
  FinalizeKeyEvent,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// KeyEvent class properties
//
static JSPropertySpec KeyEventProperties[] =
{
  {"charCode",    KEYEVENT_CHARCODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"keyCode",    KEYEVENT_KEYCODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"altKey",    KEYEVENT_ALTKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"ctrlKey",    KEYEVENT_CTRLKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"shiftKey",    KEYEVENT_SHIFTKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"metaKey",    KEYEVENT_METAKEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"screenX",    MOUSEEVENT_SCREENX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"screenY",    MOUSEEVENT_SCREENY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"clientX",    MOUSEEVENT_CLIENTX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"clientY",    MOUSEEVENT_CLIENTY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"button",    MOUSEEVENT_BUTTON,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"relatedTarget",    MOUSEEVENT_RELATEDTARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// KeyEvent class methods
//
static JSFunctionSpec KeyEventMethods[] = 
{
  {"initKeyEvent",          KeyEventInitKeyEvent,     10},
  {"initMouseEvent",          MouseEventInitMouseEvent,     11},
  {0}
};


//
// KeyEvent constructor
//
PR_STATIC_CALLBACK(JSBool)
KeyEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// KeyEvent class initialization
//
extern "C" NS_DOM nsresult NS_InitKeyEventClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "KeyEvent", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitUIEventClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &KeyEventClass,      // JSClass
                         KeyEvent,            // JSNative ctor
                         0,             // ctor args
                         KeyEventProperties,  // proto props
                         KeyEventMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "KeyEvent", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_CANCEL);
      JS_SetProperty(jscontext, constructor, "DOM_VK_CANCEL", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_BACK_SPACE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_BACK_SPACE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_TAB);
      JS_SetProperty(jscontext, constructor, "DOM_VK_TAB", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_CLEAR);
      JS_SetProperty(jscontext, constructor, "DOM_VK_CLEAR", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_RETURN);
      JS_SetProperty(jscontext, constructor, "DOM_VK_RETURN", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_ENTER);
      JS_SetProperty(jscontext, constructor, "DOM_VK_ENTER", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SHIFT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SHIFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_CONTROL);
      JS_SetProperty(jscontext, constructor, "DOM_VK_CONTROL", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_ALT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_ALT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_PAUSE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_PAUSE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_CAPS_LOCK);
      JS_SetProperty(jscontext, constructor, "DOM_VK_CAPS_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_ESCAPE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_ESCAPE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SPACE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SPACE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_PAGE_UP);
      JS_SetProperty(jscontext, constructor, "DOM_VK_PAGE_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_PAGE_DOWN);
      JS_SetProperty(jscontext, constructor, "DOM_VK_PAGE_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_END);
      JS_SetProperty(jscontext, constructor, "DOM_VK_END", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_HOME);
      JS_SetProperty(jscontext, constructor, "DOM_VK_HOME", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_LEFT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_LEFT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_UP);
      JS_SetProperty(jscontext, constructor, "DOM_VK_UP", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_RIGHT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_RIGHT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_DOWN);
      JS_SetProperty(jscontext, constructor, "DOM_VK_DOWN", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_PRINTSCREEN);
      JS_SetProperty(jscontext, constructor, "DOM_VK_PRINTSCREEN", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_INSERT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_INSERT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_DELETE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_DELETE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_0);
      JS_SetProperty(jscontext, constructor, "DOM_VK_0", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_1);
      JS_SetProperty(jscontext, constructor, "DOM_VK_1", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_2);
      JS_SetProperty(jscontext, constructor, "DOM_VK_2", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_3);
      JS_SetProperty(jscontext, constructor, "DOM_VK_3", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_4);
      JS_SetProperty(jscontext, constructor, "DOM_VK_4", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_5);
      JS_SetProperty(jscontext, constructor, "DOM_VK_5", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_6);
      JS_SetProperty(jscontext, constructor, "DOM_VK_6", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_7);
      JS_SetProperty(jscontext, constructor, "DOM_VK_7", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_8);
      JS_SetProperty(jscontext, constructor, "DOM_VK_8", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_9);
      JS_SetProperty(jscontext, constructor, "DOM_VK_9", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SEMICOLON);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SEMICOLON", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_EQUALS);
      JS_SetProperty(jscontext, constructor, "DOM_VK_EQUALS", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_A);
      JS_SetProperty(jscontext, constructor, "DOM_VK_A", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_B);
      JS_SetProperty(jscontext, constructor, "DOM_VK_B", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_C);
      JS_SetProperty(jscontext, constructor, "DOM_VK_C", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_D);
      JS_SetProperty(jscontext, constructor, "DOM_VK_D", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_E);
      JS_SetProperty(jscontext, constructor, "DOM_VK_E", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_G);
      JS_SetProperty(jscontext, constructor, "DOM_VK_G", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_H);
      JS_SetProperty(jscontext, constructor, "DOM_VK_H", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_I);
      JS_SetProperty(jscontext, constructor, "DOM_VK_I", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_J);
      JS_SetProperty(jscontext, constructor, "DOM_VK_J", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_K);
      JS_SetProperty(jscontext, constructor, "DOM_VK_K", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_L);
      JS_SetProperty(jscontext, constructor, "DOM_VK_L", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_M);
      JS_SetProperty(jscontext, constructor, "DOM_VK_M", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_N);
      JS_SetProperty(jscontext, constructor, "DOM_VK_N", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_O);
      JS_SetProperty(jscontext, constructor, "DOM_VK_O", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_P);
      JS_SetProperty(jscontext, constructor, "DOM_VK_P", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_Q);
      JS_SetProperty(jscontext, constructor, "DOM_VK_Q", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_R);
      JS_SetProperty(jscontext, constructor, "DOM_VK_R", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_S);
      JS_SetProperty(jscontext, constructor, "DOM_VK_S", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_T);
      JS_SetProperty(jscontext, constructor, "DOM_VK_T", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_U);
      JS_SetProperty(jscontext, constructor, "DOM_VK_U", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_V);
      JS_SetProperty(jscontext, constructor, "DOM_VK_V", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_W);
      JS_SetProperty(jscontext, constructor, "DOM_VK_W", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_X);
      JS_SetProperty(jscontext, constructor, "DOM_VK_X", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_Y);
      JS_SetProperty(jscontext, constructor, "DOM_VK_Y", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_Z);
      JS_SetProperty(jscontext, constructor, "DOM_VK_Z", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD0);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD0", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD1);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD1", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD2);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD2", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD3);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD3", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD4);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD4", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD5);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD5", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD6);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD6", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD7);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD7", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD8);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD8", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUMPAD9);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUMPAD9", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_MULTIPLY);
      JS_SetProperty(jscontext, constructor, "DOM_VK_MULTIPLY", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_ADD);
      JS_SetProperty(jscontext, constructor, "DOM_VK_ADD", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SEPARATOR);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SEPARATOR", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SUBTRACT);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SUBTRACT", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_DECIMAL);
      JS_SetProperty(jscontext, constructor, "DOM_VK_DECIMAL", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_DIVIDE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_DIVIDE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F1);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F1", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F2);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F2", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F3);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F3", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F4);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F4", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F5);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F5", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F6);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F6", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F7);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F7", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F8);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F8", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F9);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F9", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F10);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F10", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F11);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F11", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F12);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F12", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F13);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F13", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F14);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F14", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F15);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F15", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F16);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F16", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F17);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F17", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F18);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F18", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F19);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F19", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F20);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F20", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F21);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F21", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F22);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F22", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F23);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F23", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_F24);
      JS_SetProperty(jscontext, constructor, "DOM_VK_F24", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_NUM_LOCK);
      JS_SetProperty(jscontext, constructor, "DOM_VK_NUM_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SCROLL_LOCK", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_COMMA);
      JS_SetProperty(jscontext, constructor, "DOM_VK_COMMA", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_PERIOD);
      JS_SetProperty(jscontext, constructor, "DOM_VK_PERIOD", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_SLASH);
      JS_SetProperty(jscontext, constructor, "DOM_VK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_BACK_QUOTE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_BACK_QUOTE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET);
      JS_SetProperty(jscontext, constructor, "DOM_VK_OPEN_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_BACK_SLASH);
      JS_SetProperty(jscontext, constructor, "DOM_VK_BACK_SLASH", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET);
      JS_SetProperty(jscontext, constructor, "DOM_VK_CLOSE_BRACKET", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_QUOTE);
      JS_SetProperty(jscontext, constructor, "DOM_VK_QUOTE", &vp);

      vp = INT_TO_JSVAL(nsIDOMKeyEvent::DOM_VK_META);
      JS_SetProperty(jscontext, constructor, "DOM_VK_META", &vp);

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
// Method for creating a new KeyEvent JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptKeyEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptKeyEvent");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMKeyEvent *aKeyEvent;

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

  if (NS_OK != NS_InitKeyEventClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIKeyEventIID, (void **)&aKeyEvent);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &KeyEventClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aKeyEvent);
  }
  else {
    NS_RELEASE(aKeyEvent);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
