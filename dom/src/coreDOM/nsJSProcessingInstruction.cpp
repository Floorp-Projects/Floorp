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
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMLinkStyle.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIProcessingInstructionIID, NS_IDOMPROCESSINGINSTRUCTION_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kILinkStyleIID, NS_IDOMLINKSTYLE_IID);

//
// ProcessingInstruction property ids
//
enum ProcessingInstruction_slots {
  PROCESSINGINSTRUCTION_TARGET = -1,
  PROCESSINGINSTRUCTION_DATA = -2,
  LINKSTYLE_SHEET = -3
};

/***********************************************************************/
//
// ProcessingInstruction Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetProcessingInstructionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMProcessingInstruction *a = (nsIDOMProcessingInstruction*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case PROCESSINGINSTRUCTION_TARGET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PROCESSINGINSTRUCTION_TARGET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTarget(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case PROCESSINGINSTRUCTION_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PROCESSINGINSTRUCTION_DATA, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetData(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case LINKSTYLE_SHEET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_LINKSTYLE_SHEET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMStyleSheet* prop;
          nsIDOMLinkStyle* b;
          if (NS_OK == a->QueryInterface(kILinkStyleIID, (void **)&b)) {
            rv = b->GetSheet(&prop);
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
// ProcessingInstruction Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetProcessingInstructionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMProcessingInstruction *a = (nsIDOMProcessingInstruction*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case PROCESSINGINSTRUCTION_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_PROCESSINGINSTRUCTION_DATA, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetData(prop);
          
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
// ProcessingInstruction class properties
//
static JSPropertySpec ProcessingInstructionProperties[] =
{
  {"target",    PROCESSINGINSTRUCTION_TARGET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"data",    PROCESSINGINSTRUCTION_DATA,    JSPROP_ENUMERATE},
  {"sheet",    LINKSTYLE_SHEET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// ProcessingInstruction finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeProcessingInstruction(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// ProcessingInstruction enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateProcessingInstruction(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// ProcessingInstruction resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveProcessingInstruction(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for ProcessingInstruction
//
JSClass ProcessingInstructionClass = {
  "ProcessingInstruction", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetProcessingInstructionProperty,
  SetProcessingInstructionProperty,
  EnumerateProcessingInstruction,
  ResolveProcessingInstruction,
  JS_ConvertStub,
  FinalizeProcessingInstruction,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// ProcessingInstruction class methods
//
static JSFunctionSpec ProcessingInstructionMethods[] = 
{
  {0}
};


//
// ProcessingInstruction constructor
//
PR_STATIC_CALLBACK(JSBool)
ProcessingInstruction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// ProcessingInstruction class initialization
//
extern "C" NS_DOM nsresult NS_InitProcessingInstructionClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "ProcessingInstruction", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitNodeClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &ProcessingInstructionClass,      // JSClass
                         ProcessingInstruction,            // JSNative ctor
                         0,             // ctor args
                         ProcessingInstructionProperties,  // proto props
                         ProcessingInstructionMethods,     // proto funcs
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
// Method for creating a new ProcessingInstruction JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptProcessingInstruction(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptProcessingInstruction");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMProcessingInstruction *aProcessingInstruction;

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

  if (NS_OK != NS_InitProcessingInstructionClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIProcessingInstructionIID, (void **)&aProcessingInstruction);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &ProcessingInstructionClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aProcessingInstruction);
  }
  else {
    NS_RELEASE(aProcessingInstruction);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
