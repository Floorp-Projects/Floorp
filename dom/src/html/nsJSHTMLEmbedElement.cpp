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
#include "nsIDOMPlugin.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIComponentManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsDOMCID.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IDOMPLUGIN_IID);
static NS_DEFINE_IID(kIHTMLEmbedElementIID, NS_IDOMHTMLEMBEDELEMENT_IID);

//
// HTMLEmbedElement property ids
//
enum HTMLEmbedElement_slots {
  HTMLEMBEDELEMENT_ALIGN = -1,
  HTMLEMBEDELEMENT_HEIGHT = -2,
  HTMLEMBEDELEMENT_NAME = -3,
  HTMLEMBEDELEMENT_SRC = -4,
  HTMLEMBEDELEMENT_TYPE = -5,
  HTMLEMBEDELEMENT_WIDTH = -6
};

/***********************************************************************/
//
// HTMLEmbedElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLEmbedElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLEmbedElement *a = (nsIDOMHTMLEmbedElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLEMBEDELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_ALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLEMBEDELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLEMBEDELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLEMBEDELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_SRC, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSrc(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLEMBEDELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_TYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case HTMLEMBEDELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_WIDTH, PR_FALSE);
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
// HTMLEmbedElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLEmbedElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLEmbedElement *a = (nsIDOMHTMLEmbedElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLEMBEDELEMENT_ALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_ALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAlign(prop);
          
        }
        break;
      }
      case HTMLEMBEDELEMENT_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case HTMLEMBEDELEMENT_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case HTMLEMBEDELEMENT_SRC:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_SRC, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSrc(prop);
          
        }
        break;
      }
      case HTMLEMBEDELEMENT_TYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_TYPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetType(prop);
          
        }
        break;
      }
      case HTMLEMBEDELEMENT_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_HTMLEMBEDELEMENT_WIDTH, PR_TRUE);
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
// HTMLEmbedElement class properties
//
static JSPropertySpec HTMLEmbedElementProperties[] =
{
  {"align",    HTMLEMBEDELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"height",    HTMLEMBEDELEMENT_HEIGHT,    JSPROP_ENUMERATE},
  {"name",    HTMLEMBEDELEMENT_NAME,    JSPROP_ENUMERATE},
  {"src",    HTMLEMBEDELEMENT_SRC,    JSPROP_ENUMERATE},
  {"type",    HTMLEMBEDELEMENT_TYPE,    JSPROP_ENUMERATE},
  {"width",    HTMLEMBEDELEMENT_WIDTH,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLEmbedElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLEmbedElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLEmbedElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLEmbedElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj, nsnull);
}


//
// HTMLEmbedElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLEmbedElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id, nsnull);
}


/***********************************************************************/
//
// class for HTMLEmbedElement
//
JSClass HTMLEmbedElementClass = {
  "HTMLEmbedElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLEmbedElementProperty,
  SetHTMLEmbedElementProperty,
  EnumerateHTMLEmbedElement,
  ResolveHTMLEmbedElement,
  JS_ConvertStub,
  FinalizeHTMLEmbedElement,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// HTMLEmbedElement class methods
//
static JSFunctionSpec HTMLEmbedElementMethods[] = 
{
  {0}
};


//
// HTMLEmbedElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLEmbedElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsresult result;
  nsIID interfaceID, classID;
  PRBool isConstructor;
  nsIDOMHTMLEmbedElement *nativeThis;
  nsIScriptObjectOwner *owner = nsnull;
  nsIJSNativeInitializer* initializer = nsnull;

  static NS_DEFINE_IID(kIDOMHTMLEmbedElementIID, NS_IDOMHTMLEMBEDELEMENT_IID);
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

  result = manager->LookupName(NS_ConvertASCIItoUCS2("HTMLEmbedElement"), isConstructor, interfaceID, classID);
  if (NS_OK != result) {
    return JS_FALSE;
  }

  result = nsComponentManager::CreateInstance(classID,
                                        nsnull,
                                        kIDOMHTMLEmbedElementIID,
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
// HTMLEmbedElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLEmbedElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLEmbedElement", &vp)) ||
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
                         &HTMLEmbedElementClass,      // JSClass
                         HTMLEmbedElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLEmbedElementProperties,  // proto props
                         HTMLEmbedElementMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    JS_AliasProperty(jscontext, global, "HTMLEmbedElement", "Plugin");
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
// Method for creating a new HTMLEmbedElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLEmbedElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLEmbedElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLEmbedElement *aHTMLEmbedElement;

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

  if (NS_OK != NS_InitHTMLEmbedElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLEmbedElementIID, (void **)&aHTMLEmbedElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLEmbedElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLEmbedElement);
  }
  else {
    NS_RELEASE(aHTMLEmbedElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
