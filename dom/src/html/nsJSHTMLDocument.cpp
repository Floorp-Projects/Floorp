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
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMNodeList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kINSHTMLDocumentIID, NS_IDOMNSHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
static NS_DEFINE_IID(kINodeListIID, NS_IDOMNODELIST_IID);

//
// HTMLDocument property ids
//
enum HTMLDocument_slots {
  HTMLDOCUMENT_TITLE = -1,
  HTMLDOCUMENT_REFERRER = -2,
  HTMLDOCUMENT_DOMAIN = -3,
  HTMLDOCUMENT_URL = -4,
  HTMLDOCUMENT_BODY = -5,
  HTMLDOCUMENT_IMAGES = -6,
  HTMLDOCUMENT_APPLETS = -7,
  HTMLDOCUMENT_LINKS = -8,
  HTMLDOCUMENT_FORMS = -9,
  HTMLDOCUMENT_ANCHORS = -10,
  HTMLDOCUMENT_COOKIE = -11,
  NSHTMLDOCUMENT_WIDTH = -12,
  NSHTMLDOCUMENT_HEIGHT = -13,
  NSHTMLDOCUMENT_ALINKCOLOR = -14,
  NSHTMLDOCUMENT_LINKCOLOR = -15,
  NSHTMLDOCUMENT_VLINKCOLOR = -16,
  NSHTMLDOCUMENT_BGCOLOR = -17,
  NSHTMLDOCUMENT_FGCOLOR = -18,
  NSHTMLDOCUMENT_LASTMODIFIED = -19,
  NSHTMLDOCUMENT_EMBEDS = -20
};

