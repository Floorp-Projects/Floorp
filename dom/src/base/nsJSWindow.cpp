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
#include "nsCOMPtr.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMBarProp.h"
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
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIBarPropIID, NS_IDOMBARPROP_IID);
static NS_DEFINE_IID(kIScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kIHistoryIID, NS_IDOMHISTORY_IID);
static NS_DEFINE_IID(kIEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kIEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);

NS_DEF_PTR(nsIDOMNavigator);
NS_DEF_PTR(nsIDOMElement);
NS_DEF_PTR(nsIDOMDocument);
NS_DEF_PTR(nsIDOMBarProp);
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
  WINDOW_CONTENT = -9,
  WINDOW_MENUBAR = -10,
  WINDOW_TOOLBAR = -11,
  WINDOW_LOCATIONBAR = -12,
  WINDOW_PERSONALBAR = -13,
  WINDOW_STATUSBAR = -14,
  WINDOW_SCROLLBARS = -15,
  WINDOW_DIRECTORIES = -16,
  WINDOW_CLOSED = -17,
  WINDOW_FRAMES = -18,
  WINDOW_OPENER = -19,
  WINDOW_STATUS = -20,
  WINDOW_DEFAULTSTATUS = -21,
  WINDOW_NAME = -22,
  WINDOW_INNERWIDTH = -23,
  WINDOW_INNERHEIGHT = -24,
  WINDOW_OUTERWIDTH = -25,
  WINDOW_OUTERHEIGHT = -26,
  WINDOW_SCREENX = -27,
  WINDOW_SCREENY = -28,
  WINDOW_PAGEXOFFSET = -29,
  WINDOW_PAGEYOFFSET = -30
};

