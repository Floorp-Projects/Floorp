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
#include "nsIDOMAbstractView.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMNode.h"
#include "nsIDOMUIEvent.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIAbstractViewIID, NS_IDOMABSTRACTVIEW_IID);
static NS_DEFINE_IID(kINSUIEventIID, NS_IDOMNSUIEVENT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIUIEventIID, NS_IDOMUIEVENT_IID);

//
// UIEvent property ids
//
enum UIEvent_slots {
  UIEVENT_VIEW = -1,
  UIEVENT_DETAIL = -2,
  NSUIEVENT_LAYERX = -3,
  NSUIEVENT_LAYERY = -4,
  NSUIEVENT_PAGEX = -5,
  NSUIEVENT_PAGEY = -6,
  NSUIEVENT_WHICH = -7,
  NSUIEVENT_RANGEPARENT = -8,
  NSUIEVENT_RANGEOFFSET = -9,
  NSUIEVENT_CANCELBUBBLE = -10,
  NSUIEVENT_ISCHAR = -11
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case UIEVENT_VIEW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_UIEVENT_VIEW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMAbstractView* prop;
          rv = a->GetView(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case UIEVENT_DETAIL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_UIEVENT_DETAIL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetDetail(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case NSUIEVENT_LAYERX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_LAYERX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetLayerX(&prop);
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
      case NSUIEVENT_LAYERY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_LAYERY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetLayerY(&prop);
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
      case NSUIEVENT_PAGEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_PAGEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetPageX(&prop);
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
      case NSUIEVENT_PAGEY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_PAGEY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetPageY(&prop);
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
      case NSUIEVENT_WHICH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_WHICH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetWhich(&prop);
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
      case NSUIEVENT_RANGEPARENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_RANGEPARENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetRangeParent(&prop);
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
      case NSUIEVENT_RANGEOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_RANGEOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetRangeOffset(&prop);
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
      case NSUIEVENT_CANCELBUBBLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_CANCELBUBBLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetCancelBubble(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case NSUIEVENT_ISCHAR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_ISCHAR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          nsIDOMNSUIEvent* b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            rv = b->GetIsChar(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case NSUIEVENT_CANCELBUBBLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_CANCELBUBBLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          nsIDOMNSUIEvent *b;
          if (NS_OK == a->QueryInterface(kINSUIEventIID, (void **)&b)) {
            b->SetCancelBubble(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
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
// UIEvent class properties
//
static JSPropertySpec UIEventProperties[] =
{
  {"view",    UIEVENT_VIEW,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"detail",    UIEVENT_DETAIL,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"layerX",    NSUIEVENT_LAYERX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"layerY",    NSUIEVENT_LAYERY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pageX",    NSUIEVENT_PAGEX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pageY",    NSUIEVENT_PAGEY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"which",    NSUIEVENT_WHICH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rangeParent",    NSUIEVENT_RANGEPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rangeOffset",    NSUIEVENT_RANGEOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cancelBubble",    NSUIEVENT_CANCELBUBBLE,    JSPROP_ENUMERATE},
  {"isChar",    NSUIEVENT_ISCHAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


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
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// UIEvent resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveUIEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method InitUIEvent
//
PR_STATIC_CALLBACK(JSBool)
UIEventInitUIEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMUIEvent *nativeThis = (nsIDOMUIEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  nsCOMPtr<nsIDOMAbstractView> b3;
  PRInt32 b4;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_UIEVENT_INITUIEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 5) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToBool(&b1, cx, argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b3),
                                           kIAbstractViewIID,
                                           NS_ConvertASCIItoUCS2("AbstractView"),
                                           cx,
                                           argv[3])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[4], (int32 *)&b4)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->InitUIEvent(b0, b1, b2, b3, b4);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetPreventDefault
//
PR_STATIC_CALLBACK(JSBool)
NSUIEventGetPreventDefault(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMUIEvent *privateThis = (nsIDOMUIEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSUIEvent> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSUIEventIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRBool nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSUIEVENT_GETPREVENTDEFAULT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetPreventDefault(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
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
  FinalizeUIEvent,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// UIEvent class methods
//
static JSFunctionSpec UIEventMethods[] = 
{
  {"initUIEvent",          UIEventInitUIEvent,     5},
  {"getPreventDefault",          NSUIEventGetPreventDefault,     0},
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
