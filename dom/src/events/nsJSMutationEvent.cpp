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
#include "nsIDOMMutationEvent.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIMutationEventIID, NS_IDOMMUTATIONEVENT_IID);

//
// MutationEvent property ids
//
enum MutationEvent_slots {
  MUTATIONEVENT_RELATEDNODE = -1,
  MUTATIONEVENT_PREVVALUE = -2,
  MUTATIONEVENT_NEWVALUE = -3,
  MUTATIONEVENT_ATTRNAME = -4,
  MUTATIONEVENT_ATTRCHANGE = -5
};

/***********************************************************************/
//
// MutationEvent Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetMutationEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMutationEvent *a = (nsIDOMMutationEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case MUTATIONEVENT_RELATEDNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_RELATEDNODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetRelatedNode(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case MUTATIONEVENT_PREVVALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_PREVVALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPrevValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case MUTATIONEVENT_NEWVALUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_NEWVALUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetNewValue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case MUTATIONEVENT_ATTRNAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_ATTRNAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAttrName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case MUTATIONEVENT_ATTRCHANGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_ATTRCHANGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint16 prop;
          rv = a->GetAttrChange(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
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
// MutationEvent Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetMutationEventProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMMutationEvent *a = (nsIDOMMutationEvent*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// MutationEvent class properties
//
static JSPropertySpec MutationEventProperties[] =
{
  {"relatedNode",    MUTATIONEVENT_RELATEDNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"prevValue",    MUTATIONEVENT_PREVVALUE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"newValue",    MUTATIONEVENT_NEWVALUE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"attrName",    MUTATIONEVENT_ATTRNAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"attrChange",    MUTATIONEVENT_ATTRCHANGE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// MutationEvent finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeMutationEvent(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// MutationEvent enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateMutationEvent(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// MutationEvent resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveMutationEvent(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method InitMutationEvent
//
PR_STATIC_CALLBACK(JSBool)
MutationEventInitMutationEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMMutationEvent *nativeThis = (nsIDOMMutationEvent*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString b0;
  PRBool b1;
  PRBool b2;
  nsCOMPtr<nsIDOMNode> b3;
  nsAutoString b4;
  nsAutoString b5;
  nsAutoString b6;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_MUTATIONEVENT_INITMUTATIONEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 7) {
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
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[3])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);
    nsJSUtils::nsConvertJSValToString(b5, cx, argv[5]);
    nsJSUtils::nsConvertJSValToString(b6, cx, argv[6]);

    result = nativeThis->InitMutationEvent(b0, b1, b2, b3, b4, b5, b6);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for MutationEvent
//
JSClass MutationEventClass = {
  "MutationEvent", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetMutationEventProperty,
  SetMutationEventProperty,
  EnumerateMutationEvent,
  ResolveMutationEvent,
  JS_ConvertStub,
  FinalizeMutationEvent,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// MutationEvent class methods
//
static JSFunctionSpec MutationEventMethods[] = 
{
  {"initMutationEvent",          MutationEventInitMutationEvent,     7},
  {0}
};


//
// MutationEvent constructor
//
PR_STATIC_CALLBACK(JSBool)
MutationEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// MutationEvent class initialization
//
extern "C" NS_DOM nsresult NS_InitMutationEventClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "MutationEvent", &vp)) ||
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
                         &MutationEventClass,      // JSClass
                         MutationEvent,            // JSNative ctor
                         0,             // ctor args
                         MutationEventProperties,  // proto props
                         MutationEventMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "MutationEvent", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMMutationEvent::MODIFICATION);
      JS_SetProperty(jscontext, constructor, "MODIFICATION", &vp);

      vp = INT_TO_JSVAL(nsIDOMMutationEvent::ADDITION);
      JS_SetProperty(jscontext, constructor, "ADDITION", &vp);

      vp = INT_TO_JSVAL(nsIDOMMutationEvent::REMOVAL);
      JS_SetProperty(jscontext, constructor, "REMOVAL", &vp);

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
// Method for creating a new MutationEvent JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptMutationEvent(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptMutationEvent");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMMutationEvent *aMutationEvent;

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

  if (NS_OK != NS_InitMutationEventClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIMutationEventIID, (void **)&aMutationEvent);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &MutationEventClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aMutationEvent);
  }
  else {
    NS_RELEASE(aMutationEvent);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
