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
#include "nsIDOMXULElement.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIXULElementIID, NS_IDOMXULELEMENT_IID);
static NS_DEFINE_IID(kIXULTreeElementIID, NS_IDOMXULTREEELEMENT_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

//
// XULTreeElement property ids
//
enum XULTreeElement_slots {
  XULTREEELEMENT_SELECTEDITEMS = -1,
  XULTREEELEMENT_CURRENTITEM = -2,
  XULTREEELEMENT_SUPPRESSONSELECT = -3
};

/***********************************************************************/
//
// XULTreeElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetXULTreeElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULTreeElement *a = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULTREEELEMENT_SELECTEDITEMS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SELECTEDITEMS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNodeList* prop;
          rv = a->GetSelectedItems(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULTREEELEMENT_CURRENTITEM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_CURRENTITEM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMXULElement* prop;
          rv = a->GetCurrentItem(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case XULTREEELEMENT_SUPPRESSONSELECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SUPPRESSONSELECT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetSuppressOnSelect(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
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
// XULTreeElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetXULTreeElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMXULTreeElement *a = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case XULTREEELEMENT_CURRENTITEM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_CURRENTITEM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMXULElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIXULElementIID, NS_ConvertASCIItoUCS2("XULElement"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetCurrentItem(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case XULTREEELEMENT_SUPPRESSONSELECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SUPPRESSONSELECT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetSuppressOnSelect(prop);
          
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
// XULTreeElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeXULTreeElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// XULTreeElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateXULTreeElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// XULTreeElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveXULTreeElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method SelectItem
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementSelectItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SELECTITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SelectItem(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method TimedSelect
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementTimedSelect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_TIMEDSELECT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->TimedSelect(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ClearItemSelection
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementClearItemSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_CLEARITEMSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->ClearItemSelection();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method AddItemToSelection
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementAddItemToSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_ADDITEMTOSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->AddItemToSelection(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RemoveItemFromSelection
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementRemoveItemFromSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_REMOVEITEMFROMSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RemoveItemFromSelection(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ToggleItemSelection
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementToggleItemSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_TOGGLEITEMSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->ToggleItemSelection(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SelectItemRange
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementSelectItemRange(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMXULElement> b0;
  nsCOMPtr<nsIDOMXULElement> b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SELECTITEMRANGE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b1),
                                           kIXULElementIID,
                                           NS_ConvertASCIItoUCS2("XULElement"),
                                           cx,
                                           argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->SelectItemRange(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SelectAll
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementSelectAll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_SELECTALL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SelectAll();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method InvertSelection
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElementInvertSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMXULTreeElement *nativeThis = (nsIDOMXULTreeElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_XULTREEELEMENT_INVERTSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->InvertSelection();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for XULTreeElement
//
JSClass XULTreeElementClass = {
  "XULTreeElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetXULTreeElementProperty,
  SetXULTreeElementProperty,
  EnumerateXULTreeElement,
  ResolveXULTreeElement,
  JS_ConvertStub,
  FinalizeXULTreeElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// XULTreeElement class properties
//
static JSPropertySpec XULTreeElementProperties[] =
{
  {"selectedItems",    XULTREEELEMENT_SELECTEDITEMS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"currentItem",    XULTREEELEMENT_CURRENTITEM,    JSPROP_ENUMERATE},
  {"suppressOnSelect",    XULTREEELEMENT_SUPPRESSONSELECT,    JSPROP_ENUMERATE},
  {0}
};


//
// XULTreeElement class methods
//
static JSFunctionSpec XULTreeElementMethods[] = 
{
  {"selectItem",          XULTreeElementSelectItem,     1},
  {"timedSelect",          XULTreeElementTimedSelect,     2},
  {"clearItemSelection",          XULTreeElementClearItemSelection,     0},
  {"addItemToSelection",          XULTreeElementAddItemToSelection,     1},
  {"removeItemFromSelection",          XULTreeElementRemoveItemFromSelection,     1},
  {"toggleItemSelection",          XULTreeElementToggleItemSelection,     1},
  {"selectItemRange",          XULTreeElementSelectItemRange,     2},
  {"selectAll",          XULTreeElementSelectAll,     0},
  {"invertSelection",          XULTreeElementInvertSelection,     0},
  {0}
};


//
// XULTreeElement constructor
//
PR_STATIC_CALLBACK(JSBool)
XULTreeElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// XULTreeElement class initialization
//
extern "C" NS_DOM nsresult NS_InitXULTreeElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XULTreeElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitXULElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XULTreeElementClass,      // JSClass
                         XULTreeElement,            // JSNative ctor
                         0,             // ctor args
                         XULTreeElementProperties,  // proto props
                         XULTreeElementMethods,     // proto funcs
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
// Method for creating a new XULTreeElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptXULTreeElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptXULTreeElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMXULTreeElement *aXULTreeElement;

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

  if (NS_OK != NS_InitXULTreeElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIXULTreeElementIID, (void **)&aXULTreeElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &XULTreeElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aXULTreeElement);
  }
  else {
    NS_RELEASE(aXULTreeElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
