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
#include "nsIDOMImage.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIImageIID, NS_IDOMIMAGE_IID);
static NS_DEFINE_IID(kIHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);

//
// HTMLImageElement property ids
//
enum HTMLImageElement_slots {
  HTMLIMAGEELEMENT_LOWSRC = -1,
  HTMLIMAGEELEMENT_NAME = -2,
  HTMLIMAGEELEMENT_ALIGN = -3,
  HTMLIMAGEELEMENT_ALT = -4,
  HTMLIMAGEELEMENT_ISMAP = -5,
  HTMLIMAGEELEMENT_LONGDESC = -6,
  HTMLIMAGEELEMENT_USEMAP = -7,
  IMAGE_LOWSRC = -8,
  IMAGE_COMPLETE = -9,
  IMAGE_BORDER = -10,
  IMAGE_HEIGHT = -11,
  IMAGE_HSPACE = -12,
  IMAGE_VSPACE = -13,
  IMAGE_WIDTH = -14,
  IMAGE_NATURALHEIGHT = -15,
  IMAGE_NATURALWIDTH = -16
};

/***********************************************************************/
//
// HTMLImageElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLImageElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLImageElement *a = (nsIDOMHTMLImageElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIMAGEELEMENT_LOWSRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_LOWSRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLowSrc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ALT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlt(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_ISMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ISMAP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetIsMap(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_LONGDESC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_LONGDESC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLongDesc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLIMAGEELEMENT_USEMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_USEMAP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetUseMap(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case IMAGE_LOWSRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_LOWSRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetLowsrc(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case IMAGE_COMPLETE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_COMPLETE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetComplete(&prop);
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
      case IMAGE_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_BORDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetBorder(&prop);
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
      case IMAGE_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetHeight(&prop);
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
      case IMAGE_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_HSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetHspace(&prop);
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
      case IMAGE_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_VSPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetVspace(&prop);
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
      case IMAGE_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetWidth(&prop);
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
      case IMAGE_NATURALHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_NATURALHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetNaturalHeight(&prop);
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
      case IMAGE_NATURALWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_NATURALWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMImage* b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            rv = b->GetNaturalWidth(&prop);
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
// HTMLImageElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLImageElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLImageElement *a = (nsIDOMHTMLImageElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLIMAGEELEMENT_LOWSRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_LOWSRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLowSrc(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_ALT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ALT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlt(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_ISMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_ISMAP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetIsMap(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_LONGDESC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_LONGDESC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLongDesc(prop);
          
        }
        break;
      }
      case HTMLIMAGEELEMENT_USEMAP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLIMAGEELEMENT_USEMAP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetUseMap(prop);
          
        }
        break;
      }
      case IMAGE_LOWSRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_LOWSRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetLowsrc(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case IMAGE_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_BORDER, PR_TRUE);
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
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetBorder(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case IMAGE_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_HEIGHT, PR_TRUE);
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
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetHeight(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case IMAGE_HSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_HSPACE, PR_TRUE);
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
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetHspace(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case IMAGE_VSPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_VSPACE, PR_TRUE);
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
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetVspace(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case IMAGE_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_IMAGE_WIDTH, PR_TRUE);
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
      
          nsIDOMImage *b;
          if (NS_OK == a->QueryInterface(kIImageIID, (void **)&b)) {
            b->SetWidth(prop);
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
// HTMLImageElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLImageElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLImageElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLImageElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLImageElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLImageElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for HTMLImageElement
//
JSClass HTMLImageElementClass = {
  "HTMLImageElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLImageElementProperty,
  SetHTMLImageElementProperty,
  EnumerateHTMLImageElement,
  ResolveHTMLImageElement,
  JS_ConvertStub,
  FinalizeHTMLImageElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLImageElement class properties
//
static JSPropertySpec HTMLImageElementProperties[] =
{
  {"lowSrc",    HTMLIMAGEELEMENT_LOWSRC,    JSPROP_ENUMERATE},
  {"name",    HTMLIMAGEELEMENT_NAME,    JSPROP_ENUMERATE},
  {"align",    HTMLIMAGEELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"alt",    HTMLIMAGEELEMENT_ALT,    JSPROP_ENUMERATE},
  {"isMap",    HTMLIMAGEELEMENT_ISMAP,    JSPROP_ENUMERATE},
  {"longDesc",    HTMLIMAGEELEMENT_LONGDESC,    JSPROP_ENUMERATE},
  {"useMap",    HTMLIMAGEELEMENT_USEMAP,    JSPROP_ENUMERATE},
  {"lowsrc",    IMAGE_LOWSRC,    JSPROP_ENUMERATE},
  {"complete",    IMAGE_COMPLETE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"border",    IMAGE_BORDER,    JSPROP_ENUMERATE},
  {"height",    IMAGE_HEIGHT,    JSPROP_ENUMERATE},
  {"hspace",    IMAGE_HSPACE,    JSPROP_ENUMERATE},
  {"vspace",    IMAGE_VSPACE,    JSPROP_ENUMERATE},
  {"width",    IMAGE_WIDTH,    JSPROP_ENUMERATE},
  {"naturalHeight",    IMAGE_NATURALHEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"naturalWidth",    IMAGE_NATURALWIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLImageElement class methods
//
static JSFunctionSpec HTMLImageElementMethods[] = 
{
  {0}
};


//
// HTMLImageElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLImageElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID interfaceID, classID;
  PRBool isConstructor;
  nsIDOMHTMLImageElement *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMHTMLImageElementIID, NS_IDOMHTMLIMAGEELEMENT_IID);
  static NS_DEFINE_IID(kIJSNativeInitializerIID, NS_IJSNATIVEINITIALIZER_IID);

  nsCOMPtr<nsIScriptContext> scriptCX;
  nsJSUtils::nsGetStaticScriptContext(cx, obj, getter_AddRefs(scriptCX));
  if (!scriptCX) {
    return JS_FALSE;
  }

  nsCOMPtr<nsIScriptNameSpaceManager> manager;
  scriptCX->GetNameSpaceManager(getter_AddRefs(manager));
  if (!manager) {
    return JS_FALSE;
  }

  result = manager->LookupName(NS_ConvertASCIItoUCS2("HTMLImageElement"), isConstructor, interfaceID, classID);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMHTMLImageElementIID,
                                        (void **)&nativeThis);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nativeThis->QueryInterface(kIJSNativeInitializerIID, (void **)&initializer);
  if (NS_OK == result) {
    result = initializer->Initialize(cx, obj, argc, argv);
    NS_RELEASE(initializer);

    if (NS_OK != result) {
      NS_RELEASE(nativeThis);
      return JS_FALSE;
    }
  }

  result = nativeThis->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner);
  if (NS_OK != result) {
    NS_RELEASE(nativeThis);
    return JS_FALSE;
  }

  owner->SetScriptObject((void *)obj);
  JS_SetPrivate(cx, obj, nativeThis);

  NS_RELEASE(owner);
  return JS_TRUE;
}

//
// HTMLImageElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLImageElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLImageElement", &vp)) ||
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
                         &HTMLImageElementClass,      // JSClass
                         HTMLImageElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLImageElementProperties,  // proto props
                         HTMLImageElementMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    JS_AliasProperty(jscontext, global, "HTMLImageElement", "Image");
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
// Method for creating a new HTMLImageElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLImageElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLImageElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLImageElement *aHTMLImageElement;

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

  if (NS_OK != NS_InitHTMLImageElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLImageElementIID, (void **)&aHTMLImageElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLImageElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLImageElement);
  }
  else {
    NS_RELEASE(aHTMLImageElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
