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
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLTableRowElement.h"
#include "nsIDOMHTMLCollection.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIHTMLElementIID, NS_IDOMHTMLELEMENT_IID);
static NS_DEFINE_IID(kIHTMLTableRowElementIID, NS_IDOMHTMLTABLEROWELEMENT_IID);
static NS_DEFINE_IID(kIHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);

NS_DEF_PTR(nsIDOMHTMLElement);
NS_DEF_PTR(nsIDOMHTMLTableRowElement);
NS_DEF_PTR(nsIDOMHTMLCollection);

//
// HTMLTableRowElement property ids
//
enum HTMLTableRowElement_slots {
  HTMLTABLEROWELEMENT_ROWINDEX = -1,
  HTMLTABLEROWELEMENT_SECTIONROWINDEX = -2,
  HTMLTABLEROWELEMENT_CELLS = -3,
  HTMLTABLEROWELEMENT_ALIGN = -4,
  HTMLTABLEROWELEMENT_BGCOLOR = -5,
  HTMLTABLEROWELEMENT_CH = -6,
  HTMLTABLEROWELEMENT_CHOFF = -7,
  HTMLTABLEROWELEMENT_VALIGN = -8
};

/***********************************************************************/
//
// HTMLTableRowElement Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetHTMLTableRowElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableRowElement *a = (nsIDOMHTMLTableRowElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLEROWELEMENT_ROWINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.rowindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetRowIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_SECTIONROWINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.sectionrowindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetSectionRowIndex(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_CELLS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.cells", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (NS_SUCCEEDED(a->GetCells(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_ALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.align", &ok);
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
      case HTMLTABLEROWELEMENT_BGCOLOR:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.bgcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBgColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_CH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.ch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCh(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_CHOFF:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.choff", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetChOff(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case HTMLTABLEROWELEMENT_VALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.valign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVAlign(prop))) {
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
// HTMLTableRowElement Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetHTMLTableRowElementProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMHTMLTableRowElement *a = (nsIDOMHTMLTableRowElement*)nsJSUtils::nsGetNativeThis(cx, obj);

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
      case HTMLTABLEROWELEMENT_ROWINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.rowindex", &ok);
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
      
        a->SetRowIndex(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_SECTIONROWINDEX:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.sectionrowindex", &ok);
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
      
        a->SetSectionRowIndex(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_CELLS:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.cells", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHTMLCollection* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIHTMLCollectionIID, "HTMLCollection",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetCells(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      case HTMLTABLEROWELEMENT_ALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.align", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAlign(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_BGCOLOR:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.bgcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBgColor(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_CH:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.ch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCh(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_CHOFF:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.choff", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetChOff(prop);
        
        break;
      }
      case HTMLTABLEROWELEMENT_VALIGN:
      {
        secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.valign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVAlign(prop);
        
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
// HTMLTableRowElement finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeHTMLTableRowElement(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// HTMLTableRowElement enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateHTMLTableRowElement(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// HTMLTableRowElement resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveHTMLTableRowElement(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method InsertCell
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableRowElementInsertCell(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableRowElement *nativeThis = (nsIDOMHTMLTableRowElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMHTMLElement* nativeRet;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.insertcell", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->InsertCell(b0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function insertCell requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method DeleteCell
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableRowElementDeleteCell(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMHTMLTableRowElement *nativeThis = (nsIDOMHTMLTableRowElement*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsIScriptSecurityManager *secMan;
  if (NS_OK == scriptCX->GetSecurityManager(&secMan)) {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "htmltablerowelement.deletecell", &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
    NS_RELEASE(secMan);
  }
  else {
    return JS_FALSE;
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->DeleteCell(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function deleteCell requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for HTMLTableRowElement
//
JSClass HTMLTableRowElementClass = {
  "HTMLTableRowElement", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetHTMLTableRowElementProperty,
  SetHTMLTableRowElementProperty,
  EnumerateHTMLTableRowElement,
  ResolveHTMLTableRowElement,
  JS_ConvertStub,
  FinalizeHTMLTableRowElement
};


//
// HTMLTableRowElement class properties
//
static JSPropertySpec HTMLTableRowElementProperties[] =
{
  {"rowIndex",    HTMLTABLEROWELEMENT_ROWINDEX,    JSPROP_ENUMERATE},
  {"sectionRowIndex",    HTMLTABLEROWELEMENT_SECTIONROWINDEX,    JSPROP_ENUMERATE},
  {"cells",    HTMLTABLEROWELEMENT_CELLS,    JSPROP_ENUMERATE},
  {"align",    HTMLTABLEROWELEMENT_ALIGN,    JSPROP_ENUMERATE},
  {"bgColor",    HTMLTABLEROWELEMENT_BGCOLOR,    JSPROP_ENUMERATE},
  {"ch",    HTMLTABLEROWELEMENT_CH,    JSPROP_ENUMERATE},
  {"chOff",    HTMLTABLEROWELEMENT_CHOFF,    JSPROP_ENUMERATE},
  {"vAlign",    HTMLTABLEROWELEMENT_VALIGN,    JSPROP_ENUMERATE},
  {0}
};


//
// HTMLTableRowElement class methods
//
static JSFunctionSpec HTMLTableRowElementMethods[] = 
{
  {"insertCell",          HTMLTableRowElementInsertCell,     1},
  {"deleteCell",          HTMLTableRowElementDeleteCell,     1},
  {0}
};


//
// HTMLTableRowElement constructor
//
PR_STATIC_CALLBACK(JSBool)
HTMLTableRowElement(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// HTMLTableRowElement class initialization
//
extern "C" NS_DOM nsresult NS_InitHTMLTableRowElementClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "HTMLTableRowElement", &vp)) ||
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
                         &HTMLTableRowElementClass,      // JSClass
                         HTMLTableRowElement,            // JSNative ctor
                         0,             // ctor args
                         HTMLTableRowElementProperties,  // proto props
                         HTMLTableRowElementMethods,     // proto funcs
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
// Method for creating a new HTMLTableRowElement JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptHTMLTableRowElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptHTMLTableRowElement");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMHTMLTableRowElement *aHTMLTableRowElement;

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

  if (NS_OK != NS_InitHTMLTableRowElementClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kIHTMLTableRowElementIID, (void **)&aHTMLTableRowElement);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &HTMLTableRowElementClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aHTMLTableRowElement);
  }
  else {
    NS_RELEASE(aHTMLTableRowElement);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
