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
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMRange.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);
static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);

NS_DEF_PTR(nsIDOMNode);
NS_DEF_PTR(nsIDOMDocumentFragment);
NS_DEF_PTR(nsIDOMRange);

//
// Range property ids
//
enum Range_slots {
  RANGE_STARTPARENT = -1,
  RANGE_STARTOFFSET = -2,
  RANGE_ENDPARENT = -3,
  RANGE_ENDOFFSET = -4,
  RANGE_ISCOLLAPSED = -5,
  RANGE_COMMONPARENT = -6
};

/***********************************************************************/
//
// Range Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRange *a = (nsIDOMRange*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case RANGE_STARTPARENT:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetStartParent(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case RANGE_STARTOFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetStartOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case RANGE_ENDPARENT:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetEndParent(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case RANGE_ENDOFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetEndOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case RANGE_ISCOLLAPSED:
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
      case RANGE_COMMONPARENT:
      {
        nsIDOMNode* prop;
        if (NS_OK == a->GetCommonParent(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
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
// Range Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRange *a = (nsIDOMRange*)JS_GetPrivate(cx, obj);

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
// Range finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeRange(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Range enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateRange(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Range resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveRange(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method SetStart
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStart(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetStart(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setStart requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetStartBefore
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStartBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetStartBefore(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setStartBefore requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetStartAfter
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStartAfter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetStartAfter(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setStartAfter requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetEnd
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEnd(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
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

    if (NS_OK != nativeThis->SetEnd(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setEnd requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetEndBefore
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEndBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetEndBefore(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setEndBefore requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetEndAfter
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEndAfter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SetEndAfter(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setEndAfter requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Collapse
//
PR_STATIC_CALLBACK(JSBool)
RangeCollapse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRBool b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!nsJSUtils::nsConvertJSValToBool(&b0, cx, argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Collapse(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function collapse requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SelectNode
//
PR_STATIC_CALLBACK(JSBool)
RangeSelectNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SelectNode(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function selectNode requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SelectNodeContents
//
PR_STATIC_CALLBACK(JSBool)
RangeSelectNodeContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SelectNodeContents(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function selectNodeContents requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CompareEndPoints
//
PR_STATIC_CALLBACK(JSBool)
RangeCompareEndPoints(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;
  PRUint32 b0;
  nsIDOMRangePtr b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIRangeIID,
                                           "Range",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->CompareEndPoints(b0, b1, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function compareEndPoints requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteContents
//
PR_STATIC_CALLBACK(JSBool)
RangeDeleteContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->DeleteContents()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function deleteContents requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ExtractContents
//
PR_STATIC_CALLBACK(JSBool)
RangeExtractContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMDocumentFragment* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ExtractContents(&nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function extractContents requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CloneContents
//
PR_STATIC_CALLBACK(JSBool)
RangeCloneContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMDocumentFragment* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->CloneContents(&nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function cloneContents requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method InsertNode
//
PR_STATIC_CALLBACK(JSBool)
RangeInsertNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->InsertNode(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function insertNode requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SurroundContents
//
PR_STATIC_CALLBACK(JSBool)
RangeSurroundContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMNodePtr b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kINodeIID,
                                           "Node",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->SurroundContents(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function surroundContents requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Clone
//
PR_STATIC_CALLBACK(JSBool)
RangeClone(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMRange* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Clone(&nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function clone requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
RangeToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->ToString(nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function toString requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Range
//
JSClass RangeClass = {
  "Range", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetRangeProperty,
  SetRangeProperty,
  EnumerateRange,
  ResolveRange,
  JS_ConvertStub,
  FinalizeRange
};


//
// Range class properties
//
static JSPropertySpec RangeProperties[] =
{
  {"startParent",    RANGE_STARTPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"startOffset",    RANGE_STARTOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"endParent",    RANGE_ENDPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"endOffset",    RANGE_ENDOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"isCollapsed",    RANGE_ISCOLLAPSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"commonParent",    RANGE_COMMONPARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Range class methods
//
static JSFunctionSpec RangeMethods[] = 
{
  {"setStart",          RangeSetStart,     2},
  {"setStartBefore",          RangeSetStartBefore,     1},
  {"setStartAfter",          RangeSetStartAfter,     1},
  {"setEnd",          RangeSetEnd,     2},
  {"setEndBefore",          RangeSetEndBefore,     1},
  {"setEndAfter",          RangeSetEndAfter,     1},
  {"collapse",          RangeCollapse,     1},
  {"selectNode",          RangeSelectNode,     1},
  {"selectNodeContents",          RangeSelectNodeContents,     1},
  {"compareEndPoints",          RangeCompareEndPoints,     2},
  {"deleteContents",          RangeDeleteContents,     0},
  {"extractContents",          RangeExtractContents,     0},
  {"cloneContents",          RangeCloneContents,     0},
  {"insertNode",          RangeInsertNode,     1},
  {"surroundContents",          RangeSurroundContents,     1},
  {"clone",          RangeClone,     0},
  {"toString",          RangeToString,     0},
  {0}
};


//
// Range constructor
//
PR_STATIC_CALLBACK(JSBool)
Range(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Range class initialization
//
extern "C" NS_DOM nsresult NS_InitRangeClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "Range", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &RangeClass,      // JSClass
                         Range,            // JSNative ctor
                         0,             // ctor args
                         RangeProperties,  // proto props
                         RangeMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "Range", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMRange::START_TO_START);
      JS_SetProperty(jscontext, constructor, "START_TO_START", &vp);

      vp = INT_TO_JSVAL(nsIDOMRange::START_TO_END);
      JS_SetProperty(jscontext, constructor, "START_TO_END", &vp);

      vp = INT_TO_JSVAL(nsIDOMRange::END_TO_START);
      JS_SetProperty(jscontext, constructor, "END_TO_START", &vp);

      vp = INT_TO_JSVAL(nsIDOMRange::END_TO_END);
      JS_SetProperty(jscontext, constructor, "END_TO_END", &vp);

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
// Method for creating a new Range JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptRange(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptRange");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMRange *aRange;

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

  if (NS_OK != NS_InitRangeClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIRangeIID, (void **)&aRange);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &RangeClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aRange);
  }
  else {
    NS_RELEASE(aRange);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
