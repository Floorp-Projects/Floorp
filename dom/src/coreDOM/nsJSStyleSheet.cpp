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
#include "nsIDOMStyleSheet.h"
#include "nsIDOMMediaList.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kIMediaListIID, NS_IDOMMEDIALIST_IID);

//
// StyleSheet property ids
//
enum StyleSheet_slots {
  STYLESHEET_TYPE = -1,
  STYLESHEET_DISABLED = -2,
  STYLESHEET_OWNERNODE = -3,
  STYLESHEET_PARENTSTYLESHEET = -4,
  STYLESHEET_HREF = -5,
  STYLESHEET_TITLE = -6,
  STYLESHEET_MEDIA = -7
};

/***********************************************************************/
//
// StyleSheet Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetStyleSheetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMStyleSheet *a = (nsIDOMStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case STYLESHEET_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case STYLESHEET_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_DISABLED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetDisabled(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case STYLESHEET_OWNERNODE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_OWNERNODE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNode* prop;
          rv = a->GetOwnerNode(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case STYLESHEET_PARENTSTYLESHEET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_PARENTSTYLESHEET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMStyleSheet* prop;
          rv = a->GetParentStyleSheet(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case STYLESHEET_HREF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_HREF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHref(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case STYLESHEET_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_TITLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTitle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case STYLESHEET_MEDIA:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_MEDIA, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMMediaList* prop;
          rv = a->GetMedia(&prop);
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
// StyleSheet Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetStyleSheetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMStyleSheet *a = (nsIDOMStyleSheet*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case STYLESHEET_DISABLED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_STYLESHEET_DISABLED, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_BOOLEAN_ERR;
            break;
          }
      
          rv = a->SetDisabled(prop);
          
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
// StyleSheet class properties
//
static JSPropertySpec StyleSheetProperties[] =
{
  {"type",    STYLESHEET_TYPE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"disabled",    STYLESHEET_DISABLED,    JSPROP_ENUMERATE},
  {"ownerNode",    STYLESHEET_OWNERNODE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"parentStyleSheet",    STYLESHEET_PARENTSTYLESHEET,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"href",    STYLESHEET_HREF,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"title",    STYLESHEET_TITLE,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"media",    STYLESHEET_MEDIA,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// StyleSheet finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeStyleSheet(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// StyleSheet enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateStyleSheet(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// StyleSheet resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveStyleSheet(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for StyleSheet
//
JSClass StyleSheetClass = {
  "StyleSheet", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetStyleSheetProperty,
  SetStyleSheetProperty,
  EnumerateStyleSheet,
  ResolveStyleSheet,
  JS_ConvertStub,
  FinalizeStyleSheet,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// StyleSheet class methods
//
static JSFunctionSpec StyleSheetMethods[] = 
{
  {0}
};


//
// StyleSheet constructor
//
PR_STATIC_CALLBACK(JSBool)
StyleSheet(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// StyleSheet class initialization
//
extern "C" NS_DOM nsresult NS_InitStyleSheetClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "StyleSheet", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &StyleSheetClass,      // JSClass
                         StyleSheet,            // JSNative ctor
                         0,             // ctor args
                         StyleSheetProperties,  // proto props
                         StyleSheetMethods,     // proto funcs
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
// Method for creating a new StyleSheet JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptStyleSheet(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptStyleSheet");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMStyleSheet *aStyleSheet;

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

  if (NS_OK != NS_InitStyleSheetClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIStyleSheetIID, (void **)&aStyleSheet);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &StyleSheetClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aStyleSheet);
  }
  else {
    NS_RELEASE(aStyleSheet);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
