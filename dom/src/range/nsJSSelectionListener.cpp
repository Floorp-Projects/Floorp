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
#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMSelectionListener.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kISelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kISelectionListenerIID, NS_IDOMSELECTIONLISTENER_IID);


/***********************************************************************/
//
// SelectionListener Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetSelectionListenerProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSelectionListener *a = (nsIDOMSelectionListener*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// SelectionListener Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetSelectionListenerProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMSelectionListener *a = (nsIDOMSelectionListener*)nsJSUtils::nsGetNativeThis(cx, obj);

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
// SelectionListener finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeSelectionListener(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// SelectionListener enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateSelectionListener(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// SelectionListener resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveSelectionListener(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


//
// Native method NotifySelectionChanged
//
PR_STATIC_CALLBACK(JSBool)
SelectionListenerNotifySelectionChanged(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMSelectionListener *nativeThis = (nsIDOMSelectionListener*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMDocument> b0;
  nsCOMPtr<nsIDOMSelection> b1;
  PRInt32 b2;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_SELECTIONLISTENER_NOTIFYSELECTIONCHANGED, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIDocumentIID,
                                           NS_ConvertASCIItoUCS2("Document"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b1),
                                           kISelectionIID,
                                           NS_ConvertASCIItoUCS2("Selection"),
                                           cx,
                                           argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[2], (int32 *)&b2)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->NotifySelectionChanged(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for SelectionListener
//
JSClass SelectionListenerClass = {
  "SelectionListener", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetSelectionListenerProperty,
  SetSelectionListenerProperty,
  EnumerateSelectionListener,
  ResolveSelectionListener,
  JS_ConvertStub,
  FinalizeSelectionListener,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// SelectionListener class properties
//
static JSPropertySpec SelectionListenerProperties[] =
{
  {0}
};


//
// SelectionListener class methods
//
static JSFunctionSpec SelectionListenerMethods[] = 
{
  {"notifySelectionChanged",          SelectionListenerNotifySelectionChanged,     3},
  {0}
};


//
// SelectionListener constructor
//
PR_STATIC_CALLBACK(JSBool)
SelectionListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// SelectionListener class initialization
//
extern "C" NS_DOM nsresult NS_InitSelectionListenerClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "SelectionListener", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &SelectionListenerClass,      // JSClass
                         SelectionListener,            // JSNative ctor
                         0,             // ctor args
                         SelectionListenerProperties,  // proto props
                         SelectionListenerMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

    if ((PR_TRUE == JS_LookupProperty(jscontext, global, "SelectionListener", &vp)) &&
        JSVAL_IS_OBJECT(vp) &&
        ((constructor = JSVAL_TO_OBJECT(vp)) != nsnull)) {
      vp = INT_TO_JSVAL(nsIDOMSelectionListener::NO_REASON);
      JS_SetProperty(jscontext, constructor, "NO_REASON", &vp);

      vp = INT_TO_JSVAL(nsIDOMSelectionListener::DRAG_REASON);
      JS_SetProperty(jscontext, constructor, "DRAG_REASON", &vp);

      vp = INT_TO_JSVAL(nsIDOMSelectionListener::MOUSEDOWN_REASON);
      JS_SetProperty(jscontext, constructor, "MOUSEDOWN_REASON", &vp);

      vp = INT_TO_JSVAL(nsIDOMSelectionListener::MOUSEUP_REASON);
      JS_SetProperty(jscontext, constructor, "MOUSEUP_REASON", &vp);

      vp = INT_TO_JSVAL(nsIDOMSelectionListener::KEYPRESS_REASON);
      JS_SetProperty(jscontext, constructor, "KEYPRESS_REASON", &vp);

      vp = INT_TO_JSVAL(nsIDOMSelectionListener::SELECTALL_REASON);
      JS_SetProperty(jscontext, constructor, "SELECTALL_REASON", &vp);

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
// Method for creating a new SelectionListener JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptSelectionListener(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptSelectionListener");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMSelectionListener *aSelectionListener;

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

  if (NS_OK != NS_InitSelectionListenerClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kISelectionListenerIID, (void **)&aSelectionListener);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &SelectionListenerClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aSelectionListener);
  }
  else {
    NS_RELEASE(aSelectionListener);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