/***********************************************************************/
//
// HTMLDocument Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLDocument *a = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case HTMLDOCUMENT_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_TITLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTitle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_REFERRER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_REFERRER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetReferrer(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_DOMAIN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_DOMAIN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDomain(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_URL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_URL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetURL(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_BODY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_BODY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLElement* prop;
          rv = a->GetBody(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_IMAGES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_IMAGES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetImages(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_APPLETS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_APPLETS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetApplets(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_LINKS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_LINKS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetLinks(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_FORMS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_FORMS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetForms(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_ANCHORS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_ANCHORS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          rv = a->GetAnchors(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case HTMLDOCUMENT_COOKIE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_COOKIE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCookie(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case NSHTMLDOCUMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
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
      case NSHTMLDOCUMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
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
      case NSHTMLDOCUMENT_ALINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_ALINKCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetAlinkColor(prop);
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
      case NSHTMLDOCUMENT_LINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_LINKCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetLinkColor(prop);
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
      case NSHTMLDOCUMENT_VLINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_VLINKCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetVlinkColor(prop);
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
      case NSHTMLDOCUMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_BGCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetBgColor(prop);
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
      case NSHTMLDOCUMENT_FGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_FGCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetFgColor(prop);
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
      case NSHTMLDOCUMENT_LASTMODIFIED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_LASTMODIFIED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetLastModified(prop);
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
      case NSHTMLDOCUMENT_EMBEDS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_EMBEDS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLCollection* prop;
          nsIDOMNSHTMLDocument* b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            rv = b->GetEmbeds(&prop);
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
        checkNamedItem = PR_TRUE;
    }
  }

  if (checkNamedItem) {
    nsIDOMNSHTMLDocument* b;
    nsresult result = NS_OK;
    if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
      result = b->NamedItem(cx, &id, 1, vp);
      NS_RELEASE(b);
      if (NS_FAILED(result)) {
        return nsJSUtils::nsReportError(cx, obj, result);
      }
    }
    else {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
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
// HTMLDocument Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLDocumentProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLDocument *a = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  PRBool checkNamedItem = PR_TRUE;
  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    checkNamedItem = PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case HTMLDOCUMENT_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_TITLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTitle(prop);
          
        }
        break;
      }
      case HTMLDOCUMENT_DOMAIN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_DOMAIN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDomain(prop);
          
        }
        break;
      }
      case HTMLDOCUMENT_BODY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_BODY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHTMLElement* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIHTMLElementIID, NS_ConvertASCIItoUCS2("HTMLElement"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetBody(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case HTMLDOCUMENT_COOKIE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_COOKIE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCookie(prop);
          
        }
        break;
      }
      case NSHTMLDOCUMENT_ALINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_ALINKCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMNSHTMLDocument *b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            b->SetAlinkColor(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case NSHTMLDOCUMENT_LINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_LINKCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMNSHTMLDocument *b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            b->SetLinkColor(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case NSHTMLDOCUMENT_VLINKCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_VLINKCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMNSHTMLDocument *b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            b->SetVlinkColor(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case NSHTMLDOCUMENT_BGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_BGCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMNSHTMLDocument *b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            b->SetBgColor(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case NSHTMLDOCUMENT_FGCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_FGCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMNSHTMLDocument *b;
          if (NS_OK == a->QueryInterface(kINSHTMLDocumentIID, (void **)&b)) {
            b->SetFgColor(prop);
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
// HTMLDocument finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLDocument(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLDocument enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLDocument(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLDocument resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLDocument(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method Close
//
PR_STATIC_CALLBACK(JSBool)
HTMLDocumentClose(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *nativeThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_CLOSE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Close();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetElementsByName
//
PR_STATIC_CALLBACK(JSBool)
HTMLDocumentGetElementsByName(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *nativeThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMNodeList* nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLDOCUMENT_GETELEMENTSBYNAME, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->GetElementsByName(b0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method GetSelection
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentGetSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_GETSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetSelection(nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method NamedItem
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentNamedItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  jsval nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_NAMEDITEM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->NamedItem(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = nativeRet;
  }

  return JS_TRUE;
}


//
// Native method Open
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentOpen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_OPEN, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Open(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Write
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentWrite(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_WRITE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Write(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Writeln
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentWriteln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_WRITELN, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Writeln(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Clear
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentClear(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_CLEAR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Clear(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method CaptureEvents
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentCaptureEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_CAPTUREEVENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->CaptureEvents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ReleaseEvents
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentReleaseEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_RELEASEEVENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ReleaseEvents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RouteEvent
//
PR_STATIC_CALLBACK(JSBool)
NSHTMLDocumentRouteEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLDocument *privateThis = (nsIDOMHTMLDocument*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMNSHTMLDocument> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kINSHTMLDocumentIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsCOMPtr<nsIDOMEvent> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_NSHTMLDOCUMENT_ROUTEEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIEventIID,
                                           NS_ConvertASCIItoUCS2("Event"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RouteEvent(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLDocument
//
JSClass HTMLDocumentClass = {
  "HTMLDocument", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLDocumentProperty,
  SetHTMLDocumentProperty,
  EnumerateHTMLDocument,
  ResolveHTMLDocument,
  JS_ConvertStub,
  FinalizeHTMLDocument,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLDocument class properties
//
static JSPropertySpec HTMLDocumentProperties[] =
{
  {"title",    HTMLDOCUMENT_TITLE,    JSPROP_ENUMERATE},
  {"referrer",    HTMLDOCUMENT_REFERRER,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"domain",    HTMLDOCUMENT_DOMAIN,    JSPROP_ENUMERATE},
  {"URL",    HTMLDOCUMENT_URL,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"body",    HTMLDOCUMENT_BODY,    JSPROP_ENUMERATE},
  {"images",    HTMLDOCUMENT_IMAGES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"applets",    HTMLDOCUMENT_APPLETS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"links",    HTMLDOCUMENT_LINKS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"forms",    HTMLDOCUMENT_FORMS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"anchors",    HTMLDOCUMENT_ANCHORS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"cookie",    HTMLDOCUMENT_COOKIE,    JSPROP_ENUMERATE},
  {"width",    NSHTMLDOCUMENT_WIDTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"height",    NSHTMLDOCUMENT_HEIGHT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"alinkColor",    NSHTMLDOCUMENT_ALINKCOLOR,    JSPROP_ENUMERATE},
  {"linkColor",    NSHTMLDOCUMENT_LINKCOLOR,    JSPROP_ENUMERATE},
  {"vlinkColor",    NSHTMLDOCUMENT_VLINKCOLOR,    JSPROP_ENUMERATE},
  {"bgColor",    NSHTMLDOCUMENT_BGCOLOR,    JSPROP_ENUMERATE},
  {"fgColor",    NSHTMLDOCUMENT_FGCOLOR,    JSPROP_ENUMERATE},
  {"lastModified",    NSHTMLDOCUMENT_LASTMODIFIED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"embeds",    NSHTMLDOCUMENT_EMBEDS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// HTMLDocument class methods
//
static JSFunctionSpec HTMLDocumentMethods[] = 
{
  {"close",          HTMLDocumentClose,     0},
  {"getElementsByName",          HTMLDocumentGetElementsByName,     1},
  {"getSelection",          NSHTMLDocumentGetSelection,     0},
  {"namedItem",          NSHTMLDocumentNamedItem,     0},
  {"open",          NSHTMLDocumentOpen,     0},
  {"write",          NSHTMLDocumentWrite,     0},
  {"writeln",          NSHTMLDocumentWriteln,     0},
  {"clear",          NSHTMLDocumentClear,     0},
  {"captureEvents",          NSHTMLDocumentCaptureEvents,     1},
  {"releaseEvents",          NSHTMLDocumentReleaseEvents,     1},
  {"routeEvent",          NSHTMLDocumentRouteEvent,     1},
  {0}
};


//
// HTMLDocument constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLDocument(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLDocument class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLDocumentClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLDocument", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitDocumentClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &HTMLDocumentClass,      // JSClass
                         HTMLDocument,            // JSNative ctor
                         0,             // ctor args
                         HTMLDocumentProperties,  // proto props
                         HTMLDocumentMethods,     // proto funcs
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
// Method for creating a new HTMLDocument JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLDocument");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLDocument *aHTMLDocument;

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

  if (NS_OK != NS_InitHTMLDocumentClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLDocumentIID, (void **)&aHTMLDocument);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLDocumentClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLDocument);
  }
  else {
    NS_RELEASE(aHTMLDocument);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
