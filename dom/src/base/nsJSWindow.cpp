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
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScreen.h"
#include "nsIDOMHistory.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventCapturer.h"
#include "nsIDOMWindow.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kIHistoryIID, NS_IDOMHISTORY_IID);
static NS_DEFINE_IID(kIEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kIEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMNavigator);
NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMScreen);
NS_DEF_PTR(nsIDOMHistory);
NS_DEF_PTR(nsIDOMEventListener);
NS_DEF_PTR(nsIDOMWindowCollection);
NS_DEF_PTR(nsIDOMEventTarget);
NS_DEF_PTR(nsIDOMEventCapturer);
NS_DEF_PTR(nsIDOMWindow);

//
// Window property ids
//
enum Window_slots {
  WINDOW_WINDOW = -1,
  WINDOW_SELF = -2,
  WINDOW_DOCUMENT = -3,
  WINDOW_NAVIGATOR = -4,
  WINDOW_SCREEN = -5,
  WINDOW_HISTORY = -6,
  WINDOW_PARENT = -7,
  WINDOW_TOP = -8,
  WINDOW_CLOSED = -9,
  WINDOW_FRAMES = -10,
  WINDOW_OPENER = -11,
  WINDOW_STATUS = -12,
  WINDOW_DEFAULTSTATUS = -13,
  WINDOW_NAME = -14,
  WINDOW_INNERWIDTH = -15,
  WINDOW_INNERHEIGHT = -16,
  WINDOW_OUTERWIDTH = -17,
  WINDOW_OUTERHEIGHT = -18,
  WINDOW_SCREENX = -19,
  WINDOW_SCREENY = -20,
  WINDOW_PAGEXOFFSET = -21,
  WINDOW_PAGEYOFFSET = -22
};

