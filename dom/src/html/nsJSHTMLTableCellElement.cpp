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
#include "nsIDOMHTMLTableCellElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IDOMHTMLTABLECELLELEMENT_IID);

//
// HTMLTableCellElement property ids
//
enum HTMLTableCellElement_slots {
  HTMLTABLECELLELEMENT_CELLINDEX = -1,
  HTMLTABLECELLELEMENT_ABBR = -2,
  HTMLTABLECELLELEMENT_ALIGN = -3,
  HTMLTABLECELLELEMENT_AXIS = -4,
  HTMLTABLECELLELEMENT_BGCOLOR = -5,
  HTMLTABLECELLELEMENT_CH = -6,
  HTMLTABLECELLELEMENT_CHOFF = -7,
  HTMLTABLECELLELEMENT_COLSPAN = -8,
  HTMLTABLECELLELEMENT_HEADERS = -9,
  HTMLTABLECELLELEMENT_HEIGHT = -10,
  HTMLTABLECELLELEMENT_NOWRAP = -11,
  HTMLTABLECELLELEMENT_ROWSPAN = -12,
  HTMLTABLECELLELEMENT_SCOPE = -13,
  HTMLTABLECELLELEMENT_VALIGN = -14,
  HTMLTABLECELLELEMENT_WIDTH = -15
};

/***********************************************************************/
//
// HTMLTableCellElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTableCellElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableCellElement *a = (nsIDOMHTMLTableCellElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLECELLELEMENT_CELLINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CELLINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetCellIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ABBR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ABBR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAbbr(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_AXIS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_AXIS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAxis(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_BGCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBgColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_CH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCh(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_CHOFF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CHOFF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetChOff(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_COLSPAN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_COLSPAN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetColSpan(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_HEADERS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_HEADERS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeaders(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_NOWRAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_NOWRAP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetNoWrap(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ROWSPAN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ROWSPAN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetRowSpan(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_SCOPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_SCOPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetScope(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_VALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_VALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLTABLECELLELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_WIDTH, PR_FALSE);
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
// HTMLTableCellElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTableCellElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableCellElement *a = (nsIDOMHTMLTableCellElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLECELLELEMENT_CELLINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CELLINDEX, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          rv = a->SetCellIndex(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ABBR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ABBR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAbbr(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_AXIS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_AXIS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAxis(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_BGCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBgColor(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_CH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCh(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_CHOFF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_CHOFF, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetChOff(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_COLSPAN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_COLSPAN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          rv = a->SetColSpan(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_HEADERS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_HEADERS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeaders(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_NOWRAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_NOWRAP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetNoWrap(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_ROWSPAN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_ROWSPAN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          rv = a->SetRowSpan(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_SCOPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_SCOPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetScope(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_VALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_VALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVAlign(prop);
          
        }
        break;
      }
      case HTMLTABLECELLELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLTABLECELLELEMENT_WIDTH, PR_TRUE);
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
// HTMLTableCellElement class properties
//
static JSPropertySpec HTMLTableCellElementProperties[] =
{
  {"cellIndex",    HTMLTABLECELLELEMENT_CELLINDEX,    JSPROP_ENUMERATE},
  {"abbr",    HTMLTABLECELLELEMENT_ABBR,    JSPROP_ENUMERATE},
  {"align",    HTMLTABLECELLELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"axis",    HTMLTABLECELLELEMENT_AXIS,    JSPROP_ENUMERATE},
  {"bgColor",    HTMLTABLECELLELEMENT_BGCOLOR,    JSPROP_ENUMERATE},
  {"ch",    HTMLTABLECELLELEMENT_CH,    JSPROP_ENUMERATE},
  {"chOff",    HTMLTABLECELLELEMENT_CHOFF,    JSPROP_ENUMERATE},
  {"colSpan",    HTMLTABLECELLELEMENT_COLSPAN,    JSPROP_ENUMERATE},
  {"headers",    HTMLTABLECELLELEMENT_HEADERS,    JSPROP_ENUMERATE},
  {"height",    HTMLTABLECELLELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"noWrap",    HTMLTABLECELLELEMENT_NOWRAP,    JSPROP_ENUMERATE},
  {"rowSpan",    HTMLTABLECELLELEMENT_ROWSPAN,    JSPROP_ENUMERATE},
  {"scope",    HTMLTABLECELLELEMENT_SCOPE,    JSPROP_ENUMERATE},
  {"vAlign",    HTMLTABLECELLELEMENT_VALIGN,    JSPROP_ENUMERATE},
  {"width",    HTMLTABLECELLELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLTableCellElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTableCellElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTableCellElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTableCellElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLTableCellElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTableCellElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLTableCellElement
//
JSClass HTMLTableCellElementClass = {
  "HTMLTableCellElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTableCellElementProperty,
  SetHTMLTableCellElementProperty,
  EnumerateHTMLTableCellElement,
  ResolveHTMLTableCellElement,
  JS_ConvertStub,
  FinalizeHTMLTableCellElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLTableCellElement class methods
//
static JSFunctionSpec HTMLTableCellElementMethods[] = 
{
  {0}
};


//
// HTMLTableCellElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableCellElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTableCellElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTableCellElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTableCellElement", &vp)) ||
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
                         &HTMLTableCellElementClass,      // JSClass
                         HTMLTableCellElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTableCellElementProperties,  // proto props
                         HTMLTableCellElementMethods,     // proto funcs
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
// Method for creating a new HTMLTableCellElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTableCellElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTableCellElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTableCellElement *aHTMLTableCellElement;

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

  if (NS_OK != NS_InitHTMLTableCellElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTableCellElementIID, (void **)&aHTMLTableCellElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTableCellElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTableCellElement);
  }
  else {
    NS_RELEASE(aHTMLTableCellElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
