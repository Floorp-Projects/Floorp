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
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLObjectElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLObjectElementIID, NS_IDOMHTMLOBJECTELEMENT_IID);

//
// HTMLObjectElement property ids
//
enum HTMLObjectElement_slots {
  HTMLOBJECTELEMENT_FORM = -1,
  HTMLOBJECTELEMENT_CODE = -2,
  HTMLOBJECTELEMENT_ALIGN = -3,
  HTMLOBJECTELEMENT_ARCHIVE = -4,
  HTMLOBJECTELEMENT_BORDER = -5,
  HTMLOBJECTELEMENT_CODEBASE = -6,
  HTMLOBJECTELEMENT_CODETYPE = -7,
  HTMLOBJECTELEMENT_DATA = -8,
  HTMLOBJECTELEMENT_DECLARE = -9,
  HTMLOBJECTELEMENT_HEIGHT = -10,
  HTMLOBJECTELEMENT_HSPACE = -11,
  HTMLOBJECTELEMENT_NAME = -12,
  HTMLOBJECTELEMENT_STANDBY = -13,
  HTMLOBJECTELEMENT_TABINDEX = -14,
  HTMLOBJECTELEMENT_TYPE = -15,
  HTMLOBJECTELEMENT_USEMAP = -16,
  HTMLOBJECTELEMENT_VSPACE = -17,
  HTMLOBJECTELEMENT_WIDTH = -18,
  HTMLOBJECTELEMENT_CONTENTDOCUMENT = -19
};

/***********************************************************************/
//
// HTMLObjectElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLObjectElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLObjectElement *a = (nsIDOMHTMLObjectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLOBJECTELEMENT_FORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_FORM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLFormElement* prop;
          rv = a->GetForm(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCode(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_ARCHIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_ARCHIVE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetArchive(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_BORDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorder(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODEBASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODEBASE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCodeBase(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODETYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODETYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCodeType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_DATA, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetData(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_DECLARE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_DECLARE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDeclare(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_HSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHspace(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_STANDBY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_STANDBY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetStandby(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_TABINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetTabIndex(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_USEMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_USEMAP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetUseMap(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_VSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVspace(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLOBJECTELEMENT_CONTENTDOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CONTENTDOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          rv = a->GetContentDocument(&prop);
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
// HTMLObjectElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLObjectElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLObjectElement *a = (nsIDOMHTMLObjectElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLOBJECTELEMENT_CODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCode(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_ARCHIVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_ARCHIVE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetArchive(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_BORDER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorder(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODEBASE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODEBASE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCodeBase(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODETYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CODETYPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCodeType(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_DATA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_DATA, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetData(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_DECLARE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_DECLARE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetDeclare(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_HSPACE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHspace(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_STANDBY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_STANDBY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetStandby(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_TABINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_TABINDEX, PR_TRUE);
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
      
          rv = a->SetTabIndex(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_TYPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetType(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_USEMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_USEMAP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetUseMap(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_VSPACE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVspace(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_WIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidth(prop);
          
        }
        break;
      }
      case HTMLOBJECTELEMENT_CONTENTDOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLOBJECTELEMENT_CONTENTDOCUMENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIDocumentIID, NS_ConvertASCIItoUCS2("Document"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetContentDocument(prop);
          NS_IF_RELEASE(prop);
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
// HTMLObjectElement class properties
//
static JSPropertySpec HTMLObjectElementProperties[] =
{
  {"form",    HTMLOBJECTELEMENT_FORM,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"code",    HTMLOBJECTELEMENT_CODE,    JSPROP_ENUMERATE},
  {"align",    HTMLOBJECTELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"archive",    HTMLOBJECTELEMENT_ARCHIVE,    JSPROP_ENUMERATE},
  {"border",    HTMLOBJECTELEMENT_BORDER,    JSPROP_ENUMERATE},
  {"codeBase",    HTMLOBJECTELEMENT_CODEBASE,    JSPROP_ENUMERATE},
  {"codeType",    HTMLOBJECTELEMENT_CODETYPE,    JSPROP_ENUMERATE},
  {"data",    HTMLOBJECTELEMENT_DATA,    JSPROP_ENUMERATE},
  {"declare",    HTMLOBJECTELEMENT_DECLARE,    JSPROP_ENUMERATE},
  {"height",    HTMLOBJECTELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"hspace",    HTMLOBJECTELEMENT_HSPACE,    JSPROP_ENUMERATE},
  {"name",    HTMLOBJECTELEMENT_NAME,    JSPROP_ENUMERATE},
  {"standby",    HTMLOBJECTELEMENT_STANDBY,    JSPROP_ENUMERATE},
  {"tabIndex",    HTMLOBJECTELEMENT_TABINDEX,    JSPROP_ENUMERATE},
  {"type",    HTMLOBJECTELEMENT_TYPE,    JSPROP_ENUMERATE},
  {"useMap",    HTMLOBJECTELEMENT_USEMAP,    JSPROP_ENUMERATE},
  {"vspace",    HTMLOBJECTELEMENT_VSPACE,    JSPROP_ENUMERATE},
  {"width",    HTMLOBJECTELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {"contentDocument",    HTMLOBJECTELEMENT_CONTENTDOCUMENT,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLObjectElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLObjectElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLObjectElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLObjectElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLObjectElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLObjectElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLObjectElement
//
JSClass HTMLObjectElementClass = {
  "HTMLObjectElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLObjectElementProperty,
  SetHTMLObjectElementProperty,
  EnumerateHTMLObjectElement,
  ResolveHTMLObjectElement,
  JS_ConvertStub,
  FinalizeHTMLObjectElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLObjectElement class methods
//
static JSFunctionSpec HTMLObjectElementMethods[] = 
{
  {0}
};


//
// HTMLObjectElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLObjectElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLObjectElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLObjectElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLObjectElement", &vp)) ||
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
                         &HTMLObjectElementClass,      // JSClass
                         HTMLObjectElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLObjectElementProperties,  // proto props
                         HTMLObjectElementMethods,     // proto funcs
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
// Method for creating a new HTMLObjectElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLObjectElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLObjectElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLObjectElement *aHTMLObjectElement;

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

  if (NS_OK != NS_InitHTMLObjectElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLObjectElementIID, (void **)&aHTMLObjectElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLObjectElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLObjectElement);
  }
  else {
    NS_RELEASE(aHTMLObjectElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
