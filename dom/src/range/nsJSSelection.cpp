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
#include "nsIDOMSelection.h"
#include "nsIDOMNode.h"
#include "nsIDOMSelectionListener.h"
#include "nsIDOMRange.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kISelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kISelectionListenerIID, NS_IDOMSELECTIONLISTENER_IID);
static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);

NS_DEF_PTR(nsIDOMSelection);
NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMSelectionListener);
NS_DEF_PTR(nsIDOMRange);

//
// Selection property ids
//
enum Selection_slots {
  SELECTION_ANCHORNODE = -1,
  SELECTION_ANCHOROFFSET = -2,
  SELECTION_FOCUSNODE = -3,
  SELECTION_FOCUSOFFSET = -4,
  SELECTION_ISCOLLAPSED = -5,
  SELECTION_RANGECOUNT = -6
};

/***********************************************************************/
//
// Selection Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetSelectionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSelection *a = (nsIDOMSelection*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case SELECTION_ANCHORNODE:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetAnchorNode(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SELECTION_ANCHOROFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetAnchorOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SELECTION_FOCUSNODE:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetFocusNode(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SELECTION_FOCUSOFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetFocusOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SELECTION_ISCOLLAPSED:
      {
        PRBool prop;
        if (NS_OK == a->GetIsCollapsed(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case SELECTION_RANGECOUNT:
      {
        PRInt32 prop;
        if (NS_OK == a->GetRangeCount(&prop)) {
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
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Selection Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetSelectionProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSelection *a = (nsIDOMSelection*)JS_GetPrivate(cx, obj);

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
// Selection finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeSelection(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Selection enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateSelection(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Selection resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveSelection(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method GetRangeAt
//
PR_STATIC_CALLBACK(JSBool)
SelectionGetRangeAt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMRange* nativeRet;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->GetRangeAt(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function getRangeAt requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ClearSelection
//
PR_STATIC_CALLBACK(JSBool)
SelectionClearSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ClearSelection()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function clearSelection requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Collapse
//
PR_STATIC_CALLBACK(JSBool)
SelectionCollapse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Collapse(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function collapse requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Extend
//
PR_STATIC_CALLBACK(JSBool)
SelectionExtend(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Extend(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function extend requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteFromDocument
//
PR_STATIC_CALLBACK(JSBool)
SelectionDeleteFromDocument(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->DeleteFromDocument()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function deleteFromDocument requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddRange
//
PR_STATIC_CALLBACK(JSBool)
SelectionAddRange(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMRangePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIRangeIID,
                                           "Range",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddRange(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addRange requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method StartBatchChanges
//
PR_STATIC_CALLBACK(JSBool)
SelectionStartBatchChanges(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->StartBatchChanges()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function startBatchChanges requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method EndBatchChanges
//
PR_STATIC_CALLBACK(JSBool)
SelectionEndBatchChanges(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->EndBatchChanges()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function endBatchChanges requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddSelectionListener
//
PR_STATIC_CALLBACK(JSBool)
SelectionAddSelectionListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMSelectionListenerPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kISelectionListenerIID,
                                           "SelectionListener",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddSelectionListener(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addSelectionListener requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveSelectionListener
//
PR_STATIC_CALLBACK(JSBool)
SelectionRemoveSelectionListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelection *nativeThis = (nsIDOMSelection*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMSelectionListenerPtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kISelectionListenerIID,
                                           "SelectionListener",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveSelectionListener(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeSelectionListener requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Selection
//
JSClass SelectionClass = {
  "Selection", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetSelectionProperty,
  SetSelectionProperty,
  EnumerateSelection,
  ResolveSelection,
  JS_ConvertStub,
  FinalizeSelection
};


//
// Selection class properties
//
static JSPropertySpec SelectionProperties[] =
{
  {"anchorNode",    SELECTION_ANCHORNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"anchorOffset",    SELECTION_ANCHOROFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"focusNode",    SELECTION_FOCUSNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"focusOffset",    SELECTION_FOCUSOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"isCollapsed",    SELECTION_ISCOLLAPSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"rangeCount",    SELECTION_RANGECOUNT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Selection class methods
//
static JSFunctionSpec SelectionMethods[] = 
{
  {"getRangeAt",          SelectionGetRangeAt,     1},
  {"clearSelection",          SelectionClearSelection,     0},
  {"collapse",          SelectionCollapse,     2},
  {"extend",          SelectionExtend,     2},
  {"deleteFromDocument",          SelectionDeleteFromDocument,     0},
  {"addRange",          SelectionAddRange,     1},
  {"startBatchChanges",          SelectionStartBatchChanges,     0},
  {"endBatchChanges",          SelectionEndBatchChanges,     0},
  {"addSelectionListener",          SelectionAddSelectionListener,     1},
  {"removeSelectionListener",          SelectionRemoveSelectionListener,     1},
  {0}
};


//
// Selection constructor
//
PR_STATIC_CALLBACK(JSBool)
Selection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Selection class initialization
//
extern "C" NS_DOM nsresult NS_InitSelectionClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Selection", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &SelectionClass,      // JSClass
                         Selection,            // JSNative ctor
                         0,             // ctor args
                         SelectionProperties,  // proto props
                         SelectionMethods,     // proto funcs
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
// Method for creating a new Selection JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptSelection(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptSelection");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMSelection *aSelection;

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

  if (NS_OK != NS_InitSelectionClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kISelectionIID, (void **)&aSelection);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &SelectionClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSelection);
  }
  else {
    NS_RELEASE(aSelection);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
