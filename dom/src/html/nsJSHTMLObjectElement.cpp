/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLObjectElement.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIHTMLObjectElementIID, NS_IDOMHTMLOBJECTELEMENT_IID);

NS_DEF_PTR(nsIDOMHTMLFormElement);
NS_DEF_PTR(nsIDOMHTMLObjectElement);

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
  HTMLOBJECTELEMENT_WIDTH = -18
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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLOBJECTELEMENT_FORM:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.form", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLFormElement* prop;
        if (NS_SUCCEEDED(a->GetForm(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.code", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCode(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_ALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.align", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_ARCHIVE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.archive", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetArchive(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_BORDER:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.border", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorder(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODEBASE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.codebase", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCodeBase(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_CODETYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.codetype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCodeType(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_DATA:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.data", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetData(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_DECLARE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.declare", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetDeclare(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_HEIGHT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.height", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_HSPACE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.hspace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHspace(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.name", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetName(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_STANDBY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.standby", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetStandby(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.tabindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetTabIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.type", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetType(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_USEMAP:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.usemap", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetUseMap(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_VSPACE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.vspace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVspace(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLOBJECTELEMENT_WIDTH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.width", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

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

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsIScriptSecurityManager *secMan;
    PRBool ok = PR_FALSE;
    if (NS_OK != scriptCX->GetSecurityManager(&secMan)) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case HTMLOBJECTELEMENT_CODE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.code", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCode(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_ALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.align", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_ARCHIVE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.archive", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetArchive(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_BORDER:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.border", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorder(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_CODEBASE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.codebase", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCodeBase(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_CODETYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.codetype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCodeType(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_DATA:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.data", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetData(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_DECLARE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.declare", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToBool(&prop, cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetDeclare(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_HEIGHT:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.height", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHeight(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_HSPACE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.hspace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHspace(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_NAME:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.name", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_STANDBY:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.standby", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetStandby(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_TABINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.tabindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetTabIndex(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_TYPE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.type", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetType(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_USEMAP:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.usemap", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetUseMap(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_VSPACE:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.vspace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVspace(prop);
        
        break;
      }
      case HTMLOBJECTELEMENT_WIDTH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmlobjectelement.width", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWidth(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
    NS_RELEASE(secMan);
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


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
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLObjectElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLObjectElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
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
  FinalizeHTMLObjectElement
};


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
  {0}
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