/***********************************************************************/
//
// Window Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_WINDOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.window", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetWindow(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.self", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetSelf(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.document", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMDocument* prop;
        if (NS_SUCCEEDED(a->GetDocument(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.navigator", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMNavigator* prop;
        if (NS_SUCCEEDED(a->GetNavigator(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.screen", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMScreen* prop;
        if (NS_SUCCEEDED(a->GetScreen(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.history", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMHistory* prop;
        if (NS_SUCCEEDED(a->GetHistory(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.parent", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetParent(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.top", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetTop(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_CONTENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.content", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetContent(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_MENUBAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.menubar", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetMenubar(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_TOOLBAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.toolbar", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetToolbar(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_LOCATIONBAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.locationbar", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetLocationbar(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PERSONALBAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.personalbar", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetPersonalbar(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_STATUSBAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.statusbar", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetStatusbar(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCROLLBARS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.scrollbars", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetScrollbars(&prop))) {
          // get the js object
          nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_DIRECTORIES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.directories", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMBarProp* prop;
        if (NS_SUCCEEDED(a->GetDirectories(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.closed", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRBool prop;
        if (NS_SUCCEEDED(a->GetClosed(&prop))) {
          *vp = BOOLEAN_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_FRAMES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.frames", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindowCollection* prop;
        if (NS_SUCCEEDED(a->GetFrames(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.opener", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsIDOMWindow* prop;
        if (NS_SUCCEEDED(a->GetOpener(&prop))) {
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.status", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetStatus(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.defaultstatus", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetDefaultStatus(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.name", PR_FALSE, &ok);
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
      case WINDOW_INNERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.innerwidth", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetInnerWidth(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.innerheight", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetInnerHeight(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.outerwidth", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetOuterWidth(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.outerheight", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetOuterHeight(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCREENX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.screenx", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetScreenX(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_SCREENY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.screeny", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetScreenY(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.pagexoffset", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetPageXOffset(&prop))) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.pageyoffset", PR_FALSE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        PRInt32 prop;
        if (NS_SUCCEEDED(a->GetPageYOffset(&prop))) {
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
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_OPENER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.opener", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
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
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.status", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetStatus(prop);
        
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.defaultstatus", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetDefaultStatus(prop);
        
        break;
      }
      case WINDOW_NAME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.name", PR_TRUE, &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetName(prop);
        
        break;
      }
      case WINDOW_INNERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.innerwidth", PR_TRUE, &ok);
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
      
        a->SetInnerWidth(prop);
        
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.innerheight", PR_TRUE, &ok);
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
      
        a->SetInnerHeight(prop);
        
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.outerwidth", PR_TRUE, &ok);
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
      
        a->SetOuterWidth(prop);
        
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.outerheight", PR_TRUE, &ok);
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
      
        a->SetOuterHeight(prop);
        
        break;
      }
      case WINDOW_SCREENX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.screenx", PR_TRUE, &ok);
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
      
        a->SetScreenX(prop);
        
        break;
      }
      case WINDOW_SCREENY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.screeny", PR_TRUE, &ok);
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
      
        a->SetScreenY(prop);
        
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.pagexoffset", PR_TRUE, &ok);
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
      
        a->SetPageXOffset(prop);
        
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "window.pageyoffset", PR_TRUE, &ok);
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
  nsJSUtils::nsGenericFinalize(cx, obj);
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
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.dump",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function dump requires 1 parameter");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->Dump(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Alert
//
PR_STATIC_CALLBACK(JSBool)
WindowAlert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.alert",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Alert(cx, argv+0, argc-0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Confirm
//
PR_STATIC_CALLBACK(JSBool)
WindowConfirm(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRBool nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.confirm",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Confirm(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method Prompt
//
PR_STATIC_CALLBACK(JSBool)
WindowPrompt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsAutoString nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.prompt",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Prompt(cx, argv+0, argc-0, nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
WindowFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.focus",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Focus()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
WindowBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.blur",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Blur()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Close
//
PR_STATIC_CALLBACK(JSBool)
WindowClose(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.close",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Close()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Back
//
PR_STATIC_CALLBACK(JSBool)
WindowBack(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.back",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Back()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Forward
//
PR_STATIC_CALLBACK(JSBool)
WindowForward(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.forward",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Forward()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Home
//
PR_STATIC_CALLBACK(JSBool)
WindowHome(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.home",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Home()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Stop
//
PR_STATIC_CALLBACK(JSBool)
WindowStop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.stop",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Stop()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Print
//
PR_STATIC_CALLBACK(JSBool)
WindowPrint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.print",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Print()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method MoveTo
//
PR_STATIC_CALLBACK(JSBool)
WindowMoveTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.moveto",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function moveTo requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method MoveBy
//
PR_STATIC_CALLBACK(JSBool)
WindowMoveBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.moveby",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function moveBy requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method ResizeTo
//
PR_STATIC_CALLBACK(JSBool)
WindowResizeTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.resizeto",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function resizeTo requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method ResizeBy
//
PR_STATIC_CALLBACK(JSBool)
WindowResizeBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.resizeby",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function resizeBy requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method SizeToContent
//
PR_STATIC_CALLBACK(JSBool)
WindowSizeToContent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.sizetocontent",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->SizeToContent()) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ScrollTo
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.scrollto",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function scrollTo requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method ScrollBy
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;
  PRInt32 b1;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.scrollby",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 2) {
      JS_ReportError(cx, "Function scrollBy requires 2 parameters");
      return JS_FALSE;
    }

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

  return JS_TRUE;
}


//
// Native method ClearTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowClearTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.cleartimeout",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function clearTimeout requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ClearTimeout(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ClearInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowClearInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.clearinterval",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function clearInterval requires 1 parameter");
      return JS_FALSE;
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->ClearInterval(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowSetTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.settimeout",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->SetTimeout(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method SetInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowSetInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  PRInt32 nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.setinterval",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->SetInterval(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method CreatePopup
//
PR_STATIC_CALLBACK(JSBool)
WindowCreatePopup(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMWindow* nativeRet;
  nsIDOMElementPtr b0;
  nsIDOMElementPtr b1;
  PRInt32 b2;
  PRInt32 b3;
  nsAutoString b4;
  nsAutoString b5;
  nsAutoString b6;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.createpopup",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 7) {
      JS_ReportError(cx, "Function createPopup requires 7 parameters");
      return JS_FALSE;
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b0,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[0])) {
      return JS_FALSE;
    }
    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&b1,
                                           kIElementIID,
                                           "Element",
                                           cx,
                                           argv[1])) {
      return JS_FALSE;
    }
    if (!JS_ValueToInt32(cx, argv[2], (int32 *)&b2)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }
    if (!JS_ValueToInt32(cx, argv[3], (int32 *)&b3)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }
    nsJSUtils::nsConvertJSValToString(b4, cx, argv[4]);
    nsJSUtils::nsConvertJSValToString(b5, cx, argv[5]);
    nsJSUtils::nsConvertJSValToString(b6, cx, argv[6]);

    if (NS_OK != nativeThis->CreatePopup(b0, b1, b2, b3, b4, b5, b6, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Open
//
PR_STATIC_CALLBACK(JSBool)
WindowOpen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMWindow* nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.open",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->Open(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method OpenDialog
//
PR_STATIC_CALLBACK(JSBool)
WindowOpenDialog(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMWindow* nativeRet;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "window.opendialog",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {

    if (NS_OK != nativeThis->OpenDialog(cx, argv+0, argc-0, &nativeRet)) {
      return JS_FALSE;
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method CaptureEvent
//
PR_STATIC_CALLBACK(JSBool)
EventCapturerCaptureEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMEventCapturer *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventCapturerIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventCapturer");
    return JS_FALSE;
  }

  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "eventcapturer.captureevent",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function captureEvent requires 1 parameter");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->CaptureEvent(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ReleaseEvent
//
PR_STATIC_CALLBACK(JSBool)
EventCapturerReleaseEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMEventCapturer *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventCapturerIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventCapturer");
    return JS_FALSE;
  }

  nsAutoString b0;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "eventcapturer.releaseevent",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 1) {
      JS_ReportError(cx, "Function releaseEvent requires 1 parameter");
      return JS_FALSE;
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    if (NS_OK != nativeThis->ReleaseEvent(b0)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method AddEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetAddEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "eventtarget.addeventlistener",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 3) {
      JS_ReportError(cx, "Function addEventListener requires 3 parameters");
      return JS_FALSE;
    }

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

    if (NS_OK != nativeThis->AddEventListener(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RemoveEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetRemoveEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsIDOMEventTarget *nativeThis = nsnull;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, (void **)&nativeThis)) {
    JS_ReportError(cx, "Object must be of type EventTarget");
    return JS_FALSE;
  }

  nsAutoString b0;
  nsIDOMEventListener* b1;
  PRBool b2;

  *rval = JSVAL_NULL;

  nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
    return JS_FALSE;
  }
  {
    PRBool ok;
    secMan->CheckScriptAccess(scriptCX, obj, "eventtarget.removeeventlistener",PR_FALSE , &ok);
    if (!ok) {
      //Need to throw error here
      return JS_FALSE;
    }
  }

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    if (argc < 3) {
      JS_ReportError(cx, "Function removeEventListener requires 3 parameters");
      return JS_FALSE;
    }

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

    if (NS_OK != nativeThis->RemoveEventListener(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Window
//
JSClass WindowClass = {
  "Window", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
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
  {"content",    WINDOW_CONTENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"menubar",    WINDOW_MENUBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"toolbar",    WINDOW_TOOLBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"locationbar",    WINDOW_LOCATIONBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"personalbar",    WINDOW_PERSONALBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"statusbar",    WINDOW_STATUSBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"scrollbars",    WINDOW_SCROLLBARS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"directories",    WINDOW_DIRECTORIES,    JSPROP_ENUMERATE | JSPROP_READONLY},
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
  {"alert",          WindowAlert,     0},
  {"confirm",          WindowConfirm,     0},
  {"prompt",          WindowPrompt,     0},
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
  {"sizeToContent",          WindowSizeToContent,     0},
  {"scrollTo",          WindowScrollTo,     2},
  {"scrollBy",          WindowScrollBy,     2},
  {"clearTimeout",          WindowClearTimeout,     1},
  {"clearInterval",          WindowClearInterval,     1},
  {"setTimeout",          WindowSetTimeout,     0},
  {"setInterval",          WindowSetInterval,     0},
  {"createPopup",          WindowCreatePopup,     7},
  {"open",          WindowOpen,     0},
  {"openDialog",          WindowOpenDialog,     0},
  {"captureEvent",          EventCapturerCaptureEvent,     1},
  {"releaseEvent",          EventCapturerReleaseEvent,     1},
  {"addEventListener",          EventTargetAddEventListener,     3},
  {"removeEventListener",          EventTargetRemoveEventListener,     3},
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

    // assign "this" to the js object
    ::JS_SetPrivate(jscontext, global, aSupports);
    NS_ADDREF(aSupports);

    JS_DefineProperties(jscontext, global, WindowProperties);
    JS_DefineFunctions(jscontext, global, WindowMethods);

    *aReturn = (void*)global;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
