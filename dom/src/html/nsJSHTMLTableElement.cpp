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
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableCaptionElement.h"
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableElementIID, NS_IDOMHTMLTABLEELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableCaptionElementIID, NS_IDOMHTMLTABLECAPTIONELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableSectionElementIID, NS_IDOMHTMLTABLESECTIONELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

//
// HTMLTableElement property ids
//
enum HTMLTableElement_slots {
  HTMLTABLEELEMENT_CAPTION = -1,
  HTMLTABLEELEMENT_THEAD = -2,
  HTMLTABLEELEMENT_TFOOT = -3,
  HTMLTABLEELEMENT_ROWS = -4,
  HTMLTABLEELEMENT_TBODIES = -5,
  HTMLTABLEELEMENT_ALIGN = -6,
  HTMLTABLEELEMENT_BGCOLOR = -7,
  HTMLTABLEELEMENT_BORDER = -8,
  HTMLTABLEELEMENT_CELLPADDING = -9,
  HTMLTABLEELEMENT_CELLSPACING = -10,
  HTMLTABLEELEMENT_FRAME = -11,
  HTMLTABLEELEMENT_RULES = -12,
  HTMLTABLEELEMENT_SUMMARY = -13,
  HTMLTABLEELEMENT_WIDTH = -14
};

/***********************************************************************/
//
// HTMLTableElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTableElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableElement *a = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLEELEMENT_CAPTION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CAPTION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableCaptionElement* prop;
          rv = a->GetCaption(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_THEAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_THEAD, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableSectionElement* prop;
          rv = a->GetTHead(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_TFOOT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_TFOOT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableSectionElement* prop;
          rv = a->GetTFoot(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_ROWS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_ROWS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetRows(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_TBODIES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_TBODIES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetTBodies(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_BGCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBgColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_BORDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorder(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_CELLPADDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CELLPADDING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCellPadding(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_CELLSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CELLSPACING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCellSpacing(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_FRAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_FRAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFrame(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_RULES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_RULES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetRules(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_SUMMARY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_SUMMARY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSummary(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLEELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
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
// HTMLTableElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTableElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableElement *a = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLEELEMENT_CAPTION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CAPTION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableCaptionElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIHTMLTableCaptionElementIID, NS_ConvertASCIItoUCS2("HTMLTableCaptionElement"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetCaption(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case HTMLTABLEELEMENT_THEAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_THEAD, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableSectionElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIHTMLTableSectionElementIID, NS_ConvertASCIItoUCS2("HTMLTableSectionElement"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetTHead(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case HTMLTABLEELEMENT_TFOOT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_TFOOT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLTableSectionElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIHTMLTableSectionElementIID, NS_ConvertASCIItoUCS2("HTMLTableSectionElement"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetTFoot(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case HTMLTABLEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_BGCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBgColor(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_BORDER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorder(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_CELLPADDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CELLPADDING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCellPadding(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_CELLSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CELLSPACING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCellSpacing(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_FRAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_FRAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFrame(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_RULES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_RULES, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetRules(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_SUMMARY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_SUMMARY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSummary(prop);
          
        }
        break;
      }
      case HTMLTABLEELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_WIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidth(prop);
          
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
// HTMLTableElement class properties
//
static JSPropertySpec HTMLTableElementProperties[] =
{
  {"caption",    HTMLTABLEELEMENT_CAPTION,    JSPROP_ENUMERATE},
  {"tHead",    HTMLTABLEELEMENT_THEAD,    JSPROP_ENUMERATE},
  {"tFoot",    HTMLTABLEELEMENT_TFOOT,    JSPROP_ENUMERATE},
  {"rows",    HTMLTABLEELEMENT_ROWS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"tBodies",    HTMLTABLEELEMENT_TBODIES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"align",    HTMLTABLEELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"bgColor",    HTMLTABLEELEMENT_BGCOLOR,    JSPROP_ENUMERATE},
  {"border",    HTMLTABLEELEMENT_BORDER,    JSPROP_ENUMERATE},
  {"cellPadding",    HTMLTABLEELEMENT_CELLPADDING,    JSPROP_ENUMERATE},
  {"cellSpacing",    HTMLTABLEELEMENT_CELLSPACING,    JSPROP_ENUMERATE},
  {"frame",    HTMLTABLEELEMENT_FRAME,    JSPROP_ENUMERATE},
  {"rules",    HTMLTABLEELEMENT_RULES,    JSPROP_ENUMERATE},
  {"summary",    HTMLTABLEELEMENT_SUMMARY,    JSPROP_ENUMERATE},
  {"width",    HTMLTABLEELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLTableElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTableElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTableElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTableElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLTableElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTableElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


//
// Native method CreateTHead
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementCreateTHead(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMHTMLElement* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CREATETHEAD, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->CreateTHead(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method DeleteTHead
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementDeleteTHead(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_DELETETHEAD, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DeleteTHead();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method CreateTFoot
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementCreateTFoot(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMHTMLElement* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CREATETFOOT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->CreateTFoot(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method DeleteTFoot
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementDeleteTFoot(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_DELETETFOOT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DeleteTFoot();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method CreateCaption
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementCreateCaption(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMHTMLElement* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_CREATECAPTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->CreateCaption(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method DeleteCaption
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementDeleteCaption(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_DELETECAPTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DeleteCaption();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method InsertRow
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementInsertRow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMHTMLElement* nativeRet;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_INSERTROW, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->InsertRow(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method DeleteRow
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElementDeleteRow(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableElement *nativeThis = (nsIDOMHTMLTableElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLEELEMENT_DELETEROW, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->DeleteRow(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLTableElement
//
JSClass HTMLTableElementClass = {
  "HTMLTableElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTableElementProperty,
  SetHTMLTableElementProperty,
  EnumerateHTMLTableElement,
  ResolveHTMLTableElement,
  JS_ConvertStub,
  FinalizeHTMLTableElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLTableElement class methods
//
static JSFunctionSpec HTMLTableElementMethods[] = 
{
  {"createTHead",          HTMLTableElementCreateTHead,     0},
  {"deleteTHead",          HTMLTableElementDeleteTHead,     0},
  {"createTFoot",          HTMLTableElementCreateTFoot,     0},
  {"deleteTFoot",          HTMLTableElementDeleteTFoot,     0},
  {"createCaption",          HTMLTableElementCreateCaption,     0},
  {"deleteCaption",          HTMLTableElementDeleteCaption,     0},
  {"insertRow",          HTMLTableElementInsertRow,     1},
  {"deleteRow",          HTMLTableElementDeleteRow,     1},
  {0}
};


//
// HTMLTableElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTableElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTableElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTableElement", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitHTMLElementClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLTableElementClass,      // JSClass
                         HTMLTableElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTableElementProperties,  // proto props
                         HTMLTableElementMethods,     // proto funcs
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
// Method for creating a new HTMLTableElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTableElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTableElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTableElement *aHTMLTableElement;

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

  if (NS_OK != NS_InitHTMLTableElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTableElementIID, (void **)&aHTMLTableElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTableElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTableElement);
  }
  else {
    NS_RELEASE(aHTMLTableElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