/***********************************************************************/
//
// Window Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_WINDOW:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetWindow(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SELF:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetSelf(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_DOCUMENT:
      {
        nsIDOMDocument* prop;
        if (NS_OK == a->GetDocument(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_NAVIGATOR:
      {
        nsIDOMNavigator* prop;
        if (NS_OK == a->GetNavigator(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCREEN:
      {
        nsIDOMScreen* prop;
        if (NS_OK == a->GetScreen(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_HISTORY:
      {
        nsIDOMHistory* prop;
        if (NS_OK == a->GetHistory(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PARENT:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetParent(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_TOP:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetTop(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_CLOSED:
      {
        PRBool prop;
        if (NS_OK == a->GetClosed(&prop)) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_FRAMES:
      {
        nsIDOMWindowCollection* prop;
        if (NS_OK == a->GetFrames(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_OPENER:
      {
        nsIDOMWindow* prop;
        if (NS_OK == a->GetOpener(&prop)) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_STATUS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetStatus(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetDefaultStatus(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_NAME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetName(prop)) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_INNERWIDTH:
      {
        PRInt32 prop;
        if (NS_OK == a->GetInnerWidth(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        PRInt32 prop;
        if (NS_OK == a->GetInnerHeight(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        PRInt32 prop;
        if (NS_OK == a->GetOuterWidth(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        PRInt32 prop;
        if (NS_OK == a->GetOuterHeight(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCREENX:
      {
        PRInt32 prop;
        if (NS_OK == a->GetScreenX(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCREENY:
      {
        PRInt32 prop;
        if (NS_OK == a->GetScreenY(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetPageXOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        PRInt32 prop;
        if (NS_OK == a->GetPageYOffset(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// Window Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_OPENER:
      {
        nsIDOMWindow* prop;
        if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                kIWindowIID, "Window",
                                                cx, *vp)) {
          return JS_FALSE;
        }
      
        a->SetOpener(prop);
        NS_IF_RELEASE(prop);
        break;
      }
      case WINDOW_STATUS:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetStatus(prop);
        
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetDefaultStatus(prop);
        
        break;
      }
      case WINDOW_NAME:
      {
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case WINDOW_INNERWIDTH:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetInnerWidth(prop);
        
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetInnerHeight(prop);
        
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetOuterWidth(prop);
        
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetOuterHeight(prop);
        
        break;
      }
      case WINDOW_SCREENX:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetScreenX(prop);
        
        break;
      }
      case WINDOW_SCREENY:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetScreenY(prop);
        
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetPageXOffset(prop);
        
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        PRInt32 prop;
        int32 temp;
        if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
          prop = (PRInt32)temp;
        }
        else {
          JS_ReportError(cx, "Parameter must be a number");
          return JS_FALSE;
        }
      
        a->SetPageYOffset(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

  return PR_TRUE;
}


//
// Window finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeWindow(JSContext *cx, JSObject *obj)
{
}


//
// Window enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateWindow(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Window resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveWindow(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGlobalResolve(cx, obj, id);
}


//
// Native method Dump
//
PR_STATIC_CALLBACK(JSBool)
WindowDump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->Dump(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function dump requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Alert
//
PR_STATIC_CALLBACK(JSBool)
WindowAlert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->Alert(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function alert requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
WindowFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Focus()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function focus requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
WindowBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Blur()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function blur requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Close
//
PR_STATIC_CALLBACK(JSBool)
WindowClose(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Close()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function close requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Back
//
PR_STATIC_CALLBACK(JSBool)
WindowBack(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Back()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function back requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Forward
//
PR_STATIC_CALLBACK(JSBool)
WindowForward(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Forward()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function forward requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Home
//
PR_STATIC_CALLBACK(JSBool)
WindowHome(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Home()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function home requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Stop
//
PR_STATIC_CALLBACK(JSBool)
WindowStop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Stop()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function stop requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Print
//
PR_STATIC_CALLBACK(JSBool)
WindowPrint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Print()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function print requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method MoveTo
//
PR_STATIC_CALLBACK(JSBool)
WindowMoveTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->MoveTo(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function moveTo requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method MoveBy
//
PR_STATIC_CALLBACK(JSBool)
WindowMoveBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->MoveBy(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function moveBy requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ResizeTo
//
PR_STATIC_CALLBACK(JSBool)
WindowResizeTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ResizeTo(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function resizeTo requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ResizeBy
//
PR_STATIC_CALLBACK(JSBool)
WindowResizeBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ResizeBy(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function resizeBy requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ScrollTo
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ScrollTo(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function scrollTo requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ScrollBy
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 2) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ScrollBy(b0, b1)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function scrollBy requires 2 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ClearTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowClearTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ClearTimeout(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function clearTimeout requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ClearInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowClearInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ClearInterval(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function clearInterval requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowSetTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->SetTimeout(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function setTimeout requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowSetInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->SetInterval(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }
  else {
    JS_ReportError(cx, "Function setInterval requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Open
//
PR_STATIC_CALLBACK(JSBool)
WindowOpen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsIDOMWindow* nativeRet;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 0) {

    if (NS_OK != nativeThis->Open(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }
  else {
    JS_ReportError(cx, "Function open requires 0 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method CaptureEvent
//
PR_STATIC_CALLBACK(JSBool)
EventCapturerCaptureEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  nsIDOMEventCapturer *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventCapturerIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventCapturer");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->CaptureEvent(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function captureEvent requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method ReleaseEvent
//
PR_STATIC_CALLBACK(JSBool)
EventCapturerReleaseEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  nsIDOMEventCapturer *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventCapturerIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventCapturer");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->ReleaseEvent(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function releaseEvent requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method AddEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetAddEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;
  PRBool b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (!nsJSUtils::nsConvertJSValToFunc(&b1,
                                         cx,
                                         obj,
                                         argv[1])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->AddEventListener(b0, b1, b2, b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function addEventListener requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method RemoveEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetRemoveEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)JS_GetPrivate(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;
  PRBool b3;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 4) {

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (!nsJSUtils::nsConvertJSValToFunc(&b1,
                                         cx,
                                         obj,
                                         argv[1])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return JS_FALSE;
    }

    if (!nsJSUtils::nsConvertJSValToBool(&b3, cx, argv[3])) {
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->RemoveEventListener(b0, b1, b2, b3)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function removeEventListener requires 4 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Window
//
JSClass WindowClass = {
  "Window", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetWindowProperty,
  SetWindowProperty,
  EnumerateWindow,
  ResolveWindow,
  JS_ConvertStub,
  FinalizeWindow
};


//
// Window class properties
//
static JSPropertySpec WindowProperties[] =
{
  {"window",    WINDOW_WINDOW,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"self",    WINDOW_SELF,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"document",    WINDOW_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"navigator",    WINDOW_NAVIGATOR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"screen",    WINDOW_SCREEN,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"history",    WINDOW_HISTORY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"parent",    WINDOW_PARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"top",    WINDOW_TOP,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"closed",    WINDOW_CLOSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"frames",    WINDOW_FRAMES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"opener",    WINDOW_OPENER,    JSPROP_ENUMERATE},
  {"status",    WINDOW_STATUS,    JSPROP_ENUMERATE},
  {"defaultStatus",    WINDOW_DEFAULTSTATUS,    JSPROP_ENUMERATE},
  {"name",    WINDOW_NAME,    JSPROP_ENUMERATE},
  {"innerWidth",    WINDOW_INNERWIDTH,    JSPROP_ENUMERATE},
  {"innerHeight",    WINDOW_INNERHEIGHT,    JSPROP_ENUMERATE},
  {"outerWidth",    WINDOW_OUTERWIDTH,    JSPROP_ENUMERATE},
  {"outerHeight",    WINDOW_OUTERHEIGHT,    JSPROP_ENUMERATE},
  {"screenX",    WINDOW_SCREENX,    JSPROP_ENUMERATE},
  {"screenY",    WINDOW_SCREENY,    JSPROP_ENUMERATE},
  {"pageXOffset",    WINDOW_PAGEXOFFSET,    JSPROP_ENUMERATE},
  {"pageYOffset",    WINDOW_PAGEYOFFSET,    JSPROP_ENUMERATE},
  {0}
};


//
// Window class methods
//
static JSFunctionSpec WindowMethods[] = 
{
  {"dump",          WindowDump,     1},
  {"alert",          WindowAlert,     1},
  {"focus",          WindowFocus,     0},
  {"blur",          WindowBlur,     0},
  {"close",          WindowClose,     0},
  {"back",          WindowBack,     0},
  {"forward",          WindowForward,     0},
  {"home",          WindowHome,     0},
  {"stop",          WindowStop,     0},
  {"print",          WindowPrint,     0},
  {"moveTo",          WindowMoveTo,     2},
  {"moveBy",          WindowMoveBy,     2},
  {"resizeTo",          WindowResizeTo,     2},
  {"resizeBy",          WindowResizeBy,     2},
  {"scrollTo",          WindowScrollTo,     2},
  {"scrollBy",          WindowScrollBy,     2},
  {"clearTimeout",          WindowClearTimeout,     1},
  {"clearInterval",          WindowClearInterval,     1},
  {"setTimeout",          WindowSetTimeout,     0},
  {"setInterval",          WindowSetInterval,     0},
  {"open",          WindowOpen,     0},
  {"captureEvent",          EventCapturerCaptureEvent,     1},
  {"releaseEvent",          EventCapturerReleaseEvent,     1},
  {"addEventListener",          EventTargetAddEventListener,     4},
  {"removeEventListener",          EventTargetRemoveEventListener,     4},
  {0}
};


//
// Window constructor
//
PR_STATIC_CALLBACK(JSBool)
Window(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// Window class initialization
//
nsresult NS_InitWindowClass(nsIScriptContext *aContext, 
                        nsIScriptGlobalObject *aGlobal)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *global = JS_GetGlobalObject(jscontext);

  JS_DefineProperties(jscontext, global, WindowProperties);
  JS_DefineFunctions(jscontext, global, WindowMethods);

  return NS_OK;
}


//
// Method for creating a new Window JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptWindow(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null arg");
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();

  JSObject *global = ::JS_NewObject(jscontext, &WindowClass, NULL, NULL);
  if (global) {
    // The global object has a to be defined in two step:
    // 1- create a generic object, with no prototype and no parent which
    //    will be passed to JS_InitStandardClasses. JS_InitStandardClasses 
    //    will make it the global object
    // 2- define the global object to be what you really want it to be.
    //
    // The js runtime is not fully initialized before JS_InitStandardClasses
    // is called, so part of the global object initialization has to be moved 
    // after JS_InitStandardClasses

    // assign "this" to the js object, don't AddRef
    ::JS_SetPrivate(jscontext, global, aSupports);

    JS_DefineProperties(jscontext, global, WindowProperties);
    JS_DefineFunctions(jscontext, global, WindowMethods);

    *aReturn = (void*)global;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
