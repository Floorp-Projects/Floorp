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
#include "nsIDOMNavigator.h"
#include "nsIPrompt.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSelection.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMScreen.h"
#include "nsIDOMHistory.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsISidebar.h"
#include "nsIDOMPkcs11.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMWindow.h"
#include "nsIControllers.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIPromptIID, NS_IPROMPT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDocumentViewIID, NS_IDOMDOCUMENTVIEW_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kISelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIBarPropIID, NS_IDOMBARPROP_IID);
static NS_DEFINE_IID(kIAbstractViewIID, NS_IDOMABSTRACTVIEW_IID);
static NS_DEFINE_IID(kIScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kIHistoryIID, NS_IDOMHISTORY_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kISidebarIID, NS_ISIDEBAR_IID);
static NS_DEFINE_IID(kIPkcs11IID, NS_IDOMPKCS11_IID);
static NS_DEFINE_IID(kIViewCSSIID, NS_IDOMVIEWCSS_IID);
static NS_DEFINE_IID(kICryptoIID, NS_IDOMCRYPTO_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIControllersIID, NS_ICONTROLLERS_IID);

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
  WINDOW__CONTENT = -9,
  WINDOW_SIDEBAR = -10,
  WINDOW_PROMPTER = -11,
  WINDOW_MENUBAR = -12,
  WINDOW_TOOLBAR = -13,
  WINDOW_LOCATIONBAR = -14,
  WINDOW_PERSONALBAR = -15,
  WINDOW_STATUSBAR = -16,
  WINDOW_SCROLLBARS = -17,
  WINDOW_DIRECTORIES = -18,
  WINDOW_CLOSED = -19,
  WINDOW_FRAMES = -20,
  WINDOW_CRYPTO = -21,
  WINDOW_PKCS11 = -22,
  WINDOW_CONTROLLERS = -23,
  WINDOW_OPENER = -24,
  WINDOW_STATUS = -25,
  WINDOW_DEFAULTSTATUS = -26,
  WINDOW_NAME = -27,
  WINDOW_INNERWIDTH = -28,
  WINDOW_INNERHEIGHT = -29,
  WINDOW_OUTERWIDTH = -30,
  WINDOW_OUTERHEIGHT = -31,
  WINDOW_SCREENX = -32,
  WINDOW_SCREENY = -33,
  WINDOW_PAGEXOFFSET = -34,
  WINDOW_PAGEYOFFSET = -35,
  WINDOW_SCROLLX = -36,
  WINDOW_SCROLLY = -37,
  WINDOW_LENGTH = -38,
  ABSTRACTVIEW_DOCUMENT = -39
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_WINDOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_WINDOW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->GetWindow(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_SELF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SELF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->GetSelf(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_DOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          rv = a->GetDocument(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_NAVIGATOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_NAVIGATOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNavigator* prop;
          rv = a->GetNavigator(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_SCREEN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCREEN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMScreen* prop;
          rv = a->GetScreen(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_HISTORY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_HISTORY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHistory* prop;
          rv = a->GetHistory(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_PARENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PARENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->GetParent(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_STATUSBAR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_STATUSBAR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          rv = a->GetStatusbar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_SCROLLBARS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBARS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          rv = a->GetScrollbars(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_DIRECTORIES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DIRECTORIES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          rv = a->GetDirectories(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_CLOSED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CLOSED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          rv = a->GetClosed(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_FRAMES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_FRAMES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowCollection* prop;
          rv = a->GetFrames(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_CRYPTO:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CRYPTO, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCrypto* prop;
          rv = a->GetCrypto(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_PKCS11:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PKCS11, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMPkcs11* prop;
          rv = a->GetPkcs11(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_OPENER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OPENER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->GetOpener(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_STATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_STATUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetStatus(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DEFAULTSTATUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDefaultStatus(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case WINDOW_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case WINDOW_INNERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_INNERWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetInnerWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_INNERHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetInnerHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OUTERWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOuterWidth(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OUTERHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetOuterHeight(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_SCREENX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCREENX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScreenX(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_SCREENY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCREENY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScreenY(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PAGEXOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetPageXOffset(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PAGEYOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetPageYOffset(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_SCROLLX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScrollX(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_SCROLLY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScrollY(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_LENGTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_LENGTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          rv = a->GetLength(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case ABSTRACTVIEW_DOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_ABSTRACTVIEW_DOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocumentView* prop;
          nsIDOMAbstractView* b;
          if (NS_OK == a->QueryInterface(kIAbstractViewIID, (void **)&b)) {
            rv = b->GetDocument(&prop);
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

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_OPENER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OPENER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIWindowIID, NS_ConvertASCIItoUCS2("Window"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          rv = a->SetOpener(prop);
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case WINDOW_STATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_STATUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetStatus(prop);
          
        }
        break;
      }
      case WINDOW_DEFAULTSTATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DEFAULTSTATUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDefaultStatus(prop);
          
        }
        break;
      }
      case WINDOW_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case WINDOW_INNERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_INNERWIDTH, PR_TRUE);
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
      
          rv = a->SetInnerWidth(prop);
          
        }
        break;
      }
      case WINDOW_INNERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_INNERHEIGHT, PR_TRUE);
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
      
          rv = a->SetInnerHeight(prop);
          
        }
        break;
      }
      case WINDOW_OUTERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OUTERWIDTH, PR_TRUE);
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
      
          rv = a->SetOuterWidth(prop);
          
        }
        break;
      }
      case WINDOW_OUTERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OUTERHEIGHT, PR_TRUE);
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
      
          rv = a->SetOuterHeight(prop);
          
        }
        break;
      }
      case WINDOW_SCREENX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCREENX, PR_TRUE);
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
      
          rv = a->SetScreenX(prop);
          
        }
        break;
      }
      case WINDOW_SCREENY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCREENY, PR_TRUE);
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
      
          rv = a->SetScreenY(prop);
          
        }
        break;
      }
      case WINDOW_PAGEXOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PAGEXOFFSET, PR_TRUE);
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
      
          rv = a->SetPageXOffset(prop);
          
        }
        break;
      }
      case WINDOW_PAGEYOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PAGEYOFFSET, PR_TRUE);
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
      
          rv = a->SetPageYOffset(prop);
          
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

/***********************************************************************/
//
// top Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowtopGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOP, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMWindow* prop;
          rv = a->GetTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// top Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowtopSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOP, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "top", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// _content Property Getter
//
PR_STATIC_CALLBACK(JSBool)
Window_contentGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW__CONTENT, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMWindow* prop;
          rv = a->Get_content(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// _content Property Setter
//
PR_STATIC_CALLBACK(JSBool)
Window_contentSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW__CONTENT, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "_content", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// sidebar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowsidebarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SIDEBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsISidebar* prop;
          rv = a->GetSidebar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsISidebar), cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// sidebar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowsidebarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SIDEBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "sidebar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// prompter Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowprompterGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PROMPTER, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIPrompt* prop;
          rv = a->GetPrompter(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIPrompt), cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// prompter Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowprompterSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PROMPTER, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "prompter", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// menubar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowmenubarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_MENUBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          rv = a->GetMenubar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// menubar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowmenubarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_MENUBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "menubar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// toolbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowtoolbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOOLBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          rv = a->GetToolbar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// toolbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowtoolbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOOLBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "toolbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// locationbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowlocationbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_LOCATIONBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          rv = a->GetLocationbar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// locationbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowlocationbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_LOCATIONBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "locationbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// personalbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowpersonalbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PERSONALBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          rv = a->GetPersonalbar(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// personalbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowpersonalbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PERSONALBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "personalbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// controllers Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowcontrollersGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CONTROLLERS, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIControllers* prop;
          rv = a->GetControllers(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIControllers), cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// controllers Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowcontrollersSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CONTROLLERS, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "controllers", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
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
  nsresult result = NS_OK;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DUMP, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Dump(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_ALERT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Alert(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRBool nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CONFIRM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Confirm(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  jsval nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PROMPT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Prompt(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_FOCUS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Focus();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_BLUR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Blur();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_BACK, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Back();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_FORWARD, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Forward();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_HOME, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Home();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_STOP, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Stop();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PRINT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Print();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_MOVETO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->MoveTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_MOVEBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->MoveBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_RESIZETO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ResizeTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_RESIZEBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ResizeBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SIZETOCONTENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SizeToContent();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetAttention
//
PR_STATIC_CALLBACK(JSBool)
WindowGetAttention(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_GETATTENTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetAttention();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Scroll
//
PR_STATIC_CALLBACK(JSBool)
WindowScroll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->Scroll(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLTO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CLEARTIMEOUT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ClearTimeout(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CLEARINTERVAL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ClearInterval(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SETTIMEOUT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SetTimeout(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsresult result = NS_OK;
  PRInt32 nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SETINTERVAL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SetInterval(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method CaptureEvents
//
PR_STATIC_CALLBACK(JSBool)
WindowCaptureEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CAPTUREEVENTS, PR_FALSE);
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
WindowReleaseEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_RELEASEEVENTS, PR_FALSE);
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
WindowRouteEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMEvent> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_ROUTEEVENT, PR_FALSE);
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


//
// Native method EnableExternalCapture
//
PR_STATIC_CALLBACK(JSBool)
WindowEnableExternalCapture(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_ENABLEEXTERNALCAPTURE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->EnableExternalCapture();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method DisableExternalCapture
//
PR_STATIC_CALLBACK(JSBool)
WindowDisableExternalCapture(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DISABLEEXTERNALCAPTURE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DisableExternalCapture();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetCursor
//
PR_STATIC_CALLBACK(JSBool)
WindowSetCursor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SETCURSOR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->SetCursor(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
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
  nsresult result = NS_OK;
  nsIDOMWindow* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OPEN, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Open(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
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
  nsresult result = NS_OK;
  nsIDOMWindow* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_OPENDIALOG, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->OpenDialog(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_CLOSE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Close(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method UpdateCommands
//
PR_STATIC_CALLBACK(JSBool)
WindowUpdateCommands(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_UPDATECOMMANDS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->UpdateCommands(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Escape
//
PR_STATIC_CALLBACK(JSBool)
WindowEscape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_ESCAPE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Escape(b0, nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Unescape
//
PR_STATIC_CALLBACK(JSBool)
WindowUnescape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsAutoString nativeRet;
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_UNESCAPE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Unescape(b0, nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method GetSelection
//
PR_STATIC_CALLBACK(JSBool)
WindowGetSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsIDOMSelection* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_GETSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetSelection(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
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
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  nsCOMPtr<nsIDOMEventListener> b1;
  PRBool b2;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_ADDEVENTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToFunc(getter_AddRefs(b1),
                                         cx,
                                         obj,
                                         argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_FUNCTION_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->AddEventListener(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
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
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  nsCOMPtr<nsIDOMEventListener> b1;
  PRBool b2;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_REMOVEEVENTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToFunc(getter_AddRefs(b1),
                                         cx,
                                         obj,
                                         argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_FUNCTION_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->RemoveEventListener(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method DispatchEvent
//
PR_STATIC_CALLBACK(JSBool)
EventTargetDispatchEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
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
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_DISPATCHEVENT, PR_FALSE);
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

    result = nativeThis->DispatchEvent(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetComputedStyle
//
PR_STATIC_CALLBACK(JSBool)
ViewCSSGetComputedStyle(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMViewCSS> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIViewCSSIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsIDOMCSSStyleDeclaration* nativeRet;
  nsCOMPtr<nsIDOMElement> b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_VIEWCSS_GETCOMPUTEDSTYLE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->GetComputedStyle(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
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
  FinalizeWindow,
  nsnull,
  nsJSUtils::nsCheckAccess
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
  {"top",    WINDOW_TOP,    JSPROP_ENUMERATE, WindowtopGetter, WindowtopSetter},
  {"_content",    WINDOW__CONTENT,    JSPROP_ENUMERATE, Window_contentGetter, Window_contentSetter},
  {"sidebar",    WINDOW_SIDEBAR,    JSPROP_ENUMERATE, WindowsidebarGetter, WindowsidebarSetter},
  {"prompter",    WINDOW_PROMPTER,    JSPROP_ENUMERATE, WindowprompterGetter, WindowprompterSetter},
  {"menubar",    WINDOW_MENUBAR,    JSPROP_ENUMERATE, WindowmenubarGetter, WindowmenubarSetter},
  {"toolbar",    WINDOW_TOOLBAR,    JSPROP_ENUMERATE, WindowtoolbarGetter, WindowtoolbarSetter},
  {"locationbar",    WINDOW_LOCATIONBAR,    JSPROP_ENUMERATE, WindowlocationbarGetter, WindowlocationbarSetter},
  {"personalbar",    WINDOW_PERSONALBAR,    JSPROP_ENUMERATE, WindowpersonalbarGetter, WindowpersonalbarSetter},
  {"statusbar",    WINDOW_STATUSBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"scrollbars",    WINDOW_SCROLLBARS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"directories",    WINDOW_DIRECTORIES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"closed",    WINDOW_CLOSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"frames",    WINDOW_FRAMES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"crypto",    WINDOW_CRYPTO,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pkcs11",    WINDOW_PKCS11,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"controllers",    WINDOW_CONTROLLERS,    JSPROP_ENUMERATE, WindowcontrollersGetter, WindowcontrollersSetter},
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
  {"scrollX",    WINDOW_SCROLLX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"scrollY",    WINDOW_SCROLLY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"length",    WINDOW_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"document",    ABSTRACTVIEW_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
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
  {"GetAttention",          WindowGetAttention,     0},
  {"scroll",          WindowScroll,     2},
  {"scrollTo",          WindowScrollTo,     2},
  {"scrollBy",          WindowScrollBy,     2},
  {"clearTimeout",          WindowClearTimeout,     1},
  {"clearInterval",          WindowClearInterval,     1},
  {"setTimeout",          WindowSetTimeout,     0},
  {"setInterval",          WindowSetInterval,     0},
  {"captureEvents",          WindowCaptureEvents,     1},
  {"releaseEvents",          WindowReleaseEvents,     1},
  {"routeEvent",          WindowRouteEvent,     1},
  {"enableExternalCapture",          WindowEnableExternalCapture,     0},
  {"disableExternalCapture",          WindowDisableExternalCapture,     0},
  {"setCursor",          WindowSetCursor,     1},
  {"open",          WindowOpen,     0},
  {"openDialog",          WindowOpenDialog,     0},
  {"close",          WindowClose,     0},
  {"updateCommands",          WindowUpdateCommands,     1},
  {"escape",          WindowEscape,     1},
  {"unescape",          WindowUnescape,     1},
  {"getSelection",          WindowGetSelection,     0},
  {"addEventListener",          EventTargetAddEventListener,     3},
  {"removeEventListener",          EventTargetRemoveEventListener,     3},
  {"dispatchEvent",          EventTargetDispatchEvent,     1},
  {"getComputedStyle",          ViewCSSGetComputedStyle,     2},
  {0}
};


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
