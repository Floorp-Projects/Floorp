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
#include "nsIDOMNSRange.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMRange.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kINSRangeIID, NS_IDOMNSRANGE_IID);
static NS_DEFINE_IID(kIDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);
static NS_DEFINE_IID(kIRangeIID, NS_IDOMRANGE_IID);

//
// Range property ids
//
enum Range_slots {
  RANGE_STARTCONTAINER = -1,
  RANGE_STARTOFFSET = -2,
  RANGE_ENDCONTAINER = -3,
  RANGE_ENDOFFSET = -4,
  RANGE_COLLAPSED = -5,
  RANGE_COMMONANCESTORCONTAINER = -6
};

/***********************************************************************/
//
// Range Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRange *a = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case RANGE_STARTCONTAINER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_STARTCONTAINER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetStartContainer(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RANGE_STARTOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_STARTOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetStartOffset(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case RANGE_ENDCONTAINER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_ENDCONTAINER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetEndContainer(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case RANGE_ENDOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_ENDOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetEndOffset(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case RANGE_COLLAPSED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_COLLAPSED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetCollapsed(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case RANGE_COMMONANCESTORCONTAINER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_COMMONANCESTORCONTAINER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetCommonAncestorContainer(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
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
// Range Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetRangeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMRange *a = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// Range class properties
//
static JSPropertySpec RangeProperties[] =
{
  {"startContainer",    RANGE_STARTCONTAINER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"startOffset",    RANGE_STARTOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"endContainer",    RANGE_ENDCONTAINER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"endOffset",    RANGE_ENDOFFSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"collapsed",    RANGE_COLLAPSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"commonAncestorContainer",    RANGE_COMMONANCESTORCONTAINER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


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
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// Range resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveRange(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method SetStart
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStart(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETSTART, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->SetStart(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetStartBefore
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStartBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETSTARTBEFORE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SetStartBefore(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetStartAfter
//
PR_STATIC_CALLBACK(JSBool)
RangeSetStartAfter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETSTARTAFTER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SetStartAfter(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetEnd
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEnd(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETEND, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->SetEnd(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetEndBefore
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEndBefore(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETENDBEFORE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SetEndBefore(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetEndAfter
//
PR_STATIC_CALLBACK(JSBool)
RangeSetEndAfter(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SETENDAFTER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SetEndAfter(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Collapse
//
PR_STATIC_CALLBACK(JSBool)
RangeCollapse(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRBool b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_COLLAPSE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b0, cx, argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->Collapse(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SelectNode
//
PR_STATIC_CALLBACK(JSBool)
RangeSelectNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SELECTNODE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SelectNode(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SelectNodeContents
//
PR_STATIC_CALLBACK(JSBool)
RangeSelectNodeContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SELECTNODECONTENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SelectNodeContents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method CompareBoundaryPoints
//
PR_STATIC_CALLBACK(JSBool)
RangeCompareBoundaryPoints(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 nativeRet;
  PRUint32 b0;
  nsCOMPtr<nsIDOMRange> b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_COMPAREBOUNDARYPOINTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b1),
                                           kIRangeIID,
                                           NS_ConvertASCIItoUCS2("Range"),
                                           cx,
                                           argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->CompareBoundaryPoints(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method DeleteContents
//
PR_STATIC_CALLBACK(JSBool)
RangeDeleteContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_DELETECONTENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DeleteContents();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ExtractContents
//
PR_STATIC_CALLBACK(JSBool)
RangeExtractContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMDocumentFragment* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_EXTRACTCONTENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->ExtractContents(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method CloneContents
//
PR_STATIC_CALLBACK(JSBool)
RangeCloneContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMDocumentFragment* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_CLONECONTENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->CloneContents(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method InsertNode
//
PR_STATIC_CALLBACK(JSBool)
RangeInsertNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_INSERTNODE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->InsertNode(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SurroundContents
//
PR_STATIC_CALLBACK(JSBool)
RangeSurroundContents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_SURROUNDCONTENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SurroundContents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method CloneRange
//
PR_STATIC_CALLBACK(JSBool)
RangeCloneRange(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMRange* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_CLONERANGE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->CloneRange(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method Detach
//
PR_STATIC_CALLBACK(JSBool)
RangeDetach(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_DETACH, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Detach();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ToString
//
PR_STATIC_CALLBACK(JSBool)
RangeToString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *nativeThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_RANGE_TOSTRING, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->ToString(nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method CreateContextualFragment
//
PR_STATIC_CALLBACK(JSBool)
NSRangeCreateContextualFragment(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsIDOMDocumentFragment* nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_CREATECONTEXTUALFRAGMENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->CreateContextualFragment(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method IsValidFragment
//
PR_STATIC_CALLBACK(JSBool)
NSRangeIsValidFragment(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRBool nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_ISVALIDFRAGMENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->IsValidFragment(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method IsPointInRange
//
PR_STATIC_CALLBACK(JSBool)
NSRangeIsPointInRange(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRBool nativeRet;
  nsCOMPtr<nsIDOMNode> b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_ISPOINTINRANGE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->IsPointInRange(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method ComparePoint
//
PR_STATIC_CALLBACK(JSBool)
NSRangeComparePoint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt16 nativeRet;
  nsCOMPtr<nsIDOMNode> b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_COMPAREPOINT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ComparePoint(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method IntersectsNode
//
PR_STATIC_CALLBACK(JSBool)
NSRangeIntersectsNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRBool nativeRet;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_INTERSECTSNODE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->IntersectsNode(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method CompareNode
//
PR_STATIC_CALLBACK(JSBool)
NSRangeCompareNode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMRange *privateThis = (nsIDOMRange*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSRange> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSRangeIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRUint16 nativeRet;
  nsCOMPtr<nsIDOMNode> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSRANGE_COMPARENODE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kINodeIID,
                                           NS_ConvertASCIItoUCS2("Node"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->CompareNode(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Range
//
JSClass RangeClass = {
  "Range", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetRangeProperty,
  SetRangeProperty,
  EnumerateRange,
  ResolveRange,
  JS_ConvertStub,
  FinalizeRange,
  nsnull,
  nsJSUtils::nsCheckAccess
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
  {"compareBoundaryPoints",          RangeCompareBoundaryPoints,     2},
  {"deleteContents",          RangeDeleteContents,     0},
  {"extractContents",          RangeExtractContents,     0},
  {"cloneContents",          RangeCloneContents,     0},
  {"insertNode",          RangeInsertNode,     1},
  {"surroundContents",          RangeSurroundContents,     1},
  {"cloneRange",          RangeCloneRange,     0},
  {"detach",          RangeDetach,     0},
  {"toString",          RangeToString,     0},
  {"createContextualFragment",          NSRangeCreateContextualFragment,     1},
  {"isValidFragment",          NSRangeIsValidFragment,     1},
  {"isPointInRange",          NSRangeIsPointInRange,     2},
  {"comparePoint",          NSRangeComparePoint,     2},
  {"intersectsNode",          NSRangeIntersectsNode,     1},
  {"compareNode",          NSRangeCompareNode,     1},
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
