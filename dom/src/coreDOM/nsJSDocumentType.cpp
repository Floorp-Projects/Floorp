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
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMDocumentType.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIDocumentTypeIID, NS_IDOMDOCUMENTTYPE_IID);

//
// DocumentType property ids
//
enum DocumentType_slots {
  DOCUMENTTYPE_NAME = -1,
  DOCUMENTTYPE_ENTITIES = -2,
  DOCUMENTTYPE_NOTATIONS = -3,
  DOCUMENTTYPE_PUBLICID = -4,
  DOCUMENTTYPE_SYSTEMID = -5,
  DOCUMENTTYPE_INTERNALSUBSET = -6
};

/***********************************************************************/
//
// DocumentType Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetDocumentTypeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocumentType *a = (nsIDOMDocumentType*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case DOCUMENTTYPE_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case DOCUMENTTYPE_ENTITIES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_ENTITIES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNamedNodeMap* prop;
          rv = a->GetEntities(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case DOCUMENTTYPE_NOTATIONS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_NOTATIONS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNamedNodeMap* prop;
          rv = a->GetNotations(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case DOCUMENTTYPE_PUBLICID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_PUBLICID, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPublicId(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case DOCUMENTTYPE_SYSTEMID:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_SYSTEMID, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSystemId(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case DOCUMENTTYPE_INTERNALSUBSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_DOCUMENTTYPE_INTERNALSUBSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetInternalSubset(prop);
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
// DocumentType Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetDocumentTypeProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMDocumentType *a = (nsIDOMDocumentType*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// DocumentType class properties
//
static JSPropertySpec DocumentTypeProperties[] =
{
  {"name",    DOCUMENTTYPE_NAME,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"entities",    DOCUMENTTYPE_ENTITIES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"notations",    DOCUMENTTYPE_NOTATIONS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"publicId",    DOCUMENTTYPE_PUBLICID,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"systemId",    DOCUMENTTYPE_SYSTEMID,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"internalSubset",    DOCUMENTTYPE_INTERNALSUBSET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// DocumentType finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeDocumentType(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// DocumentType enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateDocumentType(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// DocumentType resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveDocumentType(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for DocumentType
//
JSClass DocumentTypeClass = {
  "DocumentType", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetDocumentTypeProperty,
  SetDocumentTypeProperty,
  EnumerateDocumentType,
  ResolveDocumentType,
  JS_ConvertStub,
  FinalizeDocumentType,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// DocumentType class methods
//
static JSFunctionSpec DocumentTypeMethods[] = 
{
  {0}
};


//
// DocumentType constructor
//
PR_STATIC_CALLBACK(JSBool)
DocumentType(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// DocumentType class initialization
//
extern "C" NS_DOM nsresult NS_InitDocumentTypeClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "DocumentType", &vp)) ||
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
                         &DocumentTypeClass,      // JSClass
                         DocumentType,            // JSNative ctor
                         0,             // ctor args
                         DocumentTypeProperties,  // proto props
                         DocumentTypeMethods,     // proto funcs
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
// Method for creating a new DocumentType JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptDocumentType(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptDocumentType");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMDocumentType *aDocumentType;

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

  if (NS_OK != NS_InitDocumentTypeClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIDocumentTypeIID, (void **)&aDocumentType);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &DocumentTypeClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aDocumentType);
  }
  else {
    NS_RELEASE(aDocumentType);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
